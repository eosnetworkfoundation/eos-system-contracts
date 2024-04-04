#include "eosio.system_tester.hpp"

#include <boost/test/unit_test.hpp>

using namespace eosio_system;

struct finalizer_key_tester : eosio_system_tester {
   static const std::string finalizer_key_1;
   static const std::string pop_1;
   static const std::string finalizer_key_2;
   static const std::string pop_2;
   static const std::string finalizer_key_3;
   static const std::string pop_3;

   fc::variant get_finalizer_key_info( uint64_t id ) {
      vector<char> data = get_row_by_id( config::system_account_name, config::system_account_name, "finkeys"_n, id );
      return abi_ser.binary_to_variant(
         "finalizer_key_info",
         data,
         abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_finalizer_info( const account_name& act ) {
      vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "finalizers"_n, act );
      return abi_ser.binary_to_variant(
         "finalizer_info",
         data,
         abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   action_result register_finalizer_key( const account_name& act, const std::string& finalizer_key, const std::string& pop  ) {
      return push_action( act, "regfinkey"_n, mvo()
                          ("finalizer_name", act)
                          ("finalizer_key", finalizer_key)
                          ("proof_of_possession", pop) );
   }

   action_result activate_finalizer_key( const account_name& act, const std::string& finalizer_key ) {
      return push_action( act, "actfinkey"_n, mvo()
                          ("finalizer_name",  act)
                          ("finalizer_key", finalizer_key) );
   }
};

const std::string finalizer_key_tester::finalizer_key_1 = "PUB_BLS_6j4Y3LfsRiBxY-DgvqrZNMCttHftBQPIWwDiN2CMhHWULjN1nGwM1O_nEEJefqwAG4X09n4Kdt4a1mfZ1ES1cLGjQo6uLLSloiVW4i9BUhMHU2nVujP1_U_9ihdI3egZ17N-iA";
const std::string finalizer_key_tester::finalizer_key_2 = "PUB_BLS_gtaOjOTa0NzDt8etBDqLoZKlfKTpTalcdfmbTJknLUJB2Fu4Cv-uoa8unF3bJ5kFewaCzf3tjYUyNE6CDSrwvYP5Nw47Y9oE9x4nqJWfJykMOoaI0kJz-GDrGN2nZdUAp5tWEg";
const std::string finalizer_key_tester::finalizer_key_3 = "PUB_BLS_CT8khvZYYZdObeIV9aTnd8fZ8bdaCL1UpSRyqNLZZM5sdrOSpOjDTAY2drTYGvQPVS21BhtD8acLJhqGyTfjqrWjyY5FTfqLdcligofSpa2lrG3FqKVNeUULR5QgcIMYga4vkQ";

const std::string pop1 = "SIG_BLS_N5r73_i50OVkydasCVVBOqqAqM4XQo_-DHgNawK77bcf06Bx0_rh5TNn9iZewNMZ6ecyEjs_sEkwjAXplhqyqf7S9FqSt8mfRxO7pE3bUZS0Z-Fxitsh9X0l_-kj3Z8VD8IwsaUwBLacudzShIXA-5E47cEqYoV3bGhANerKuDhZ4Pesm2xotAScK0pcNp0LbTNj0MZpVr0u6kJh169IoeG4ngCvD6uE2EicNrzyvDhu0u925Q1cm5z_bVha-DsANq3zcA";
const std::string pop2 = "SIG_BLS_9e1SzM60bWLdxwz4lYNQMNzMGeFuzFgJDYy7WykmynRVRQeIx2O2xnyzwv1WXvgYHLyMYZ4wK0Y_kU6jl330WazkBsw-_GzvIOGy8fnBnt5AyMaj9X5bhDbvB5MZc0QQz4-P2Z4SltTY17ZItGeekkjX_fgQ9kegM4qnuGU-2iqFj5i3Qf322L77b2SHjFoLmxdFOsfGpz7LyImSP8GcZH39W30cj5bmxfsp_90tGdAkz-7DG9nhSHYxFq6qTqMGijVPGg";
const std::string pop3 = "SIG_BLS_cJTQMGv1isqpcHEfhogLhlU56bKpGgo-Svi3Z4NXvWcly5TJo8hDChodIV-aEHgMqr06LuZftR7WFvgGkbOSdmwdO4t58R3RYOMSK-jjif2z-fEwCl7jsxUutASIRwIYtTI7h6NLCjARiKNi5BkES33xY8wYMWf-JkgbpsD2cGsZW4hkMW7T2j_1w89HmNwCn4V_hjlPM_kgz45RoYpKq4w2QEaLdCYTJ6xYOfc9Occc15c76dd1jjty_yT2RMAKO0mfUA";

BOOST_AUTO_TEST_SUITE(eosio_system_finalizer_key_tests)

const name alice = "alice1111111"_n;
const name bob   = "bob111111111"_n;

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_invalid_key_tests, finalizer_key_tester) try {
   { // attempt to register finalizer_key for an unregistered producer
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer is not a registered producer" ),
                           register_finalizer_key(alice, finalizer_key_1, pop1));
   }

   // Now alice1111111 registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   {  // finalizer key does not start with PUB_BLS
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key must start with PUB_BLS" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              ("finalizer_key", "UB_BLS_6j4Y3LfsRiBxY-DgvqrZNMCttHftBQPIWwDiN2CMhHWULjN1nGwM1O_nEEJefqwAG4X09n4Kdt4a1mfZ1ES1cLGjQo6uLLSloiVW4i9BUhMHU2nVujP1_U_9ihdI3egZ17N-iA" )
                              ("proof_of_possession", pop1 )
                           ) );
   }

   {  // proof_of_possession does not start with SIG_BLS
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "proof of possession siganture must start with SIG_BLS" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              ("finalizer_key", finalizer_key_1)
                              ("proof_of_possession", "XIG_BLS_N5r73_i50OVkydasCVVBOqqAqM4XQo_-DHgNawK77bcf06Bx0_rh5TNn9iZewNMZ6ecyEjs_sEkwjAXplhqyqf7S9FqSt8mfRxO7pE3bUZS0Z-Fxitsh9X0l_-kj3Z8VD8IwsaUwBLacudzShIXA-5E47cEqYoV3bGhANerKuDhZ4Pesm2xotAScK0pcNp0LbTNj0MZpVr0u6kJh169IoeG4ngCvD6uE2EicNrzyvDhu0u925Q1cm5z_bVha-DsANq3zcA")
                           ) );
   }

   {  // proof_of_possession fails
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "proof of possession check failed" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              // use a valid formatted finalizer_key for another signature
                              ("finalizer_key", finalizer_key_1)
                              ("proof_of_possession", pop2)
                           ) );
   }
} // register_finalizer_key_invalid_key_tests
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_by_same_finalizer_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // First finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );

   auto fin_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, fin_info["num_registered_keys"].as_uint64() );
   uint64_t active_key_id = fin_info["active_key_id"].as_uint64();

   // Cross check finalizer keys table
   auto fin_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, fin_key_info["finalizer_key"].as_string() );

   // Second finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop2 ));

   auto fin_info2 = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( 2, fin_info2["num_registered_keys"].as_uint64() ); // count incremented by 1
   BOOST_REQUIRE_EQUAL( active_key_id, fin_info2["active_key_id"].as_uint64() ); // active key should not change
}
FC_LOG_AND_RETHROW() // register_finalizer_key_by_same_finalizer_tests

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_duplicate_key_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // The first finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );

   auto fin_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, fin_info["num_registered_keys"].as_uint64() );

   // Same finalizer key as the first one
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "duplicate finalizer key" ),
                        register_finalizer_key(alice, finalizer_key_1, pop1) );
}
FC_LOG_AND_RETHROW() // register_finalizer_key_duplicate_key_tests

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_by_different_finalizers_tests, finalizer_key_tester) try {
   // register 2 producers
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // alice1111111 registers a finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );

   auto fin_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, fin_info["num_registered_keys"].as_uint64() );

   // bob111111111 registers another finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_2, pop2) );

   auto fin_info2 = get_finalizer_info(bob);
   BOOST_REQUIRE_EQUAL( 1, fin_info2["num_registered_keys"].as_uint64() );
}
FC_LOG_AND_RETHROW() // register_finalizer_key_by_different_finalizers_tests


