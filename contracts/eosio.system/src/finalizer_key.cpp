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
      std::vector<name> top_producer_names;
      std::vector<eosio::finalizer_authority> finalizer_authorities;
      std::set<uint64_t> next_finalizer_key_ids;
      top_producers.reserve(21);
      top_producer_names.reserve(21);
      finalizer_authorities.reserve(21);

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
            check( !finalizer->active_key.empty(), "Active finalizer key of a finalizer in finalizers_table cannot be empty" );

            // builds up producer_authority
            top_producers.emplace_back(
               eosio::producer_authority{
                  .producer_name = it->owner,
                  .authority     = it->get_producer_authority()
               },
               it->location
            );
            top_producer_names.emplace_back(it->owner);

            // builds up finalizer_authorities
            next_finalizer_key_ids.insert(finalizer->active_key_id);
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
         // what block_time to use here?
         //_gstate.last_producer_schedule_update = block_time;
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }

      set_finalizers(finalizer_authorities, next_finalizer_key_ids);
   }

   bool system_contract::is_savanna_consensus() const {
      return !_gstate5.last_finalizer_key_ids.empty();
   }

   void system_contract::set_finalizers( const std::vector<eosio::finalizer_authority>& finalizer_authorities, const std::set<uint64_t>& finalizer_key_ids ) {
      // Establish finalizer policy and call set_finalizers() host function
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1, // or hardcoded to 15?
         .finalizers = finalizer_authorities
      };
      eosio::set_finalizers(std::move(fin_policy)); // call host function

      _gstate5.last_finalizer_key_ids = finalizer_key_ids;
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
      auto hash = eosio::sha256(fin_key_g1.data(), fin_key_g1.size());
      check(idx.lower_bound(hash) == idx.end(), "duplicate finalizer key");

      // Proof of possession check is expensive, do it at last
      check(eosio::bls_pop_verify(fin_key_g1, signature), "proof of possession check failed");

      // Insert into finalyzer_keys table
      auto key_itr = _finalizer_keys.emplace( finalizer, [&]( auto& k ) {
         k.id            = _finalizer_keys.available_primary_key();
         k.finalizer     = finalizer;
         k.finalizer_key = finalizer_key;
      });

      // Update finalizers table
      auto f_itr = _finalizers.find(finalizer.value);
      if( f_itr == _finalizers.end() ) {
         // First time the finalizer to register a finalier key,
         // make the key active
         _finalizers.emplace( finalizer, [&]( auto& f ) {
            f.finalizer           = finalizer;
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
   void system_contract::actfinkey( const name& finalizer, const std::string& finalizer_key ) {
      require_auth( finalizer );

      auto producer = _producers.find( finalizer.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Check the key is registered
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(fin_key_g1.data(), fin_key_g1.size());
      auto fin_key = idx.lower_bound(hash);
      check(fin_key != idx.end(), "finalizer key was not registered");

      // Check the key belongs to finalizer
      check(fin_key->finalizer == finalizer, "finalizer_key was not registered by the finalizer");

      // Check finalizer itself
      auto fin = _finalizers.find(finalizer.value);
      check( fin != _finalizers.end(), "finalizer is not in the finalizers table" );
      check( fin->num_registered_keys > 0, "num_registered_keys of the finalizer must be greater than one" );

      // Check if the finalizer key is not already active
      check( fin_key->id != fin->active_key_id, "the finalizer key was already active" );

      auto old_active_key_id = fin->active_key_id;

      _finalizers.modify( fin, same_payer, [&]( auto& f ) {
         f.active_key_id = fin_key->id;
         f.active_key    = { fin_key_g1.begin(), fin_key_g1.end() };
      });

      // Replace the finalizer policy immediately if the finalizer is
      // participating in current voting
      if( _gstate5.last_finalizer_key_ids.contains(old_active_key_id) ) {
         replace_key_in_finalizer_policy(finalizer, old_active_key_id, fin_key->id);
      }
   }

   // replace the key in last finalizer policy and call set_finalizers host function immediately
   void system_contract::replace_key_in_finalizer_policy(const name& finalizer, uint64_t old_id, uint64_t new_id) {
      std::set<uint64_t> next_finalizer_key_ids {_gstate5.last_finalizer_key_ids};
      std::vector<eosio::finalizer_authority> finalizer_authorities;
      finalizer_authorities.reserve(next_finalizer_key_ids.size());

      // replace key ID in last_finalizer_key_ids
      next_finalizer_key_ids.erase(old_id);
      next_finalizer_key_ids.insert(new_id);

      for( const auto id : next_finalizer_key_ids ) {
         auto key = _finalizer_keys.find(id);
         check( key != _finalizer_keys.end(), "key not found in finalizer_keys table for replace_key_in_finalizer_policy");
         const auto pk = eosio::decode_bls_public_key_to_g1(key->finalizer_key);
         finalizer_authorities.emplace_back(
            eosio::finalizer_authority{
               .description = key->finalizer.to_string(),
               .weight      = 1,
               .public_key  = { pk.begin(), pk.end() }
            }
         );
      }

      set_finalizers(finalizer_authorities, next_finalizer_key_ids);
   }

   // Action to delete a registered finalizer key
   void system_contract::delfinkey( const name& finalizer, const std::string& finalizer_key ) {
      require_auth( finalizer );

      auto producer = _producers.find( finalizer.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Check the key is registered
      const auto pk = eosio::decode_bls_public_key_to_g1(finalizer_key);
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(pk.data(), pk.size());
      auto fin_key = idx.lower_bound(hash);
      check(fin_key != idx.end(), "finalizer_key was not registered");

      // Check the key belongs to finalizer
      check(fin_key->finalizer == finalizer, "finalizer_key was not registered by the finalizer");
      
      // Cross check finalizers table
      auto fin = _finalizers.find(finalizer.value);
      check( fin != _finalizers.end(), "finalizer is not in the finalizers table" );
      check( fin->num_registered_keys > 0, "num_registered_keys of the finalizer must be greater than one" );
      
      if( fin_key->id == fin->active_key_id ) {
         check( fin->num_registered_keys == 1, "cannot delete an active key unless it is the last registered finalizer key");
      }

      // Remove the key from finalizer_keys table
      idx.erase( fin_key );

      // Update finalizers table
      if( fin->num_registered_keys == 1 ) {
         // The finalizer does not have any registered keys. Remove it from finalizers table.
         _finalizers.erase( fin );
      } else {
         // Decrement num_registered_keys finalizers table
         _finalizers.modify( fin, same_payer, [&]( auto& f ) {
            --f.num_registered_keys;
         });
      }
   }
} /// namespace eosiosystem
