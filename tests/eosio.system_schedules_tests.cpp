#include <boost/test/unit_test.hpp>
#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_system_vesting_tests)

BOOST_FIXTURE_TEST_CASE(set_schedules, eosio_system_tester) try {


   auto check_schedule = [&](time_point_sec time, double rate){
      auto schedule = get_vesting_schedule(time.sec_since_epoch());
      REQUIRE_MATCHING_OBJECT( schedule, mvo()
         ("start_time", time)
         ("continuous_rate", rate)
      );
   };

   const std::vector<account_name> accounts = { "alice"_n };
   create_accounts_with_resources( accounts );
   const account_name alice = accounts[0];

   const uint32_t YEAR = 86400 * 365;
   uint32_t initial_start_time = control->pending_block_time().sec_since_epoch(); // 1577836853 (2020-01-01T00:00:56.000Z)
   time_point_sec start_time = time_point_sec(initial_start_time);


   BOOST_REQUIRE_EQUAL( wasm_assert_msg("continuous_rate can't be over 100%"), setschedule(start_time, 1.00001) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("continuous_rate can't be negative"), setschedule(start_time, -1) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("schedule not found"), delschedule(start_time) );

   // action validation
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(0), 0.05) ); 
   BOOST_REQUIRE_EQUAL( success(), setschedule(start_time, 0.05) ); 
   check_schedule(start_time, 0.05);

   // allow override existing schedules
   BOOST_REQUIRE_EQUAL( success(), setschedule(start_time, 0.02) );
   check_schedule(start_time, 0.02);
   

   // Should be able to delete schedules, even in the past
   BOOST_REQUIRE_EQUAL( success(), delschedule(start_time) );
   BOOST_REQUIRE_EQUAL( success(), delschedule(time_point_sec(0)) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("schedule not found"), delschedule(start_time) );

   // Resetting timers to make math clean
   initial_start_time = control->pending_block_time().sec_since_epoch();
   start_time = time_point_sec(initial_start_time);
   

   // // set 4 future schedules
   BOOST_REQUIRE_EQUAL( success(), setschedule(start_time, 0.02) );
   check_schedule(start_time, 0.02);
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 4), 0.01) );
   check_schedule(time_point_sec(initial_start_time + YEAR * 4), 0.01);
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 8), 0.005) );
   check_schedule(time_point_sec(initial_start_time + YEAR * 8), 0.005);
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 12), 0.0005) );
   check_schedule(time_point_sec(initial_start_time + YEAR * 12), 0.0005);
   BOOST_REQUIRE_EQUAL( success(), setschedule(time_point_sec(initial_start_time + YEAR * 13), 0) );
   check_schedule(time_point_sec(initial_start_time + YEAR * 13), 0);

   // current state prior to first schedule change execution
   const double before = get_global_state4()["continuous_rate"].as_double();
   BOOST_REQUIRE_EQUAL( before, 0.048790164169432007 ); // 5% continuous rate

   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) ); 
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.02 );

   // Cannot execute schedule before its time is due
   // (we did 6 actions so we're late 3s late)
   auto late = fc::seconds(3);
   produce_block( fc::seconds(YEAR * 4) - fc::seconds(1) - late ); // advance to year 4
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("no schedule to execute"), execschedule(alice) );

   // Can execute this schedule 1 second after, as that is its time
   produce_block( fc::seconds(1) ); // advance to year 4
   BOOST_REQUIRE_EQUAL( control->pending_block_time().sec_since_epoch(), initial_start_time + YEAR * 4 ); // 2024-01-01T00:00
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.01 ); // 1% continuous rate

   produce_block( fc::days(365 * 4) ); // advanced to year 8
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.005 ); // 0.5% continuous rate

   produce_block( fc::days(365 * 4) ); // advanced to year 12
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.0005 ); // 0.05% continuous rate

   produce_block( fc::days(365 * 1) ); // advanced to year 13
   BOOST_REQUIRE_EQUAL( success(), execschedule(alice) );
   BOOST_REQUIRE_EQUAL( get_global_state4()["continuous_rate"].as_double(), 0.0 ); // 0% continuous rate

   // no more schedules
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("no schedule to execute"), execschedule(alice) );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
