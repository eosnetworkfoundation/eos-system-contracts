#include <eosio.wrap/eosio.wrap.hpp>

namespace eosio {

void wrap::exec( ignore<name>, ignore<transaction> ) {
   require_auth( get_self() );

   name executer;
   _ds >> executer;

   require_auth( executer );

   //send_deferred( (uint128_t(executer.value) << 64) | (uint64_t)current_time_point().time_since_epoch().count(), executer, _ds.pos(), _ds.remaining() );
   transaction_header trx_header;
   std::vector<action> context_free_actions;
   std::vector<action> actions;
   _ds >> trx_header;
   //check( trx_header.expiration >= eosio::time_point_sec(current_time_point()), "transaction expired" );
   _ds >> context_free_actions;
   check( context_free_actions.empty(), "not allowed to `exec` a transaction with context-free actions" );
   _ds >> actions;

   for (const auto& act : actions) {
      act.send();
   }
}

} /// namespace eosio
