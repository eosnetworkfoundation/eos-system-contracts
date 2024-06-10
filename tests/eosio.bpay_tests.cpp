#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_bpay_tests);

account_name voter = "alice1111111"_n;
account_name standby = "bp.standby"_n;
account_name inactive = "bp.inactive"_n;
account_name fees = "eosio.fees"_n;
account_name bpay = "eosio.bpay"_n;

BOOST_FIXTURE_TEST_CASE( bpay_test, eosio_system_tester ) try {


   // Transferring some tokens to the fees account
   // since tokens from eosio will not be directly accepted as contributions to 
   // the bpay contract
   transfer( config::system_account_name, fees, core_sym::from_string("100000.0000"), config::system_account_name );

   
   // Setting up the producers, standby and inactive producers, and voting them in
   setup_producer_accounts({standby, inactive});
   auto producer_names = active_and_vote_producers();
   
   BOOST_REQUIRE_EQUAL( success(), regproducer(standby) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(inactive) );
   vector<name> top_producers_and_inactive = {inactive};
   top_producers_and_inactive.insert( top_producers_and_inactive.end(), producer_names.begin(), producer_names.begin()+21 );

   BOOST_REQUIRE_EQUAL( success(), vote( voter, top_producers_and_inactive ) );
   produce_blocks( 250 );


   BOOST_REQUIRE_EQUAL( 0, get_producer_info( standby )["unpaid_blocks"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( get_producer_info( producer_names[0] )["unpaid_blocks"].as<uint32_t>() > 0, true );

   // TODO: Check nothing happened here, no rewards since it comes from system account
   
   asset rewards_sent = core_sym::from_string("1000.0000");
   transfer( fees, bpay, rewards_sent, fees);

   // rewards / 21
   asset balance_per_producer = core_sym::from_string("47.6190"); 
   
   auto rewards = get_bpay_rewards(producer_names[0]);

   // bp.inactive is still active, so should be included in the rewards
   BOOST_REQUIRE_EQUAL( get_bpay_rewards(inactive)["quantity"].as<asset>(), balance_per_producer );
   // Random sample
   BOOST_REQUIRE_EQUAL( get_bpay_rewards(producer_names[11])["quantity"].as<asset>(), balance_per_producer );


   // Deactivating a producer
   BOOST_REQUIRE_EQUAL( success(), push_action(config::system_account_name, "rmvproducer"_n, mvo()("producer", inactive) ) );
   BOOST_REQUIRE_EQUAL( false, get_producer_info( inactive )["is_active"].as<bool>() );

   transfer( fees, bpay, rewards_sent, fees);
   BOOST_REQUIRE_EQUAL( get_bpay_rewards(inactive)["quantity"].as<asset>(), balance_per_producer );
   BOOST_REQUIRE_EQUAL( get_bpay_rewards(producer_names[11])["quantity"].as<asset>(), core_sym::from_string("95.2380") );

   // BP should be able to claim their rewards
   {
      auto prod = producer_names[11];
      BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_balance( prod ) );
      BOOST_REQUIRE_EQUAL( success(), bpay_claimrewards( prod ) );
      BOOST_REQUIRE_EQUAL( core_sym::from_string("95.2380"), get_balance( prod ) );
      BOOST_REQUIRE_EQUAL( true, get_bpay_rewards(prod).is_null() );   

      // should still have rewards for another producer
      BOOST_REQUIRE_EQUAL( get_bpay_rewards(producer_names[10])["quantity"].as<asset>(), core_sym::from_string("95.2380") );
   }

   // Should be able to claim rewards from a producer that is no longer active
   {
      BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_balance( inactive ) );
      BOOST_REQUIRE_EQUAL( success(), bpay_claimrewards( inactive ) );
      BOOST_REQUIRE_EQUAL( core_sym::from_string("47.6190"), get_balance( inactive ) );
      BOOST_REQUIRE_EQUAL( true, get_bpay_rewards(inactive).is_null() );   
   }

   // Should not have rewards for a producer that was never active
   {
      BOOST_REQUIRE_EQUAL( true, get_bpay_rewards(standby).is_null() );
      BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_balance( standby ) );
      BOOST_REQUIRE_EQUAL( wasm_assert_msg("no rewards to claim"), bpay_claimrewards( standby ) );
      BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_balance( standby ) );
   }

   // Tokens transferred from the eosio account should be ignored
   {
      transfer( config::system_account_name, bpay, rewards_sent, config::system_account_name );
      BOOST_REQUIRE_EQUAL( get_bpay_rewards(producer_names[10])["quantity"].as<asset>(), core_sym::from_string("95.2380") );
   }

   

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()