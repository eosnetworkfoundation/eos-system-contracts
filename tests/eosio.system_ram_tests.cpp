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

BOOST_AUTO_TEST_SUITE(eosio_system_ram_tests)

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

   BOOST_REQUIRE_EQUAL( success(), ramtransfer( alice, bob, 1000, "" ) );

   const uint64_t alice_after = get_total_stake( alice )["ram_bytes"].as_uint64();
   const uint64_t bob_after = get_total_stake( bob )["ram_bytes"].as_uint64();

   BOOST_REQUIRE_EQUAL( alice_before - 1000, alice_after );
   BOOST_REQUIRE_EQUAL( bob_before + 1000, bob_after );

   // RAM burn
   BOOST_REQUIRE_EQUAL( success(), ramburn( alice, 1000, "burn RAM memo" ) );
   const uint64_t alice_after_burn = get_total_stake( alice )["ram_bytes"].as_uint64();
   BOOST_REQUIRE_EQUAL( alice_before - 2000, alice_after_burn );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
