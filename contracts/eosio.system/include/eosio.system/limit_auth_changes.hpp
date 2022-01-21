#pragma once

#include <eosio/multi_index.hpp>

namespace eosiosystem {
   using eosio::name;

   struct [[eosio::table("limitauthchg"),eosio::contract("eosio.system")]] limit_auth_change {
      uint8_t              version = 0;
      name                 account;
      std::vector<name>    allow_perms;
      std::vector<name>    disallow_perms;

      uint64_t primary_key() const { return account.value; }

      EOSLIB_SERIALIZE(limit_auth_change, (version)(account)(allow_perms)(disallow_perms))
   };

   typedef eosio::multi_index<"limitauthchg"_n, limit_auth_change> limit_auth_change_table;
} // namespace eosiosystem
