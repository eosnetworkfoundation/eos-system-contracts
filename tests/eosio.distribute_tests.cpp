#include <boost/test/unit_test.hpp>
#include <eosio/chain/exceptions.hpp>


#include "eosio.system_tester.hpp"

using namespace eosio_system;

struct distribute_account {
    name        account;
    uint16_t    percent;
};
FC_REFLECT(distribute_account, (account)(percent))

class eosio_distribute_tester : public eosio_system_tester {
    public:
        abi_serializer abi_dist_ser;

        eosio_distribute_tester() : eosio_system_tester() {
            produce_block();
            // set eosio.dist code
            set_code( N(eosio.dist), contracts::distribute_wasm());
            set_abi( N(eosio.dist), contracts::distribute_abi().data() );

            
            const auto& accnt = control->db().get<account_object,by_name>( N(eosio.dist) );
            abi_def abi;
            BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
            abi_dist_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
        }

        action_result setdistrib(name authorizer, const std::vector<distribute_account>& accounts) {
            return push_action(authorizer, N(setdistrib), mvo()("accounts", accounts));
        }
}; // eosio_distribute_tester

BOOST_AUTO_TEST_SUITE(eosio_distribute_tests)

BOOST_AUTO_TEST_CASE( deploy_config ) try {
    eosio_distribute_tester t;
    
    // configure
    // BOOST_REQUIRE_EQUAL(t.success(), t.setdistrib(config::system_account_name, {}));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()