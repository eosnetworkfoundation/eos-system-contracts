#include <eosio.fees/eosio.fees.hpp>

namespace eosio {

void fees::on_transfer( const name from, const name to, const asset quantity, const string memo )
{
   if ( to != get_self() ) {
      return;
   }
   if (eosiosystem::system_contract::rex_available()) {
      eosiosystem::system_contract::donatetorex_action donatetorex( "eosio"_n, { get_self(), "active"_n });
      donatetorex.send(get_self(), quantity, memo);
   }
}

void fees::noop()
{
   require_auth( get_self() );
}

} /// namespace eosio
