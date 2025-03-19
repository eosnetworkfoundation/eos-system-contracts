#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>

namespace eosiosystem {
   finalizer_auth_info::finalizer_auth_info(const finalizer_info& finalizer)
      : key_id(finalizer.active_key_id)
      , fin_authority( eosio::finalizer_authority{
         .description = finalizer.finalizer_name.to_string(),
         .weight      = 1,
         .public_key  = finalizer.active_key_binary })
   {
   }

   // Returns true if nodeos has transitioned to Savanna (having last proposed finalizers)
   bool system_contract::is_savanna_consensus() {
      return !get_last_proposed_finalizers().empty();
   }

   // Validates finalizer_key in text form and returns a binary form
   eosio::bls_g1 to_binary(const std::string& finalizer_key) {
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key does not start with PUB_BLS: " + finalizer_key);
      return eosio::decode_bls_public_key_to_g1(finalizer_key);
   }

   // Returns hash of finalizer_key in binary format
   static eosio::checksum256 get_finalizer_key_hash(const eosio::bls_g1& finalizer_key_binary) {
      return eosio::sha256(finalizer_key_binary.data(), finalizer_key_binary.size());
   }

   // Returns hash of finalizer_key in text format
   static eosio::checksum256 get_finalizer_key_hash(const std::string& finalizer_key) {
      const auto fin_key_g1 = to_binary(finalizer_key);
      return get_finalizer_key_hash(fin_key_g1);
   }

   // Validates finalizer and returns the iterator to finalizers table
   finalizers_table::const_iterator system_contract::get_finalizer_itr( const name& finalizer_name ) const {
      // Check finalizer has registered keys
      auto finalizer_itr = _finalizers.find(finalizer_name.value);
      check( finalizer_itr != _finalizers.end(), "finalizer " + finalizer_name.to_string() + " has not registered any finalizer keys" );
      check( finalizer_itr->finalizer_key_count > 0, "finalizer " + finalizer_name.to_string() + "  must have at least one registered finalizer keys, has " + std::to_string(finalizer_itr->finalizer_key_count) );

      return finalizer_itr;
   }

   // If finalizers have changed since last round, establishes finalizer policy
   // from `proposed_finalizers` and calls eosio::set_finalizers host function
   // Note: this function may never fail, as it can be called by update_elected_producers,
   // and in turn by onblock.
   void system_contract::set_proposed_finalizers( std::vector<finalizer_auth_info> proposed_finalizers ) {
      // Sort proposed_finalizers by finalizer key ID
      std::sort( proposed_finalizers.begin(), proposed_finalizers.end(), []( const finalizer_auth_info& lhs, const finalizer_auth_info& rhs ) {
         return lhs.key_id < rhs.key_id;
      } );

      // Compare with last_proposed_finalizers to see if finalizers have changed.
      const auto& last_proposed_finalizers = get_last_proposed_finalizers();
      if( proposed_finalizers == last_proposed_finalizers ) {
         // Finalizer policy has not changed. Do not proceed.
         return;
      }

      // Construct finalizer authorities
      std::vector<eosio::finalizer_authority> finalizer_authorities;
      finalizer_authorities.reserve(proposed_finalizers.size());
      for( const auto& k: proposed_finalizers ) {
         finalizer_authorities.emplace_back(k.fin_authority);
      }

      // Establish new finalizer policy
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
         .finalizers = std::move(finalizer_authorities)
      };

      // Call host function
      eosio::set_finalizers(std::move(fin_policy)); // call host function