BOOST_FIXTURE_TEST_CASE(register_duplicate_key_from_different_finalizers_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // The first finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );

   auto fin_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, fin_info["num_registered_keys"].as_uint64() );

   // bob111111111 tries to register the same finalizer key as the first one
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "duplicate finalizer key" ),
                        register_finalizer_key(bob, finalizer_key_1, pop1) );
}
FC_LOG_AND_RETHROW() // register_duplicate_key_from_different_finalizers_tests

BOOST_FIXTURE_TEST_CASE(activate_finalizer_key_failure_tests, finalizer_key_tester) try {
   // attempt to activate finalizer_key for an unregistered producer
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer is not a registered producer" ),
                         activate_finalizer_key(alice, finalizer_key_1) );

   // Register producers
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // finalizer has not registered any finalizer keys yet.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer has not registered any finalizer keys" ),
                        activate_finalizer_key(alice, finalizer_key_1) );

   // Alice registers a finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_2, pop2) );

   // Activate a finalizer key not registered by Alice
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key was not registered" ),
                        activate_finalizer_key(alice, finalizer_key_3) );

   // Activate a finalizer key not registered by Alice
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer_key was not registered by the finalizer" ),
                        activate_finalizer_key(alice, finalizer_key_2) );

   // Activate a finalizer key that is already active (the first key registered is
   // automatically set to active
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "the finalizer key was already active" ),
                        activate_finalizer_key(alice, finalizer_key_1) );
}
FC_LOG_AND_RETHROW() // activate_finalizer_key_failure_tests

BOOST_FIXTURE_TEST_CASE(activate_finalizer_key_success_tests, finalizer_key_tester) try {
   // Alice registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // Alice registers two finalizer keys. The first key is active
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop2) );

   // Check finalizer_key_1 is the active key
   auto alice_info = get_finalizer_info(alice);
   uint64_t active_key_id = alice_info["active_key_id"].as_uint64();
   auto finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, finalizer_key_info["finalizer_key"].as_string() );

   // Activate the second key
   BOOST_REQUIRE_EQUAL( success(), activate_finalizer_key(alice, finalizer_key_2) );

   // Check finalizer_key_2 is the active key
   alice_info = get_finalizer_info(alice);
   active_key_id = alice_info["active_key_id"].as_uint64();
   finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_2, finalizer_key_info["finalizer_key"].as_string() );
}
FC_LOG_AND_RETHROW() // activate_finalizer_key_success_tests

BOOST_AUTO_TEST_SUITE_END()
