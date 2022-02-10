#include <eosio.system/rex.results.hpp>

namespace rex_results {

void contract::buyresult( const asset& rex_received ) { }

void contract::sellresult( const asset& proceeds ) { }

void contract::orderresult( const name& owner, const asset& proceeds ) { }

void contract::rentresult( const asset& rented_tokens ) { }

}

EOSIO_ABIGEN(actions(rex_results::actions))
