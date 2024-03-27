#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>
#include <eosio/privileged.hpp>
#include <eosio/crypto_bls_ext.hpp>

namespace eosiosystem {

   bool system_contract::is_savanna_consensus() {
      return !_gstate5.last_finalizers.empty();
   }

   bool system_contract::is_registered_producer( const name& producer ) {
      // Returns true if `producer` is a registered producer
   }

   bool system_contract::is_active_producer( const name& producer ) {
      // Returns true if `producer` is an active producer (top 21)
   }

   bool set_next_finalizers_set(const std::vector<name>& producers ) {
      // Build finalizer set
      std::vector<finalizer_authority> finalizers;
      for ( const auto p&: producers ) {
         auto it = _finalizer_keys.find( p.value );
         if( it != _finalizer_keys.end() ) { // p has registered a finalizer key
            finalizers.emplace_back(
               finalizer_authority{
                  .description = "",
                  .weight      = 1,
                  .public_key  = it->active_key
               }
            );
         }
      }

      // Abort if less than 15 top producers have registered a finalizer key
      if( finalizers.size < 15 ) {
         return false;
      }

      // Establish finalizer policy and call set_finalizers() host function
      finalizer_policy fin_policy {
         .generation = 0;
         .threshold  = 15;
         .finalizers = finalizers;
      };
      set_finalizers(std::move(fin_policy));

      _gstate5.last_finalizers = finalizers;

      return true;
   }

   void replace_key_in_finalizer_policy(const name& producer, const std::string& finalizer_key) {
      // replace the key in last finalizer policy and call set_finalizers
      // host function immediately
   }

   void delete_key_in_finalizer_policy(const name& producer, const std::string& finalizer_key) {
      // replace the key in last finalizer policy and call set_finalizers
      // host function immediately
   }

   void system_contract::switchtosvnn() {
      require_auth(get_self());

      check(!is_savanna_consensus(), "switchtosvnn can only be run once");

      // Find top 21 block producers
      auto idx = _producers.get_index<"prototalvote"_n>();
      std::vector<name> top_producers;
      top_producers.reserve(21);
      for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
         top_producers.emplace_back(it->owner);
      }
      if( top_producers.size() < 21 ) {
         return;
      }

      set_next_finalizers_set(top_producers);
   }

   void system_contract::regfinkey( const name& producer, const std::string& finalizer_key, const std::string& proof_of_possession) {
      require_auth( producer );

      check( is_registered_producer(producer), "The producer is not a registered producer");

      // Basic key and signature format check
      check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key must start with PUB_BLS");
      check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession siganture must start with SIG_BLS");

      // Proof of possession check
      const auto pk = eosio::decode_bls_public_key_to_g1(finalizer_key);
      const auto signature = eosio::decode_bls_signature_to_g2(proof_of_possession);
      check(eosio::bls_pop_verify(pk, signature), "proof of possession check failed");

      // Todo: Go over finalizer_keys_table to make sure no duplicate keys are
      // registered.

      auto it = _finalizer_keys.find( producer.value );
      if( it != _finalizer_keys.end() ) {
         _finalizer_keys.modify( it, same_payer, [&]( auto& k ) {
            k.registered_finalizer_keys.insert(finalizer_key);
         });
      } else {
         // The first time the producer registers a finalizer key
         _finalizer_keys.emplace( producer, [&]( auto& p ) {
            k.producer = producer;
            k.registered_finalizer_keys.insert(finalizer_key);
            k.active_key = finalizer_key;
         });

         if( is_active_producer(producer) ) {
            replace_key_in_finalizer_policy(producer, finalizer_key);
         }
      }
   }

   void system_contract::actfinkey( const name& producer, const std::string& finalizer_key ) {
      require_auth( producer );

      check( is_registered_producer(producer), "The producer is not a registered producer");

      auto it = _finalizer_keys.find( producer.value );
      check(it != _finalizer_keys.end(), "producer has not registered any keys");
      
      check(it->registered_finalizer_keys.find(finalizer_key) != it->registered_finalizer_keys.end(), "finalizer_key was not registered");
      
      _finalizer_keys.modify( it, same_payer, [&]( auto& k ) {
         k.active_finalizer_key = finalizer_key;
      });

      if( is_active_producer(producer) ) {
         replace_key_in_finalizer_policy(producer, finalizer_key);
      }
   }

   void system_contract::delfinkey( const name& producer, const std::string& finalizer_key ) {
      require_auth( producer );

      check( is_registered_producer(producer), "The producer is not a registered producer");

      auto it = _finalizer_keys.find( producer.value );
      check(it != _finalizer_keys.end(), "producer has not registered any keys");
      
      check(it->registered_finalizer_keys.find(finalizer_key) != it->registered_finalizer_keys.end(), "finalizer_key was not registered");
      
      _finalizer_keys.erase( it );

      if( is_active_producer(producer) ) {
         delete_key_in_finalizer_policy(producer);
      }

      // should we allow to delete a key if that makes the number of producers
      // having registered keys drop below 15?
   }
} /// namespace eosiosystem
