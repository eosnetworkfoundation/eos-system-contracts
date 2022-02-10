#pragma once

#include <eosio.system/eosio.system.hpp>

namespace rex_results {

using eosio::asset;
using eosio::name;

/**
 * The actions `buyresult`, `sellresult`, `rentresult`, and `orderresult` of `rex.results` are all no-ops.
 * They are added as inline convenience actions to `rentnet`, `rentcpu`, `buyrex`, `unstaketorex`, and `sellrex`.
 * An inline convenience action does not have any effect, however,
 * its data includes the result of the parent action and appears in its trace.
 */
class contract : eosio::contract {
   public:

      using eosio::contract::contract;

      /**
       * Buyresult action.
       *
       * @param rex_received - amount of tokens used in buy order
       */
      void buyresult( const asset& rex_received );

      /**
       * Sellresult action.
       *
       * @param proceeds - amount of tokens used in sell order
       */
      void sellresult( const asset& proceeds );

      /**
       * Orderresult action.
       *
       * @param owner - the owner of the order
       * @param proceeds - amount of tokens used in order
       */
      void orderresult( const name& owner, const asset& proceeds );

      /**
       * Rentresult action.
       *
       * @param rented_tokens - amount of rented tokens
       */
      void rentresult( const asset& rented_tokens );
};

EOSIO_ACTIONS(contract,
              eosio_system::system_contract::rex_account,
              action(buyresult, rex_received),
              action(sellresult, proceeds),
              action(orderresult, owner, proceeds),
              action(rentresult, rented_tokens))

} // namespace rex_results
