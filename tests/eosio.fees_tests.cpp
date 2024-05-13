#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_fees_tests);

BOOST_FIXTURE_TEST_CASE( fees_with_rex, eosio_system_tester ) try {
   // init REX
   const std::vector<account_name> accounts = { "alice"_n };
   const account_name alice = accounts[0];
   const asset init_balance = core_sym::from_string("1000.0000");
   setup_rex_accounts( accounts, init_balance );
   buyrex( alice, core_sym::from_string("10.0000"));

   // manual token transfer to fees account
   const asset fees_before = get_balance( "eosio.fees" );
   const asset rex_before = get_balance( "eosio.rex" );
   const asset eosio_before = get_balance( "eosio" );

   const asset fee = core_sym::from_string("100.0000");
   transfer( config::system_account_name, "eosio.fees"_n, fee, config::system_account_name );

   const asset fees_after = get_balance( "eosio.fees" );
   const asset rex_after = get_balance( "eosio.rex" );
   const asset eosio_after = get_balance( "eosio" );

   BOOST_REQUIRE_EQUAL( fees_before, fees_after );
   BOOST_REQUIRE_EQUAL( rex_before + fee, rex_after );
   BOOST_REQUIRE_EQUAL( eosio_before - fee, eosio_after );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( fees_without_rex, eosio_system_tester ) try {
   // manual token transfer to fees account
   const asset fees_before = get_balance( "eosio.fees" );
   const asset rex_before = get_balance( "eosio.rex" );
   const asset eosio_before = get_balance( "eosio" );

   const asset fee = core_sym::from_string("100.0000");
   transfer( config::system_account_name, "eosio.fees"_n, fee, config::system_account_name );

   const asset fees_after = get_balance( "eosio.fees" );
   const asset rex_after = get_balance( "eosio.rex" );
   const asset eosio_after = get_balance( "eosio" );

   BOOST_REQUIRE_EQUAL( fees_before + fee, fees_after );
   BOOST_REQUIRE_EQUAL( rex_before, rex_after );
   BOOST_REQUIRE_EQUAL( eosio_before - fee, eosio_after );

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
