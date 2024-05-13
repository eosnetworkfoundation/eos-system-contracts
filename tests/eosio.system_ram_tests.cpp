#include <boost/test/unit_test.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fc/log/logger.hpp>
#include <eosio/chain/exceptions.hpp>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_system_ram_tests);

// ramtransfer
BOOST_FIXTURE_TEST_CASE( ram_transfer, eosio_system_tester ) try {
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];
   const account_name bob = accounts[1];

   transfer( config::system_account_name, alice, core_sym::from_string("100.0000"), config::system_account_name );
   transfer( config::system_account_name, bob, core_sym::from_string("100.0000"), config::system_account_name );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( alice, alice, 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( bob, bob, 10000 ) );

   const uint64_t alice_before = get_total_stake( alice )["ram_bytes"].as_uint64();
   const uint64_t bob_before = get_total_stake( bob )["ram_bytes"].as_uint64();

   ramtransfer( alice, bob, 1000, "" );

   const uint64_t alice_after = get_total_stake( alice )["ram_bytes"].as_uint64();
   const uint64_t bob_after = get_total_stake( bob )["ram_bytes"].as_uint64();

   BOOST_REQUIRE_EQUAL( alice_before - 1000, alice_after );
   BOOST_REQUIRE_EQUAL( bob_before + 1000, bob_after );

   /*
    * The from_ram_bytes is alice's ram byte total
    * The to_ram_bytes is bob's ram byte total
    * Accounts start with 8,000 ram bytes
    *     this is via create_accounts_with_resources()
    * Next buyram on each account purchases 10,000 additional ram bytes
    *     minus fees of 17 ram bytes
    *
    * Before ram transfer the totals for each account are 17,983 ram bytes
    * After transfer of 1,000 bytes
    *      bob and alice respective totals are 18,983 and 16,983
    * After the validate ram transfer below of 1 ram byte
    *      bob and alices respective totals are 18,984 and 16,982
    */
   const char* expected_return_data = R"=====(
{
   "from": "alice",
   "to": "bob",
   "bytes": 1,
   "from_ram_bytes": 16982,
   "to_ram_bytes": 18984
}
)=====";
   validate_ramtransfer_return(alice, bob, 1, "",
                               "action_return_ramtransfer", expected_return_data );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( buy_sell_ram_validate, eosio_system_tester ) try {
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];
   const account_name bob = accounts[1];

   transfer( config::system_account_name, alice, core_sym::from_string("100.0000"), config::system_account_name );
   transfer( config::system_account_name, bob, core_sym::from_string("100.0000"), config::system_account_name );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( bob, bob, 10000 ) );

   const char* expected_buyrambytes_return_data = R"=====(
{
   "payer": "alice",
   "receiver": "alice",
   "quantity": "0.1462 TST",
   "bytes_purchased": 9991,
   "ram_bytes": 17983,
   "fee": "0.0008 TST"
}
)=====";
   validate_buyrambytes_return(alice, alice, 10000,
                               "action_return_buyram", expected_buyrambytes_return_data );

   const char* expected_sellram_return_data = R"=====(
{
   "account": "alice",
   "quantity": "0.1455 TST",
   "bytes_sold": 10000,
   "ram_bytes": 7983,
   "fee": "0.0008 TST"
}
)=====";
   validate_sellram_return(alice, 10000,
                        "action_return_sellram", expected_sellram_return_data );

   const char* expected_buyram_return_data = R"=====(
{
   "payer": "bob",
   "receiver": "alice",
   "quantity": "2.0000 TST",
   "bytes_purchased": 136750,
   "ram_bytes": 144733,
   "fee": "0.0100 TST"

}
)=====";
   validate_buyram_return(bob, alice, core_sym::from_string("2.0000"),
                          "action_return_buyram", expected_buyram_return_data );
} FC_LOG_AND_RETHROW()

