#include <boost/test/unit_test.hpp>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_system_vesting_tests)

BOOST_FIXTURE_TEST_CASE(set_schedules, eosio_system_tester) try {

   const std::vector<account_name> accounts = { "alice"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];

   const uint32_t YEAR = 86400 * 365;
   const uint32_t initial_start_time = 1577836856; // 2020-01-01T00:00
   const time_point_sec start_time = time_point_sec(initial_start_time);

   // action validation
   BOOST_REQUIRE_EQUAL( success(), setschedule(start_time, 500) ); // 5% annual rate
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("annual_rate was not modified"), setschedule(start_time, 500) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("annual_rate can't be negative"), setschedule(start_time, -1) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("schedule not found"), delschedule(time_point_sec(0)) );
   BOOST_REQUIRE_EQUAL( success(), setschedule(start_time, 200) ); // allow override existing schedules
   BOOST_REQUIRE_EQUAL( success(), delschedule(start_time) );

   // set 3 future schedules
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(start_time), 200) );
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 4), 100) );
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 8), 50) );

   // current state prior to first schedule
   const double before = get_global_state4()["continuous_rate"].as_double();
   BOOST_REQUIRE_EQUAL( before, 0.048790164169432007 ); // 5% annual rate
   const uint32_t current_time_sec = control->pending_block_time().sec_since_epoch();
   BOOST_REQUIRE_EQUAL( current_time_sec, initial_start_time ); // 2020-01-01T00:00

   // advance to halvening schedules
   produce_block( fc::days(365 * 4) ); // advance to year 4
   BOOST_REQUIRE_EQUAL( control->pending_block_time().sec_since_epoch(), initial_start_time + YEAR * 4 ); // 2024-01-01T00:00
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.019802627296179712 ); // 2% annual rate

   produce_block( fc::days(365 * 4) ); // advanced to year 8
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.0099503308531680833 ); // 1% annual rate

   produce_block( fc::days(365 * 4) ); // advanced to year 12
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.0049875415110390738 ); // 0.5% annual rate

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
