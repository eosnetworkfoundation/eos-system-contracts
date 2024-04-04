#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>
#include <eosio/privileged.hpp>

namespace eosiosystem {

   // Action to switch to Savanna
   void system_contract::switchtosvnn() {
      require_auth(get_self());

      check(!is_savanna_consensus(), "switchtosvnn can be run only once");

      using value_type = std::pair<eosio::producer_authority, uint16_t>;
      std::vector<value_type> top_producers;
      std::vector<eosio::finalizer_authority> finalizer_authorities;
      std::vector<uint64_t> new_finalizer_key_ids;
      top_producers.reserve(21);
      finalizer_authorities.reserve(21);
      new_finalizer_key_ids.reserve(21);

      // From up to 30 top producers, find 21 producers that meet all the normal requirements
      // for being a proposer and also have an active finalizer key
      uint32_t n = 0;
      auto idx = _producers.get_index<"prototalvote"_n>();
      for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
         if( n > 30 ) {
            break;
         }
         ++n;

         auto finalizer = _finalizers.find( it->owner.value );
         // If a producer is found in finalizers_table, it means it has at least one finalizer
         // key registered, and one of them must be active
         if( finalizer != _finalizers.end() ) {
            assert( !finalizer->active_key.empty() );

            // builds up producer_authority
            top_producers.emplace_back(
               eosio::producer_authority{
                  .producer_name = it->owner,
                  .authority     = it->get_producer_authority()
               },
               it->location
            );

            // builds up finalizer_authorities
            new_finalizer_key_ids.emplace_back(finalizer->active_key_id);
            finalizer_authorities.emplace_back(
               eosio::finalizer_authority{
                  .description = it->owner.to_string(),
                  .weight      = 1,
                  .public_key  = finalizer->active_key
               }
            );
         }
      }

      check( top_producers.size() > 0 && top_producers.size() >= _gstate.last_producer_schedule_size, "not enough top producers have registered finalizer keys" );

      std::sort( top_producers.begin(), top_producers.end(), []( const value_type& lhs, const value_type& rhs ) {
         return lhs.first.producer_name < rhs.first.producer_name; // sort by producer name
         // return lhs.second < rhs.second; // sort by location
      } );

      std::vector<eosio::producer_authority> producers;

      producers.reserve(top_producers.size());
      for( auto& item : top_producers )
         producers.push_back( std::move(item.first) );

      if( set_proposed_producers( producers ) >= 0 ) {
         _gstate.last_producer_schedule_update = eosio::current_time_point();
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( producers.size() );
      }

