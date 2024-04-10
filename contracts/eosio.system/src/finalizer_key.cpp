#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>

namespace eosiosystem {

   // Returns true if nodeos has transitioned to Savanna (last finalizer set not empty)
   bool system_contract::is_savanna_consensus() const {
      return !_gstate_a.last_proposed_keys.empty();
   }

   // Rteurns hash of finalizer_key
   static eosio::checksum256 get_finalizer_key_hash(const std::string& finalizer_key) {
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      return eosio::sha256(fin_key_g1.data(), fin_key_g1.size());
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

      std::vector< proposed_finalizer_key_t > proposed_fin_keys;
      proposed_fin_keys.reserve(_gstate.last_producer_schedule_size);

      // Find a set of producers that meet all the normal requirements for
      // being a proposer and also have an active finalizer key.
      // The number of the producers must be equal to the number of producers
      // in the last_producer_schedule.
      auto idx = _producers.get_index<"prototalvote"_n>();
      for( auto it = idx.cbegin(); it != idx.cend() && proposed_fin_keys.size() < _gstate.last_producer_schedule_size && 0 < it->total_votes && it->active(); ++it ) {
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

         proposed_fin_keys.emplace_back( proposed_finalizer_key_t {
            .key_id        = finalizer->active_finalizer_key_id,
            .fin_authority = eosio::finalizer_authority{
               .description = finalizer->finalizer_name.to_string(),
               .weight      = 1,
               .public_key  = finalizer->active_finalizer_key_binary
            }}
         );
      }

      check( proposed_fin_keys.size() == _gstate.last_producer_schedule_size,
            "not enough top producers have registered finalizer keys, has " + std::to_string(proposed_fin_keys.size()) + ", require " + std::to_string(_gstate.last_producer_schedule_size) );

      set_proposed_finalizers_unsorted(proposed_fin_keys);
      check( is_savanna_consensus(), "switching to Savanna failed" );
   }

   // This function may never fail, as it can be called by onblock indirectly.
   // Establish finalizer policy from `proposed_fin_keys` and calls
   // eosio::set_finalizers host function
   void system_contract::set_proposed_finalizers( const std::vector<proposed_finalizer_key_t>& proposed_fin_keys ) {
      // Construct finalizer authorities
      std::vector<eosio::finalizer_authority> finalizer_authorities;
      for( const auto& k: proposed_fin_keys ) {
         finalizer_authorities.emplace_back(k.fin_authority);
      }

      // Establish finalizer policy
      eosio::finalizer_policy fin_policy {
         .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
         .finalizers = std::move(finalizer_authorities)
      };

      // call host function
      eosio::set_finalizers(std::move(fin_policy)); // call host function
   }

   // Sort `proposed_fin_keys` and calls set_proposed_finalizers()
   void system_contract::set_proposed_finalizers_unsorted( std::vector<proposed_finalizer_key_t>& proposed_fin_keys ) {
      std::sort( proposed_fin_keys.begin(), proposed_fin_keys.end(), []( const proposed_finalizer_key_t& lhs, const proposed_finalizer_key_t& rhs ) {
         return lhs.key_id < rhs.key_id;
      } );

      set_proposed_finalizers(proposed_fin_keys);

      // store last proposed policy
      _gstate_a.last_proposed_keys = proposed_fin_keys;
   }

