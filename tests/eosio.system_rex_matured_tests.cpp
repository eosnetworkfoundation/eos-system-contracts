#include <boost/test/unit_test.hpp>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_system_rex_tests)

bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }

BOOST_FIXTURE_TEST_CASE( buy_sell_matured_rex, eosio_system_tester ) try {
   // @param num_of_maturity_buckets - used to calculate maturity time of purchase REX tokens from end of the day UTC.
   // @param sell_matured_rex - if true, matured REX is sold immediately.
   // @param buy_rex_to_savings - if true, buying REX is moved immediately to REX savings.
   //
   // https://github.com/eosnetworkfoundation/eos-system-contracts/issues/132
   // https://github.com/eosnetworkfoundation/eos-system-contracts/issues/134
   // https://github.com/eosnetworkfoundation/eos-system-contracts/issues/135

   // setup accounts
   const int64_t ratio        = 10000;
   const asset   init_rent    = core_sym::from_string("20000.0000");
   const asset   init_balance = core_sym::from_string("1000.0000");
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n, "charly"_n, "david"_n, "mark"_n };
   account_name alice = accounts[0], bob = accounts[1], charly = accounts[2], david = accounts[3], mark = accounts[4];
   setup_rex_accounts( accounts, init_balance );


   // 1. set `num_of_maturity_buckets=21` to test increasing maturity time of buying REX tokens.
   BOOST_REQUIRE_EQUAL( success(), setrexmature( 21, false, false ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("25000.0000 REX"), get_buyrex_result( alice, core_sym::from_string("2.5000") ) );
   produce_blocks(2);
   produce_block(fc::days(5));
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient available rex"), sellrex( alice, asset::from_string("25000.0000 REX") ) );
   produce_block(fc::days(16));

   BOOST_REQUIRE_EQUAL( core_sym::from_string("2.5000"),     get_sellrex_result( alice, asset::from_string("25000.0000 REX") ) );
   BOOST_REQUIRE_EQUAL( success(),                           buyrex( alice, core_sym::from_string("13.0000") ) );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("13.0000"),    get_rex_vote_stake( alice ) );
   BOOST_REQUIRE_EQUAL( success(),                           buyrex( alice, core_sym::from_string("17.0000") ) );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("30.0000"),    get_rex_vote_stake( alice ) );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("970.0000"),   get_rex_fund(alice) );
   BOOST_REQUIRE_EQUAL( get_rex_balance(alice).get_amount(), ratio * asset::from_string("30.0000 REX").get_amount() );
   auto rex_pool = get_rex_pool();
   BOOST_REQUIRE_EQUAL( core_sym::from_string("30.0000"),   rex_pool["total_lendable"].as<asset>() );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("30.0000"),   rex_pool["total_unlent"].as<asset>() );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"),    rex_pool["total_lent"].as<asset>() );
   BOOST_REQUIRE_EQUAL( init_rent,                          rex_pool["total_rent"].as<asset>() );
   BOOST_REQUIRE_EQUAL( get_rex_balance(alice),             rex_pool["total_rex"].as<asset>() );

   // 2. set `sell_matured_rex=true` and `buy_rex_to_savings=false` to test buying REX without moving it to REX savings
   BOOST_REQUIRE_EQUAL( success(), setrexmature( 21, true, false ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("25000.0000 REX"), get_buyrex_result( bob, core_sym::from_string("2.5000") ) );
   produce_blocks(2);
   produce_block(fc::days(5));
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient available rex"), sellrex( bob, asset::from_string("25000.0000 REX") ) );
   produce_block(fc::days(16));
   BOOST_REQUIRE_EQUAL( core_sym::from_string("2.5000"), get_rex_vote_stake( bob ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("10000.0000 REX"), get_buyrex_result( bob, core_sym::from_string("1.0000") ) ); // will also triggers sell matured REX
   BOOST_REQUIRE_EQUAL( core_sym::from_string("1.0000"), get_rex_vote_stake( bob ) );

   // 3. set `sell_matured_rex=false` and `buy_rex_to_savings=true` to test selling matured REX
   BOOST_REQUIRE_EQUAL( success(), setrexmature( 21, false, true ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("25000.0000 REX"), get_buyrex_result( charly, core_sym::from_string("2.5000") ) ); // when buying REX, it will automatically be moved to REX savings
   BOOST_REQUIRE_EQUAL( success(), mvfrsavings( charly, asset::from_string("25000.0000 REX") ) ); // move REX from savings to initiate matured REX unstaking process
   produce_blocks(2);
   produce_block(fc::days(5));
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient available rex"), sellrex( charly, asset::from_string("25000.0000 REX") ) );
   produce_block(fc::days(16));
   BOOST_REQUIRE_EQUAL( success(),  updaterex( charly ) ); // triggers sell matured REX (any REX action causes sell matured REX)
   BOOST_REQUIRE_EQUAL( core_sym::from_string("2.5000"), get_sellrex_result( charly, asset::from_string("25000.0000 REX") ) );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_rex_vote_stake( charly ) );
   BOOST_REQUIRE_EQUAL( init_balance, get_rex_fund( charly ) );

   // 4. legacy holders with matured REX
   BOOST_REQUIRE_EQUAL( success(), setrexmature( 5, false, false ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("25000.0000 REX"), get_buyrex_result( david, core_sym::from_string("2.5000") ) ); // legacy 5 days maturity
   BOOST_REQUIRE_EQUAL( asset::from_string("25000.0000 REX"), get_buyrex_result( mark, core_sym::from_string("2.5000") ) );
   produce_blocks(2);
   produce_block(fc::days(5));
   BOOST_REQUIRE_EQUAL( success(), setrexmature( 21, true, true ) );
   BOOST_REQUIRE_EQUAL( asset::from_string("10000.0000 REX"), get_buyrex_result( david, core_sym::from_string("1.0000") ) ); // new 21 days maturity & triggers sell matured REX

   // 4.1. Test selling less than all their matured rex, and having all of their already matured rex sold regardless
   BOOST_REQUIRE_EQUAL( core_sym::from_string("2.5000"), get_sellrex_result( mark, asset::from_string("1.0000 REX") ) );

   BOOST_REQUIRE_EQUAL( success(), mvfrsavings( david, asset::from_string("10000.0000 REX") ) ); // must move REX from savings to initiate matured REX unstaking process
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient available rex"), sellrex( david, asset::from_string("25000.0000 REX") ) ); // already sold when previously buying REX
   produce_blocks(2);
   produce_block(fc::days(5));
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient available rex"), sellrex( david, asset::from_string("10000.0000 REX") ) ); // 21 day REX not matured yet
   produce_blocks(2);
   produce_block(fc::days(21));
   BOOST_REQUIRE_EQUAL( core_sym::from_string("1.0000"), get_sellrex_result( david, asset::from_string("10000.0000 REX") ) );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_rex_vote_stake( david ) );
   BOOST_REQUIRE_EQUAL( init_balance, get_rex_fund( david ) );



} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
