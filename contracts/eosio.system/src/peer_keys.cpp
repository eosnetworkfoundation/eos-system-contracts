#include <eosio.system/eosio.system.hpp>

#include <eosio/eosio.hpp>

namespace eosiosystem {

   void system_contract::regpeerkey( const name& proposer_finalizer_name, const public_key& key ) {
      require_auth(proposer_finalizer_name);
      check(!std::holds_alternative<eosio::webauthn_public_key>(key), "webauthn keys not allowed in regpeerkey action");

      auto peers_itr = _peer_keys.find(proposer_finalizer_name.value);
      if (peers_itr == _peer_keys.end()) {
         _peer_keys.emplace(proposer_finalizer_name, [&](auto& row) {
            row.init_row(proposer_finalizer_name);
            row.set_public_key(key);
         });
      } else {
         const auto& prev_key = peers_itr->get_public_key();
         check(!prev_key || *prev_key != key, "Provided key is the same as currently stored one");
         _peer_keys.modify(peers_itr, same_payer, [&](auto& row) {
            row.update_row();
            row.set_public_key(key);
         });
      }
   }

   void system_contract::delpeerkey( const name& proposer_finalizer_name, const public_key& key ) {
      require_auth(proposer_finalizer_name);

      // not updating the version here. deleted keys will persist in the memory hashmap
      auto peers_itr = _peer_keys.find(proposer_finalizer_name.value);
      check(peers_itr != _peer_keys.end(), "Key not present for name: " + proposer_finalizer_name.to_string());
      const auto& prev_key = peers_itr->get_public_key();
      check(prev_key &&  *prev_key == key, "Current key does not match the provided one");
      _peer_keys.erase(peers_itr);
   }

} /// namespace eosiosystem
