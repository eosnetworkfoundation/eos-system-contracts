#pragma once

#include <eosio/contract.hpp>
#include <eosio/crypto.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>
#include <eosio/privileged.hpp>
#include <eosio/time.hpp>

#include <string>
#include <optional>

namespace eosiosystem {

using eosio::block_timestamp;
using eosio::name;
using eosio::public_key;

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
struct [[eosio::table("peerkeys"), eosio::contract("eosio.system")]] peer_key {
   struct v0_data {
      std::optional<public_key> pubkey; // peer key for network message authentication
      EOSLIB_SERIALIZE(v0_data, (pubkey))
   };

   name                  proposer_finalizer_name;
   block_timestamp       block_time; // timestamp of block where this row was emplaced or modified
   std::variant<v0_data> data;

   uint64_t primary_key() const { return proposer_finalizer_name.value; }
   uint64_t by_block_time() const { return block_time.slot; }

   void                                    set_public_key(const public_key& key) { data = v0_data{key}; }
   const std::optional<eosio::public_key>& get_public_key() const {
      return std::visit([](auto& v) -> const std::optional<eosio::public_key>& { return v.pubkey; }, data);
   }
   void update_row() { block_time = eosio::current_block_time(); }
   void init_row(name n) { *this = peer_key{n, eosio::current_block_time(), v0_data{}}; }
};

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
typedef eosio::multi_index<"peerkeys"_n, peer_key,
                           indexed_by<"byblocktime"_n, const_mem_fun<peer_key, uint64_t, &peer_key::by_block_time>>>
   peer_keys_table;

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
struct [[eosio::contract("eosio.system")]] peer_keys : public eosio::contract {
   peer_keys(name s, name code, eosio::datastream<const char*> ds)
      : eosio::contract(s, code, ds) {}

   struct peerkeys_t {
      name                      producer_name;
      std::optional<public_key> peer_key;

      EOSLIB_SERIALIZE(peerkeys_t, (producer_name)(peer_key))
   };

   using getpeerkeys_response = std::vector<peerkeys_t>;

   /**
    * Action to register a public key for a proposer or finalizer name.
    * This key will be used to validate a network peer's identity.
    * A proposer or finalizer can only have have one public key registered at a time.
    * If a key is already registered for `proposer_finalizer_name`, and `regpeerkey` is
    * called with a different key, the new key replaces the previous one in `peer_keys_table`
    */
   [[eosio::action]]
   void regpeerkey(const name& proposer_finalizer_name, const public_key& key);

   /**
    * Action to delete a public key for a proposer or finalizer name.
    *
    * The intent of this action is only for the account to reclaim the RAM, as
    * the node software may remember the key after it was deleted using `delpeerkey`.
    *
    * An existing public key for a given account can be changed by calling `regpeerkey` again.
    */
   [[eosio::action]]
   void delpeerkey(const name& proposer_finalizer_name, const public_key& key);

   /**
    * Returns a list of paid producers (in rank order) along with their peer public key if it was
    * added via the regpeerkey action.
    */
   [[eosio::action]]
   getpeerkeys_response getpeerkeys();

};

} // namespace eosiosystem