      set_finalizers(std::move(finalizer_authorities), new_finalizer_key_ids, {});
   }

   bool system_contract::is_savanna_consensus() const {
      return _last_finkey_ids.begin() != _last_finkey_ids.end();
   }

   void system_contract::set_finalizers( std::vector<eosio::finalizer_authority>&& finalizer_authorities, const std::vector<uint64_t>& new_key_ids, const std::unordered_set<uint64_t>& kept_key_ids ) {
      // Establish finalizer policy and call set_finalizers() host function
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1, // or hardcoded to 15?
         .finalizers = finalizer_authorities
      };
      eosio::set_finalizers(std::move(fin_policy)); // call host function

      // Purge any ids not in kept_key_ids from _last_finkey_ids
      for (auto itr = _last_finkey_ids.begin(); itr != _last_finkey_ids.end(); /* intentionally empty */ ) {
         if( kept_key_ids.contains(itr->key_id) ) {
            ++itr;
         } else {
            itr = _last_finkey_ids.erase(itr);
         }
      }

      // Add new_key_ids to _last_finkey_ids
      for (auto id: new_key_ids ) {
         _last_finkey_ids.emplace( get_self(), [&]( auto& f ) {
            f.key_id = id;
         });
      }
   }

   // Action to register a finalizer key
   void system_contract::regfinkey( const name& finalizer, const std::string& finalizer_key, const std::string& proof_of_possession) {
      require_auth( finalizer );

      auto producer = _producers.find( finalizer.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Basic key and signature format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with PUB_BLS");
      check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession siganture must start with SIG_BLS");

      // Convert to binary form
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      const auto signature = eosio::decode_bls_signature_to_g2(proof_of_possession);

      // Duplication check across all registered keys
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(finalizer_key.data(), finalizer_key.size());
      check(idx.find(hash) == idx.end(), "duplicate finalizer key");

      // Proof of possession check is expensive, do it at last
      check(eosio::bls_pop_verify(fin_key_g1, signature), "proof of possession check failed");

      // Insert into finalyzer_keys table
      auto key_itr = _finalizer_keys.emplace( finalizer, [&]( auto& k ) {
         k.id                 = _finalizer_keys.available_primary_key();
         k.finalizer_name     = finalizer;
         k.finalizer_key      = finalizer_key;
         k.finalizer_key_hash = eosio::sha256(finalizer_key.data(), finalizer_key.size());
      });

      // Update finalizers table
      auto f_itr = _finalizers.find(finalizer.value);
      if( f_itr == _finalizers.end() ) {
         // First time the finalizer to register a finalier key,
         // make the key active
         _finalizers.emplace( finalizer, [&]( auto& f ) {
            f.finalizer_name      = finalizer;
            f.active_key_id       = key_itr->id;
            f.active_key          = { fin_key_g1.begin(), fin_key_g1.end() };
            f.num_registered_keys = 1;
         });
      } else {
         // Update num_registered_keys
         _finalizers.modify( f_itr, same_payer, [&]( auto& f ) {
            ++f.num_registered_keys;
         });
      }
   }

   // Action to activate a finalizer key
   void system_contract::actfinkey( const name& finalizer_name, const std::string& finalizer_key ) {
      require_auth( finalizer_name );

      auto producer = _producers.find( finalizer_name.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Check finalizer has registered keys
      auto finalizer = _finalizers.find(finalizer_name.value);
      check( finalizer != _finalizers.end(), "finalizer has not registered any finalizer keys" );
      check( finalizer->num_registered_keys > 0, "num_registered_keys of the finalizer must be greater than one" );

      // Basic format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with  PUB_BLS");

      // Check the key is registered
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(finalizer_key.data(), finalizer_key.size());
      auto fin_key = idx.find(hash);
      check(fin_key != idx.end(), "finalizer key was not registered");

      // Check the key belongs to finalizer
      check(fin_key->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer");

      // Check if the finalizer key is not already active
      check( fin_key->id != finalizer->active_key_id, "the finalizer key was already active" );

      auto old_active_key_id = finalizer->active_key_id;

      _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
         f.active_key_id = fin_key->id;
         f.active_key    = { fin_key_g1.begin(), fin_key_g1.end() };
      });

      // Replace the finalizer policy immediately if the finalizer is
      // participating in current voting
      if( _last_finkey_ids.find(old_active_key_id) != _last_finkey_ids.end() ) {
         replace_key_in_finalizer_policy(finalizer_name, old_active_key_id, fin_key->id);
      }
   }

   // replace the key in last finalizer policy and call set_finalizers host function immediately
   void system_contract::replace_key_in_finalizer_policy(const name& finalizer, uint64_t old_id, uint64_t new_id) {
      // replace key ID in last_finkey_ids_table
      auto id_itr = _last_finkey_ids.find(old_id);
      _last_finkey_ids.modify( id_itr, get_self(), [&]( auto& f ) {
          f.key_id = new_id;
      });

      std::vector<eosio::finalizer_authority> finalizer_authorities;

      for (auto itr = _last_finkey_ids.begin(); itr != _last_finkey_ids.end(); ++itr) {
         auto key = _finalizer_keys.find(itr->key_id);
         assert( key != _finalizer_keys.end() );
         const auto pk = eosio::decode_bls_public_key_to_g1(key->finalizer_key);
         finalizer_authorities.emplace_back(
            eosio::finalizer_authority{
               .description = key->finalizer_name.to_string(),
               .weight      = 1,
               .public_key  = { pk.begin(), pk.end() }
            }
         );
      }

      // last_finkey_ids table has already been updated. Call set_finalizers host function directly
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
         .finalizers = finalizer_authorities
      };
      eosio::set_finalizers(std::move(fin_policy)); // call host function
   }

   // Action to delete a registered finalizer key
   void system_contract::delfinkey( const name& finalizer_name, const std::string& finalizer_key ) {
      require_auth( finalizer_name );

      auto producer = _producers.find( finalizer_name.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Check finalizer has registered keys
      auto finalizer = _finalizers.find(finalizer_name.value);
      check( finalizer != _finalizers.end(), "finalizer has not registered any finalizer keys" );
      assert( finalizer->num_registered_keys > 0 );

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(finalizer_key.data(), finalizer_key.size());
      auto fin_key = idx.find(hash);
      check(fin_key != idx.end(), "finalizer key was not registered");

      // Check the key belongs to finalizer
      check(fin_key->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer");
      
      if( fin_key->id == finalizer->active_key_id ) {
         check( finalizer->num_registered_keys == 1, "cannot delete an active key unless it is the last registered finalizer key");
      }

      // Remove the key from finalizer_keys table
      idx.erase( fin_key );

      // Update finalizers table
      if( finalizer->num_registered_keys == 1 ) {
         // The finalizer does not have any registered keys. Remove it from finalizers table.
         _finalizers.erase( finalizer );
      } else {
         // Decrement num_registered_keys finalizers table
         _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
            --f.num_registered_keys;
         });
      }
   }
} /// namespace eosiosystem
