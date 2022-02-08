#include <eosio.wrap/eosio.wrap.hpp>

namespace eosio_wrap {

void contract::exec( ignore<name>, ignore<transaction> ) {
   require_auth( get_self() );

   name executer;
   _ds >> executer;

   require_auth( executer );

   send_deferred( (uint128_t(executer.value) << 64) | (uint64_t)current_time_point().time_since_epoch().count(), executer, _ds.pos(), _ds.remaining() );
}

} // namespace eosio_wrap

EOSIO_ACTION_DISPATCHER(eosio_wrap::actions)
EOSIO_ABIGEN(actions(eosio_wrap::actions))
