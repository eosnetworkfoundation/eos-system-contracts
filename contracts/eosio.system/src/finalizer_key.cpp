#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>

namespace eosiosystem {

   // Common routines
   bool system_contract::is_savanna_consensus() const {
      return _last_fin_keys.begin() != _last_fin_keys.end();
   }

   static eosio::checksum256 get_finalizer_key_hash(const std::string& finalizer_key) {
      return eosio::sha256(finalizer_key.data(), finalizer_key.size());
   }

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
      uint32_t i = 0;
      auto idx = _producers.get_index<"prototalvote"_n>();
      for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active() && i < 30; ++it, ++i ) {
         auto finalizer = _finalizers.find( it->owner.value );
         if( finalizer == _finalizers.end() ) {
            // The producer is not in finalizers table, indicating it does not have an
            // active registered finalizer key. Try next one.
            continue;
         }

         // This should never happen. Double check the finalizer has an active key just in case
         if( finalizer->active_finalizer_key_binary.empty() ) {
            continue;
         }

         // builds up producer_authority
         top_producers.emplace_back(
            eosio::producer_authority{
               .producer_name = it->owner,
               .authority     = it->get_producer_authority()
            },
            it->location
         );

         // stores finalizer_key_id
         new_finalizer_key_ids.emplace_back(finalizer->active_finalizer_key_id);
         // builds up finalizer_authorities
         finalizer_authorities.emplace_back(
            eosio::finalizer_authority{
               .description = it->owner.to_string(),
               .weight      = 1,
               .public_key  = finalizer->active_finalizer_key_binary
            }
         );
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

      // Should set_finalizers be called the first before set_proposed_producers
      // during transition?
      set_finalizers(std::move(finalizer_authorities), new_finalizer_key_ids, {});

      if( set_proposed_producers( producers ) >= 0 ) {
         _gstate.last_producer_schedule_update = eosio::current_time_point();
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( producers.size() );
      }
   }

   // This function may never fail, as it can be called by update_elected_producers,
   // and in turn by onblock
   void system_contract::set_finalizers( std::vector<eosio::finalizer_authority>&& finalizer_authorities, const std::vector<uint64_t>& new_ids, const std::unordered_set<uint64_t>& kept_ids ) {
      // Establish finalizer policy
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
         .finalizers = std::move(finalizer_authorities)
      };
      // call host function
      eosio::set_finalizers(std::move(fin_policy)); // call host function

      // Purge any IDs not in kept_ids from _last_fin_keys table
      for (auto itr = _last_fin_keys.begin(); itr != _last_fin_keys.end(); /* intentionally empty */ ) {
         if( !kept_ids.contains(itr->id) ) {
            itr = _last_fin_keys.erase(itr);
         } else {
            ++itr;
         }
      }

      // Add IDs in new_ids to _last_fin_keys table
      for (auto id: new_ids ) {
         _last_fin_keys.emplace( get_self(), [&]( auto& f ) {
            f.id = id;
         });
      }
   }

   // Action to register a finalizer key
   void system_contract::regfinkey( const name& finalizer_name, const std::string& finalizer_key, const std::string& proof_of_possession) {
      require_auth( finalizer_name );

      auto producer = _producers.find( finalizer_name.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Basic key and signature format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with PUB_BLS");
      check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession signature must start with SIG_BLS");

      // Duplication check across all registered keys
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      check(idx.find(hash) == idx.end(), "duplicate finalizer key");

      // Convert to binary form. The validity will be checked during conversion.
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      const auto pop_g2 = eosio::decode_bls_signature_to_g2(proof_of_possession);

      // Proof of possession check is expensive, do it last
      check(eosio::bls_pop_verify(fin_key_g1, pop_g2), "proof of possession check failed");

      // Insert the finalizer key into finalyzer_keys table
      auto fin_key_itr = _finalizer_keys.emplace( finalizer_name, [&]( auto& k ) {
         k.id                   = _finalizer_keys.available_primary_key();
         k.finalizer_name       = finalizer_name;
         k.finalizer_key        = finalizer_key;
         k.finalizer_key_binary = { fin_key_g1.begin(), fin_key_g1.end() };
         k.finalizer_key_hash   = get_finalizer_key_hash(finalizer_key);
      });

      // Update finalizers table
      auto fin_itr = _finalizers.find(finalizer_name.value);
      if( fin_itr == _finalizers.end() ) {
         // First time the finalizer to register a finalier key,
         // make the key active
         _finalizers.emplace( finalizer_name, [&]( auto& f ) {
            f.finalizer_name              = finalizer_name;
            f.active_finalizer_key_id     = fin_key_itr->id;
            f.active_finalizer_key_binary = { fin_key_g1.begin(), fin_key_g1.end() };
            f.finalizer_key_count         = 1;
         });
      } else {
         // Update finalizer_key_count
         _finalizers.modify( fin_itr, same_payer, [&]( auto& f ) {
            assert(f.finalizer_name == finalizer_name);
            assert(f.finalizer_key_count > 0);
            ++f.finalizer_key_count;
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
      check( finalizer->finalizer_key_count > 0, "finalizer_key_count of the finalizer must be greater than one" );

      // Basic format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with  PUB_BLS");

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      auto fin_key_itr = idx.find(hash);
      check(fin_key_itr != idx.end(), "finalizer key was not registered");

      // Check the key belongs to finalizer
      check(fin_key_itr->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer");

      // Check if the finalizer key is not already active
      check( fin_key_itr->id != finalizer->active_finalizer_key_id, "the finalizer key was already active" );

      auto old_active_finalizer_key_id = finalizer->active_finalizer_key_id;

      _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
         f.active_finalizer_key_id      = fin_key_itr->id;
         f.active_finalizer_key_binary  = fin_key_itr->finalizer_key_binary;
      });

      // Replace the finalizer policy immediately if the finalizer key is
      // used in last finalizer policy
      const auto old_key_itr = _last_fin_keys.find(old_active_finalizer_key_id);
      if( old_key_itr != _last_fin_keys.end() ) {
         // Delete old key
         _last_fin_keys.erase(old_key_itr);

         // Replace the old key with finalizer key just activated
         _last_fin_keys.emplace( get_self(), [&]( auto& f ) {
            f.id = fin_key_itr->id;  // new active finalizer key id
         });

         generate_finalizer_policy_and_set_finalizers();
      }
   }

   // Generate a new finalizer policy from keys in _last_fin_keys table and
   // call set_finalizers host function immediately
   void system_contract::generate_finalizer_policy_and_set_finalizers() {
      std::vector<eosio::finalizer_authority> finalizer_authorities;

      // Build a new finalizer_authority by going over all keys in _last_fin_keys table
      for (auto itr = _last_fin_keys.begin(); itr != _last_fin_keys.end(); ++itr) {
         auto fin_key_itr = _finalizer_keys.find(itr->id);
         assert( fin_key_itr != _finalizer_keys.end() );
         finalizer_authorities.emplace_back(
            eosio::finalizer_authority{
               .description = fin_key_itr->finalizer_name.to_string(),
               .weight      = 1,
               .public_key  = fin_key_itr->finalizer_key_binary
            }
         );
      }

      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
         .finalizers = std::move(finalizer_authorities)
      };

      // _last_fin_keys table is up to date. Call set_finalizers host function directly
      eosio::set_finalizers(std::move(fin_policy));
   }

   // Action to delete a registered finalizer key
   void system_contract::delfinkey( const name& finalizer_name, const std::string& finalizer_key ) {
      require_auth( finalizer_name );

      auto producer = _producers.find( finalizer_name.value );
      check( producer != _producers.end(), "finalizer is not a registered producer");

      // Check finalizer has registered keys
      auto finalizer = _finalizers.find(finalizer_name.value);
      check( finalizer != _finalizers.end(), "finalizer has not registered any finalizer keys" );
      assert( finalizer->finalizer_key_count > 0 );

      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with PUB_BLS");

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      auto fin_key_itr = idx.find(hash);
      check(fin_key_itr != idx.end(), "finalizer key was not registered");

      // Check the key belongs to the finalizer
      check(fin_key_itr->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer");
      
      // If the key is currently active, it cannot be deleted unless it is the
      // only key of the finalizer.
      if( fin_key_itr->id == finalizer->active_finalizer_key_id ) {
         check( finalizer->finalizer_key_count == 1, "cannot delete an active key unless it is the last registered finalizer key");
      }

      // Remove the key from finalizer_keys table
      idx.erase( fin_key_itr );

      // Update finalizers table
      if( finalizer->finalizer_key_count == 1 ) {
         // The finalizer does not have any registered keys. Remove it from finalizers table.
         _finalizers.erase( finalizer );
      } else {
         // Decrement finalizer_key_count finalizers table
         _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
            --f.finalizer_key_count;
         });
      }
   }
} /// namespace eosiosystem