// ramburn
BOOST_FIXTURE_TEST_CASE( ram_burn, eosio_system_tester ) try {
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];
   const account_name bob = accounts[1];
   const account_name null_account = "eosio.null"_n;

   transfer( config::system_account_name, alice, core_sym::from_string("100.0000"), config::system_account_name );
   transfer( config::system_account_name, bob, core_sym::from_string("100.0000"), config::system_account_name );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( alice, alice, 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( alice, null_account, 10000 ) );

   const char* expected_buyramself_return_data = R"=====(
{
   "payer": "bob",
   "receiver": "bob",
   "quantity": "10.0000 TST",
   "bytes_purchased": 683747,
   "ram_bytes": 691739,
   "fee": "0.0500 TST"
}
)=====";
   validate_buyramself_return(bob, core_sym::from_string("10.0000"),
                              "action_return_buyram", expected_buyramself_return_data ) ;

   const uint64_t null_before_burn = get_total_stake( null_account )["ram_bytes"].as_uint64();
   const uint64_t alice_before_burn = get_total_stake( alice )["ram_bytes"].as_uint64();

   // burn action
   BOOST_REQUIRE_EQUAL( success(), ramburn( alice, 3000, "burn RAM memo" ) );
   const uint64_t alice_after_burn = get_total_stake( alice )["ram_bytes"].as_uint64();
   const uint64_t null_after_burn = get_total_stake( null_account )["ram_bytes"].as_uint64();
   BOOST_REQUIRE_EQUAL( alice_before_burn - 3000, alice_after_burn );
   BOOST_REQUIRE_EQUAL( null_before_burn + 3000, null_after_burn );

   const char* expected_ramburn_return_data = R"=====(
{
   "from": "bob",
   "to": "eosio.null",
   "bytes": 1,
   "from_ram_bytes": 691738,
   "to_ram_bytes": 12992,
   "fee": "1.0000 TST"
}
)=====";
   validate_ramburn_return(bob, 1, "burn RAM memo",
                           "action_return_ramtransfer", expected_ramburn_return_data );

} FC_LOG_AND_RETHROW()

// buyramburn
BOOST_FIXTURE_TEST_CASE( buy_ram_burn, eosio_system_tester ) try {
   const std::vector<account_name> accounts = { "alice"_n };
   const account_name alice = accounts[0];
   const account_name null_account = "eosio.null"_n;

   create_accounts_with_resources( accounts );
   transfer( config::system_account_name, alice, core_sym::from_string("100.0000"), config::system_account_name );

   BOOST_REQUIRE_EQUAL( success(), buyrambytes( alice, alice, 10000 ) );
   BOOST_REQUIRE_EQUAL( success(), buyrambytes( alice, null_account, 10000 ) );

   const uint64_t null_before_buyramburn = get_total_stake( null_account )["ram_bytes"].as_uint64();
   const uint64_t alice_before_buyramburn = get_total_stake( alice )["ram_bytes"].as_uint64();
   const asset initial_alice_balance = get_balance(alice);
   const asset ten_core_token = core_sym::from_string("10.0000");

   // buy ram burn action
   BOOST_REQUIRE_EQUAL( success(), buyramburn( alice, ten_core_token, "burn RAM burn memo" ) );
   const uint64_t alice_after_buyramburn = get_total_stake( alice )["ram_bytes"].as_uint64();
   const uint64_t null_after_buyramburn = get_total_stake( null_account )["ram_bytes"].as_uint64();
   BOOST_REQUIRE_EQUAL( alice_before_buyramburn, alice_after_buyramburn );
   BOOST_REQUIRE_EQUAL( true, null_before_buyramburn < null_after_buyramburn );
   BOOST_REQUIRE_EQUAL( initial_alice_balance - ten_core_token,  get_balance(alice));
} FC_LOG_AND_RETHROW()

// buyramself
BOOST_FIXTURE_TEST_CASE( buy_ram_self, eosio_system_tester ) try {
   const std::vector<account_name> accounts = { "alice"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];

   transfer( config::system_account_name, alice, core_sym::from_string("100.0000"), config::system_account_name );
   const uint64_t alice_before = get_total_stake( alice )["ram_bytes"].as_uint64();
   BOOST_REQUIRE_EQUAL( success(), buyramself( alice, core_sym::from_string("1.0000")) );
   const uint64_t alice_after = get_total_stake( alice )["ram_bytes"].as_uint64();
   BOOST_REQUIRE_EQUAL( alice_before + 68375, alice_after );

   const char* expected_buyramself_return_data = R"=====(
{
   "payer": "alice",
   "receiver": "alice",
   "quantity": "2.0000 TST",
   "bytes_purchased": 136750,
   "ram_bytes": 213117,
   "fee": "0.0100 TST"
}
)=====";

   validate_buyramself_return(alice, core_sym::from_string("2.0000"),
                       "action_return_buyram", expected_buyramself_return_data );
} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