   // This function may never fail, as it can be called by update_elected_producers,
   // and in turn by onblock
   // Sort `proposed_fin_keys`. If it is the same as last proposed finalizer
   // policy, do not proceed. Otherwise, call set_proposed_finalizers()
   void system_contract::set_proposed_finalizers_if_changed( std::vector<proposed_finalizer_key_t>& proposed_fin_keys ) {
      std::sort( proposed_fin_keys.begin(), proposed_fin_keys.end(), []( const proposed_finalizer_key_t& lhs, const proposed_finalizer_key_t& rhs ) {
         return lhs.key_id < rhs.key_id;
      } );

      if( proposed_fin_keys.size() == _gstate_a.last_proposed_keys.size() ) {
         bool new_fin_policy = false;
         for( auto i = 0; i < proposed_fin_keys.size(); ++i ) {
            if( proposed_fin_keys[i].key_id != _gstate_a.last_proposed_keys[i].key_id ||
                proposed_fin_keys[i].fin_authority.public_key != _gstate_a.last_proposed_keys[i].fin_authority.public_key ) {
               new_fin_policy = true;
               break;
            }
         }

         // Finalizer policy has not changed. Do not proceed.
         if( !new_fin_policy ) {
            return;
         }
      }

      set_proposed_finalizers(proposed_fin_keys);

      // store last proposed policy
      _gstate_a.last_proposed_keys = proposed_fin_keys;
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

      // Basic key and signature format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key does not start with PUB_BLS: " + finalizer_key);
      check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession signature does not start with SIG_BLS: " + proof_of_possession);

      // Convert to binary form. The validity will be checked during conversion.
      const auto fin_key_g1 = eosio::decode_bls_public_key_to_g1(finalizer_key);
      const auto pop_g2 = eosio::decode_bls_signature_to_g2(proof_of_possession);

      // Duplication check across all registered keys
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = eosio::sha256(fin_key_g1.data(), fin_key_g1.size());
      check(idx.find(hash) == idx.end(), "duplicate finalizer key: " + finalizer_key);

      // Proof of possession check
      check(eosio::bls_pop_verify(fin_key_g1, pop_g2), "proof of possession check failed");

      // Insert the finalizer key into finalyzer_keys table
      auto finalizer_key_itr = _finalizer_keys.emplace( finalizer_name, [&]( auto& k ) {
         k.id                   = _gstate5.get_next_finalizer_key_id();
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
            f.finalizer_name              = finalizer_name;
            f.active_finalizer_key_id     = finalizer_key_itr->id;
            f.active_finalizer_key_binary = finalizer_key_itr->finalizer_key_binary;
            f.finalizer_key_count         = 1;
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

      // Check finalizer has registered keys
      auto finalizer = _finalizers.find(finalizer_name.value);
      check( finalizer != _finalizers.end(), "finalizer has not registered any finalizer keys" );

      // Basic format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key does not start with PUB_BLS: " + finalizer_key);

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      auto finalizer_key_itr = idx.find(hash);
      check(finalizer_key_itr != idx.end(), "finalizer key was not registered: " + finalizer_key);

      // Check the key belongs to finalizer
      check(finalizer_key_itr->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer: " + finalizer_key);

      // Check if the finalizer key is not already active
      check( finalizer_key_itr->id != finalizer->active_finalizer_key_id, "finalizer key was already active: " + finalizer_key );

      auto prev_fin_key_id = finalizer->active_finalizer_key_id;

      _finalizers.modify( finalizer, same_payer, [&]( auto& f ) {
         f.active_finalizer_key_id      = finalizer_key_itr->id;
         f.active_finalizer_key_binary  = finalizer_key_itr->finalizer_key_binary;
      });

      // Do a binary search to see if the finalizer is in last proposed policy
      auto itr = std::lower_bound(_gstate_a.last_proposed_keys.begin(), _gstate_a.last_proposed_keys.end(), prev_fin_key_id, [](const proposed_finalizer_key_t& key, uint64_t id) {
         return key.key_id < id;
      });

      if( itr != _gstate_a.last_proposed_keys.end() && itr->key_id == prev_fin_key_id ) {
         // Replace the previous finalizer key with finalizer key just activated
         itr->fin_authority.public_key = finalizer_key_itr->finalizer_key_binary;

         // Call set_finalizers immediately
         set_proposed_finalizers(_gstate_a.last_proposed_keys);
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

      // Check finalizer has registered keys
      auto finalizer = _finalizers.find(finalizer_name.value);
      check( finalizer != _finalizers.end(), "finalizer " + finalizer_name.to_string() + " has not registered any finalizer keys" );
      check( finalizer->finalizer_key_count > 0, "finalizer " + finalizer_name.to_string() + "  must have at least one registered finalizer keys, has " + std::to_string(finalizer->finalizer_key_count) );

      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key does not start with PUB_BLS: " + finalizer_key);

      // Check the key is registered
      auto idx = _finalizer_keys.get_index<"byfinkey"_n>();
      auto hash = get_finalizer_key_hash(finalizer_key);
      auto fin_key_itr = idx.find(hash);
      check(fin_key_itr != idx.end(), "finalizer key was not registered: " + finalizer_key);

      // Check the key belongs to the finalizer
      check(fin_key_itr->finalizer_name == name(finalizer_name), "finalizer key " + finalizer_key + " was not registered by the finalizer " + finalizer_name.to_string() );
      
      if( fin_key_itr->id == finalizer->active_finalizer_key_id ) {
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
