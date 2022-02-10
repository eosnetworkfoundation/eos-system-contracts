#pragma once

#include <eosio.system/eosio.system.hpp>

namespace powup_results {

/**
 * The action `powerresult` of `power.results` is a no-op.
 * It is added as an inline convenience action to `powerup` reservation.
 * This inline convenience action does not have any effect, however,
 * its data includes the result of the parent action and appears in its trace.
 */
class contract : eosio::contract {
   public:

      using eosio::contract::contract;

      /**
       * powupresult action.
       *
       * @param fee       - powerup fee amount
       * @param powup_net - amount of powup NET tokens
       * @param powup_cpu - amount of powup CPU tokens
       */
      void powupresult( const eosio::asset& fee, const int64_t powup_net, const int64_t powup_cpu );
};

EOSIO_ACTIONS(contract,
              eosio_system::system_contract::reserv_account,
              action(powupresult, fee, powup_net, powup_cpu))

} // namespace powup_results
