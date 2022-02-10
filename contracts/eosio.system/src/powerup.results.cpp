#include <eosio.system/powerup.results.hpp>

namespace powup_results {

void contract::powupresult( const eosio::asset& fee, const int64_t powup_net_weight, const int64_t powup_cpu_weight ) { }

}

EOSIO_ABIGEN(actions(powup_results::actions))
