#include <boost/test/unit_test.hpp>
#include <eosio/chain/exceptions.hpp>


#include "eosio.system_tester.hpp"

using namespace eosio_system;

struct distribute_account {
    name        account;
    uint16_t    percent;
};
FC_REFLECT(distribute_account, (account)(percent))

constexpr auto sys                 = N(eosio);
constexpr auto dist_acct           = N(eosio.dist);
constexpr auto rex_acct            = N(eosio.rex);
constexpr auto alice               = N(alice);
constexpr auto bob                 = N(bob);


class eosio_distribute_tester : public eosio_system_tester {
    public:
        abi_serializer abi_dist_ser;

        eosio_distribute_tester() : eosio_system_tester() {
            produce_block();
            // set eosio.dist code
            set_code( dist_acct, contracts::distribute_wasm());
            set_abi( dist_acct, contracts::distribute_abi().data() );

            
            const auto& accnt = control->db().get<account_object,by_name>( dist_acct );
            abi_def abi;
            BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
            abi_dist_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
        }

        action_result push_action(name account, name act, name authorizer, const variant_object& data) {
            try {
                base_tester::push_action(account, act, authorizer, data, 1);
            } catch (const fc::exception& ex) {
                edump((ex.to_detail_string()));
                return error(ex.top_message());
            }
            return success();
        }

        action_result setdistrib(name authorizer, const std::vector<distribute_account>& accounts) {
            return push_action(dist_acct, N(setdistrib), authorizer, mvo()("accounts", accounts));
        }

        action_result claimdistrib(name acct) {
            return push_action(dist_acct, N(claimdistrib), acct, mvo()("claimer", acct));
        }

        fc::variant get_state(){
            vector<char> data = get_row_by_account(dist_acct, dist_acct, N(state), N(state));
            return data.empty() ? fc::variant() : abi_dist_ser.binary_to_variant("distribute_state", data, abi_serializer::create_yield_function(abi_serializer_max_time));
        }

        fc::variant get_claimer(const name& acct){
            vector<char> data = get_row_by_account(dist_acct, dist_acct, N(claimers), acct);
            return data.empty() ? fc::variant() : abi_dist_ser.binary_to_variant("distribute_claimer", data, abi_serializer::create_yield_function(abi_serializer_max_time));
        }

}; // eosio_distribute_tester

BOOST_AUTO_TEST_SUITE(eosio_distribute_tests)

BOOST_AUTO_TEST_CASE( deploy_config ) try {
    eosio_distribute_tester t;
    
    // configure checks
    BOOST_REQUIRE_EQUAL(t.error("missing authority of eosio.dist"), t.setdistrib(sys, {}));
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Cannot set account to eosio.dist"), t.setdistrib(dist_acct, { {dist_acct, 10000} }));
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Account does not exist: alice"), t.setdistrib(dist_acct, { {alice, 10000} }));
    t.create_accounts_with_resources({alice, bob}, sys);
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Total percentage does not equal 100%"), t.setdistrib(dist_acct, { {alice, 1000} }));
    // force overflow
    uint16_t pct = 65535;
    // pct++;
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Only positive percentages are allowed"), t.setdistrib(dist_acct, { {alice, ++pct} }));
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Total percentage exceeds 100%"), t.setdistrib(dist_acct, { {alice, 5000}, {bob, 5001} }));

    // rex is not enabled
    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("REX must be enabled"), t.setdistrib(dist_acct, { {rex_acct, 10000}}));

    auto check_state = [](std::vector<distribute_account> claimants, std::vector<distribute_account> accounts_state) {
        ilog("claimants: ${claimants}",("claimants",claimants));
        ilog("state: ${state}",("state",accounts_state));
        BOOST_REQUIRE_EQUAL(claimants.size(), accounts_state.size());
        for(size_t i = 0; i < claimants.size(); ++i) {
            BOOST_REQUIRE_EQUAL(claimants[i].account, accounts_state[i].account );
            BOOST_REQUIRE_EQUAL(claimants[i].percent, accounts_state[i].percent );
        }
    };

    // successful 
    std::vector<distribute_account> distribution = { {alice, 5000}, {bob, 5000} };
    BOOST_REQUIRE_EQUAL(t.success(), t.setdistrib(dist_acct, distribution));

    // get state  
    auto accounts_state = t.get_state()["accounts"].as<std::vector<distribute_account>>();
    check_state(distribution, accounts_state);

    // reset claimers
    distribution = {};
    BOOST_REQUIRE_EQUAL(t.success(), t.setdistrib(dist_acct, distribution));
    accounts_state = t.get_state()["accounts"].as<std::vector<distribute_account>>();
    check_state(distribution, accounts_state);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE( transfer ) try {
    eosio_distribute_tester t;
    
    auto check_claimer = [&](const name& acct, const asset& balance, bool is_empty = false) {
        auto cl = t.get_claimer(acct);
        if(is_empty){
            BOOST_REQUIRE(cl.is_null());
        } else {
            ilog("${cl}",("cl", cl));
            BOOST_REQUIRE_EQUAL(balance, cl["balance"].as<asset>());
        }
    };
    t.create_accounts_with_resources({alice, bob}, sys);
    std::vector<distribute_account> distribution = { {alice, 5000}, {bob, 5000} };
    BOOST_REQUIRE_EQUAL(t.success(), t.setdistrib(dist_acct, distribution));

    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Not a valid distribution account"), t.claimdistrib(alice));
    t.issue_and_transfer(dist_acct, core_sym::from_string("1000.0000"));
    
    t.produce_block();
    
    check_claimer(alice, core_sym::from_string("500.0000"));
    check_claimer(bob, core_sym::from_string("500.0000"));

    BOOST_REQUIRE_EQUAL(t.success(), t.claimdistrib(alice));
    BOOST_REQUIRE_EQUAL(t.success(), t.claimdistrib(bob));
    t.produce_block();
    check_claimer(alice, core_sym::from_string("0.0000"), true);

    BOOST_REQUIRE_EQUAL(t.wasm_assert_msg("Not a valid distribution account"), t.claimdistrib(alice));

    distribution = { {alice, 9999}, {bob, 1} };
    BOOST_REQUIRE_EQUAL(t.success(), t.setdistrib(dist_acct, distribution));

    t.issue_and_transfer(dist_acct, core_sym::from_string("100.0000"));
    auto alice_bal = t.get_claimer(alice)["balance"].as<asset>();
    auto bob_bal = t.get_claimer(bob)["balance"].as<asset>();

    BOOST_REQUIRE_EQUAL(core_sym::from_string("100.0000"), alice_bal + bob_bal);


} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()