      // Store last proposed policy in both cache and DB table
      auto itr = _last_prop_finalizers.begin();
      if( itr == _last_prop_finalizers.end() ) {
         _last_prop_finalizers.emplace( get_self(), [&]( auto& f ) {
            f.last_proposed_finalizers = proposed_finalizers;
         });
      } else {
         _last_prop_finalizers.modify(itr, same_payer, [&]( auto& f ) {
            f.last_proposed_finalizers = proposed_finalizers;
         });
      }
      // Ensure not invalidate anyone holding the references to the vector
      // that was returned earlier by get_last_proposed_finalizer
      if (_last_prop_finalizers_cached.has_value()) {
         std::swap(*_last_prop_finalizers_cached, proposed_finalizers);
      } else {
         _last_prop_finalizers_cached.emplace(std::move(proposed_finalizers));
      }
   }

   // Returns last proposed finalizers
   const std::vector<finalizer_auth_info>& system_contract::get_last_proposed_finalizers() {
      if( !_last_prop_finalizers_cached.has_value() ) {
         const auto finalizers_itr = _last_prop_finalizers.begin();
         if( finalizers_itr == _last_prop_finalizers.end() ) {
            _last_prop_finalizers_cached = {};
         } else {
            _last_prop_finalizers_cached = finalizers_itr->last_proposed_finalizers;
         }
      }

      return *_last_prop_finalizers_cached;
   }

   // Generates an ID for a new finalizer key to be used in finalizer_keys table.
   // It may never be reused.
   uint64_t system_contract::get_next_finalizer_key_id() {
      uint64_t next_id = 0;
      auto itr = _fin_key_id_generator.begin();

      if( itr == _fin_key_id_generator.end() ) {
         _fin_key_id_generator.emplace( get_self(), [&]( auto& f ) {
            f.next_finalizer_key_id = next_id;
         });
      } else {
         next_id = itr->next_finalizer_key_id  + 1;
         _fin_key_id_generator.modify(itr, same_payer, [&]( auto& f ) {
            f.next_finalizer_key_id = next_id;
         });
      }

      return next_id;
   }

   /**
   * Action to switch to Savanna
   *
   * @pre  Require the authority of the contract itself
   * @pre  A sufficient numner of the top 21 block producers have registered a finalizer key
   * @post is_savanna_consensus returns true
   */
   void system_contract::switchtosvnn() {
      require_auth(get_self());

      check(!is_savanna_consensus(), "switchtosvnn can be run only once");

      std::vector< finalizer_auth_info > proposed_finalizers;
      proposed_finalizers.reserve(_gstate.last_producer_schedule_size);

      // Find a set of producers that meet all the normal requirements for
      // being a proposer and also have an active finalizer key.
      // The number of the producers must be equal to the number of producers
      // in the last_producer_schedule.
      auto idx = _producers.get_index<"prototalvote"_n>();
      for( auto it = idx.cbegin(); it != idx.cend() && proposed_finalizers.size() < _gstate.last_producer_schedule_size && 0 < it->total_votes && it->active(); ++it ) {
         auto finalizer = _finalizers.find( it->owner.value );
         if( finalizer == _finalizers.end() ) {
            // The producer is not in finalizers table, indicating it does not have an
            // active registered finalizer key. Try next one.
            continue;
         }

         // This should never happen. Double check the finalizer has an active key just in case
         if( finalizer->active_key_binary.empty() ) {
            continue;
         }

         proposed_finalizers.emplace_back(*finalizer);
      }

      check( proposed_finalizers.size() == _gstate.last_producer_schedule_size,
            "not enough top producers have registered finalizer keys, has " + std::to_string(proposed_finalizers.size()) + ", require " + std::to_string(_gstate.last_producer_schedule_size) );

      set_proposed_finalizers(std::move(proposed_finalizers));
      check( is_savanna_consensus(), "switching to Savanna failed" );
   }

   /*
    * Action to register a finalizer key
    *
    * @pre `finalizer_name` must be a registered producer
    * @pre `finalizer_key` must be in base64url format
    * @pre `proof_of_possession` must be a valid of proof of possession signature
    * @pre Authority of `finalizer_name` to register. `linkauth` may be used to allow a lower authrity to exectute this action.
    */
   void system_contract::regfinkey( const name& finalizer_name, const std::string& finalizer_key, const std::string& proof_of_possession) {
      require_auth( finalizer_name );

      auto producer = _producers.find( finalizer_name.value );
      check( producer != _producers.end(), "finalizer " + finalizer_name.to_string() + " is not a registered producer");

      // Basic signature format check
      check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession signature does not start with SIG_BLS: " + proof_of_possession);

      // Convert to binary form. The validity will be checked during conversion.
      const auto fin_key_g1 = to_binary(finalizer_key);
      const auto pop_g2 = eosio::decode_bls_signature_to_g2(proof_of_possession);

      // Duplication check across all registered keys
      const auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      const auto hash = get_finalizer_key_hash(fin_key_g1);
      check(idx.find(hash) == idx.end(), "duplicate finalizer key: " + finalizer_key);

      // Proof of possession check
      check(eosio::bls_pop_verify(fin_key_g1, pop_g2), "proof of possession check failed");

      // Insert the finalizer key into finalyzer_keys table
      const auto finalizer_key_itr = _finalizer_keys.emplace( finalizer_name, [&]( auto& k ) {
         k.id                   = get_next_finalizer_key_id();
         k.finalizer_name       = finalizer_name;
         k.finalizer_key        = finalizer_key;
         k.finalizer_key_binary = { fin_key_g1.begin(), fin_key_g1.end() };
      });

      // Update finalizers table
      auto finalizer = _finalizers.find(finalizer_name.value);
      if( finalizer == _finalizers.end() ) {
         // This is the first time the finalizer registering a finalizer key,
         // mark the key active
         _finalizers.emplace( finalizer_name, [&]( auto& f ) {
            f.finalizer_name       = finalizer_name;
            f.active_key_id        = finalizer_key_itr->id;
            f.active_key_binary    = finalizer_key_itr->finalizer_key_binary;
            f.finalizer_key_count  = 1;
         });
      } else {
         // Update finalizer_key_count
         _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
            ++f.finalizer_key_count;
         });
      }
   }

   /*
    * Action to activate a finalizer key
    *
    * @pre `finalizer_key` must be a registered finalizer key in base64url format
    * @pre Authority of `finalizer_name`
    */
   void system_contract::actfinkey( const name& finalizer_name, const std::string& finalizer_key ) {
      require_auth( finalizer_name );

      const auto finalizer = get_finalizer_itr(finalizer_name);

      // Check the key is registered
      const auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      const auto hash = get_finalizer_key_hash(finalizer_key);
      const auto finalizer_key_itr = idx.find(hash);
      check(finalizer_key_itr != idx.end(), "finalizer key was not registered: " + finalizer_key);

      // Check the key belongs to finalizer
      check(finalizer_key_itr->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer: " + finalizer_key);

      // Check if the finalizer key is not already active
      check( !finalizer_key_itr->is_active(finalizer->active_key_id), "finalizer key was already active: " + finalizer_key );

      const auto active_key_id = finalizer->active_key_id;

      // Mark the finalizer key as active by updating finalizer's information in finalizers table
      _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
         f.active_key_id      = finalizer_key_itr->id;
         f.active_key_binary  = finalizer_key_itr->finalizer_key_binary;
      });

      const auto& last_proposed_finalizers = get_last_proposed_finalizers();
      if( last_proposed_finalizers.empty() ) {
         // prior to switching to Savanna
         return;
      }

      // Search last_proposed_finalizers for active_key_id
      auto itr = std::lower_bound(last_proposed_finalizers.begin(), last_proposed_finalizers.end(), active_key_id, [](const finalizer_auth_info& key, uint64_t id) {
         return key.key_id < id;
      });

      // If active_key_id is in last_proposed_finalizers, it means the finalizer is
      // active. Replace the existing entry in last_proposed_finalizers with
      // the information of finalizer_key just activated and call set_proposed_finalizers immediately
      if( itr != last_proposed_finalizers.end() && itr->key_id == active_key_id ) {
         auto proposed_finalizers = last_proposed_finalizers;
         auto& matching_entry = proposed_finalizers[itr - last_proposed_finalizers.begin()];

         matching_entry.key_id = finalizer_key_itr->id;
         matching_entry.fin_authority.public_key = finalizer_key_itr->finalizer_key_binary;

         set_proposed_finalizers(std::move(proposed_finalizers));
      }
   }

   /*
    * Action to delete a registered finalizer key
    *
    * @pre `finalizer_key` must be a registered finalizer key in base64url format
    * @pre `finalizer_key` must not be active, unless it is the last registered finalizer key
    * @pre Authority of `finalizer_name`
    * */
   void system_contract::delfinkey( const name& finalizer_name, const std::string& finalizer_key ) {
      require_auth( finalizer_name );

      auto finalizer = get_finalizer_itr(finalizer_name);

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      auto fin_key_itr = idx.find(hash);
      check(fin_key_itr != idx.end(), "finalizer key was not registered: " + finalizer_key);

      // Check the key belongs to the finalizer
      check(fin_key_itr->finalizer_name == name(finalizer_name), "finalizer key " + finalizer_key + " was not registered by the finalizer " + finalizer_name.to_string() );
      
      if( fin_key_itr->is_active(finalizer->active_key_id) ) {
         check( finalizer->finalizer_key_count == 1, "cannot delete an active key unless it is the last registered finalizer key, has " + std::to_string(finalizer->finalizer_key_count) + " keys");
      }

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

      // Remove the key from finalizer_keys table
      idx.erase( fin_key_itr );
   }
} /// namespace eosiosystem
