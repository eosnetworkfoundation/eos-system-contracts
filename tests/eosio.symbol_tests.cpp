#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include "eosio.system_tester.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class eosio_symbol_test : public tester {
public:

   eosio_symbol_test() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(eosio.symbol), N(eosio.token) } );
      produce_blocks( 2 );

      set_code( N(eosio.symbol), contracts::symbol_wasm() );
      set_abi( N(eosio.symbol), contracts::symbol_abi().data() );
      produce_blocks( 2 );
      set_code( N(eosio.token), contracts::token_wasm() );
      set_abi( N(eosio.token), contracts::token_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(eosio.symbol) );
      const auto& accnt2 = control->db().get<account_object,by_name>( N(eosio.token) );
      abi_def abi;
      abi_def abi2;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt2.abi, abi2), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
      abi_ser2.set_abi(abi2, abi_serializer::create_yield_function(abi_serializer_max_time));

      create_token_contract("eosio.token"_n, asset::from_string("1000000000.0000 EOS"));
      issue("eosio.token"_n, asset::from_string("1000000000.0000 EOS"), "");
   }

   fc::variant get_account_wallet( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(eosio.token), acc, N(accounts), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser2.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_account_credit( name& acc )
   {
      vector<char> data = get_row_by_account( "eosio.symbol"_n, "eosio.symbol"_n, "accounts"_n, acc );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "user", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_sym_config( const uint32_t& symbol_length )
   {
      vector<char> data = get_row_by_account( "eosio.symbol"_n, "eosio.symbol"_n, "symconfigs"_n, name{symbol_length} );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "symconfig", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(eosio.symbol);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   action_result push_token_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser2.get_action_type(name);

      action act;
      act.account = N(eosio.token);
      act.name    = name;
      act.data    = abi_ser2.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   action_result transfer( account_name from,
                  account_name to,
                  asset        quantity,
                  string       memo ) {
      return push_token_action( from, N(transfer), mvo()
           ( "from", from)
           ( "to", to)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result issue( account_name issuer, asset quantity, string memo ) {
      return push_token_action( issuer, N(issue), mvo()
           ( "to", issuer)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result create_token_contract( account_name issuer,
                         asset        maximum_supply ) {

      return push_token_action( N(eosio.token), N(create), mvo()
           ( "issuer", issuer)
           ( "maximum_supply", maximum_supply)
      );
   }



   action_result create( account_name owner,
                         account_name issuer,
                         asset        maximum_supply ) {
      return push_action( owner, N(create), mvo()
           ( "issuer", issuer)
           ( "maximum_supply", maximum_supply)
      );
   }
   
   action_result setsalefee( account_name actor,
                             uint32_t     fee ) {
      return push_action( actor, N(setsalefee), mvo()
           ( "fee", fee)
      );
   }

   action_result withdraw( account_name owner,
                           asset        amount ) {
      return push_action( owner, N(withdraw), mvo()
           ( "owner", owner)
           ( "amount", amount)
      );
   }

   action_result setsym( account_name actor, 
                         uint32_t symlen, 
                         asset price, 
                         asset floor, 
                         uint32_t increase_threshold, 
                         uint32_t decrease_threshold, 
                         uint32_t window ) {
      return push_action( actor, N(setsym), mvo()
           ( "symlen", symlen )
           ( "price", price )
           ( "floor", floor )
           ( "increase_threshold", increase_threshold)
           ( "decrease_threshold", decrease_threshold)
           ( "window", window )
      );
   }

   action_result setowner( account_name owner,
                           account_name to,
                           symbol_code  sym,
                           string       memo ) {
      return push_action( owner, N(setowner), mvo()
           ( "to", to)
           ( "sym", sym)
           ( "memo", memo)
      );
   }

   action_result purchase( account_name buyer,
                           const symbol_code newsymbol,
                           asset amount    ) {
      return push_action( buyer, N(purchase), mvo()
           ( "buyer", buyer )
           ( "newsymbol", newsymbol )
           ( "amount", amount )
      );
   }

   action_result buysymbol( account_name       buyer,
                            const symbol_code& symbol ) {
      return push_action( buyer, N(buysymbol), mvo()
           ( "buyer", buyer )
           ( "symbol", symbol )
      );
   }

   action_result sellsymbol( account_name       seller,
                             const symbol_code& symbol,
                             asset              price ) {
      return push_action( seller, N(sellsymbol), mvo()
           ( "symbol", symbol )
           ( "price", price )
      );
   }


   void check_wallet_balance(name account, std::string balance) {
      auto bob_balance = get_account_wallet(account, "4,EOS");

      REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
         ("balance", balance)
      );
   }

   void check_credit_balance(name account, std::string balance) {
      auto account_balance = get_account_credit(account);

      REQUIRE_MATCHING_OBJECT( account_balance, mvo()
         ("account", account)
         ("balance", balance)
      );
   }

   abi_serializer abi_ser;
   abi_serializer abi_ser2;

};

BOOST_AUTO_TEST_SUITE(eosio_symbol_tests)


symbol_code sc(std::string symbol_code) {
   return asset::from_string("100.0000 " + symbol_code).get_symbol().to_symbol_code();
}


BOOST_FIXTURE_TEST_CASE( will_throw_on_bad_params , eosio_symbol_test ) try{
   
   BOOST_REQUIRE_EQUAL( error("missing authority of eosio.symbol"),
                        setsym( N(bob), 3, asset::from_string("100.0000 EOS"), asset::from_string("90.0000 EOS"), 24, 22, 86400 ) );
   
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("invalid token symbol"),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 MATE"), asset::from_string("90.0000 EOS"), 24, 22, 86400 ) );
   
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("window must be above 0 seconds"),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 EOS"), asset::from_string("90.0000 EOS"), 24, 22, 0 ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("floor and price must be equal or above 0"),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 EOS"), asset::from_string("-90.0000 EOS"), 24, 22, 86400 ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("increase threshold must be greater"),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 EOS"), asset::from_string("90.0000 EOS"), 21, 22, 86400 ) );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( cannot_buy_symbol_without_balance, eosio_symbol_test ) try {

   BOOST_REQUIRE_EQUAL( success(),
                        setsym( N(eosio.symbol), 3, asset::from_string("50.0000 EOS"), asset::from_string("40.0000 EOS"), 6, 4, 86400 ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("account not found"),
                        purchase( N(alice), sc("CAT"), asset::from_string("100.0000 EOS") ) );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( can_add_symbol_as_admin, eosio_symbol_test ) try {

   // Give bob some tokens
   transfer("eosio.token"_n, "bob"_n, asset::from_string("1000.0000 EOS"), "");
   // check his wallet balance
   check_wallet_balance(N(bob), "1000.0000 EOS");
   transfer("bob"_n, "eosio.symbol"_n, asset::from_string("600.0000 EOS"), "");
   // confirm wallet balance decreased
   check_wallet_balance(N(bob), "400.0000 EOS");
   // confirm credit balance increased to amount transferred
   check_credit_balance(N(bob), "600.0000 EOS");

   uint32_t a_day = 60 * 60 * 24;
   BOOST_REQUIRE_EQUAL( success(),
                        setsym( N(eosio.symbol), 3, asset::from_string("50.0000 EOS"), asset::from_string("40.0000 EOS"), 6, 4, a_day ) );

   auto initial_config = get_sym_config(3);

// produce_block( fc::days(1) );
// produce_blocks(1);

   REQUIRE_MATCHING_OBJECT( initial_config, mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 0)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );

   // eosio.symbol can make a purchase for free and without counting towards mint
   BOOST_REQUIRE_EQUAL( success(),
                        purchase( N(eosio.symbol), sc("DOG"), asset::from_string("100.0000 EOS") ) );

   auto after_config = get_sym_config(3);

   REQUIRE_MATCHING_OBJECT( after_config, mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 0)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("symbol already exists"),
                        purchase( N(eosio.symbol), sc("DOG"), asset::from_string("100.0000 EOS") ) );


} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( cannot_buy_not_for_sale, eosio_symbol_test ) try {

 BOOST_REQUIRE_EQUAL( wasm_assert_msg("sym len not for sale"),
                        purchase( N(eosio.symbol), sc("DOG"), asset::from_string("100.0000 EOS") ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( cannot_buy_with_insufficient_price, eosio_symbol_test ) try {

   transfer("eosio.token"_n, "bob"_n, asset::from_string("1000.0000 EOS"), "");
   transfer("bob"_n, "eosio.symbol"_n, asset::from_string("1000.0000 EOS"), "");
   check_credit_balance(N(bob), "1000.0000 EOS");

   uint32_t a_day = 60 * 60 * 24;
   BOOST_REQUIRE_EQUAL( success(),
                        setsym( N(eosio.symbol), 3, asset::from_string("50.0000 EOS"), asset::from_string("40.0000 EOS"), 6, 4, a_day ) );


   produce_block( fc::hours(4) );

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 0)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("insufficient purchase amount"),
                        purchase( "bob"_n, sc("DOG"), asset::from_string("49.0000 EOS") ) );

   check_credit_balance("bob"_n, "1000.0000 EOS");

   produce_block( fc::hours(4) );

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("DOG"), asset::from_string("50.0000 EOS") ) );
   check_credit_balance("bob"_n, "950.0000 EOS");
   check_credit_balance("eosio.symbol"_n, "50.0000 EOS");


   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 1)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("CAT"), asset::from_string("830.0000 EOS") ) );
   check_credit_balance("bob"_n, "900.0000 EOS");
   check_credit_balance("eosio.symbol"_n, "100.0000 EOS");



   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 2)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("BTC"), asset::from_string("50.0000 EOS") ) );
   check_credit_balance("bob"_n, "850.0000 EOS");
   check_credit_balance("eosio.symbol"_n, "150.0000 EOS");

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "50.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 3)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );



   produce_block( fc::hours(18) );

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("XYZ"), asset::from_string("50.0000 EOS") ) );
   check_credit_balance("bob"_n, "805.0000 EOS");
   check_credit_balance("eosio.symbol"_n, "195.0000 EOS");

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "45.0000 EOS")
      ("floor", "40.0000 EOS")
      ("window_start", "2020-01-02T02:00:09.000")
      ("window_duration", 86400)
      ("minted_in_window", 1)
      ("increase_threshold", 6)
      ("decrease_threshold", 4)
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( will_lower_price_after_satisfied_stale_window, eosio_symbol_test ) try {

   // This is to check in case the last updated window interval was satisfied, however, several windows has been missed
   transfer("eosio.token"_n, "bob"_n, asset::from_string("1000.0000 EOS"), "");
   transfer("bob"_n, "eosio.symbol"_n, asset::from_string("1000.0000 EOS"), "");
   check_credit_balance(N(bob), "1000.0000 EOS");

   uint32_t a_day = 60 * 60 * 24;
   BOOST_REQUIRE_EQUAL( success(),
                        setsym( N(eosio.symbol), 3, asset::from_string("32.5000 EOS"), asset::from_string("1.0000 EOS"), 4, 2, a_day ) );

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "32.5000 EOS")
      ("floor", "1.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 0)
      ("increase_threshold", 4)
      ("decrease_threshold", 2)
   );

   produce_block( fc::hours(4) );

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("CAT"), asset::from_string("32.5000 EOS") ) );

   check_credit_balance("bob"_n, "967.5000 EOS");
   check_credit_balance("eosio.symbol"_n, "32.5000 EOS");

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("DOG"), asset::from_string("32.5000 EOS") ) );

   check_credit_balance("bob"_n, "935.0000 EOS");
   check_credit_balance("eosio.symbol"_n, "65.0000 EOS");

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("ABC"), asset::from_string("32.5000 EOS") ) );

   check_credit_balance("bob"_n, "902.5000 EOS");
   check_credit_balance("eosio.symbol"_n, "97.5000 EOS");

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "32.5000 EOS")
      ("floor", "1.0000 EOS")
      ("window_start", "2020-01-01T00:00:07.000")
      ("window_duration", 86400)
      ("minted_in_window", 3)
      ("increase_threshold", 4)
      ("decrease_threshold", 2)
   );

   produce_block( fc::days(2) );

   // expect the first window to have been satisified but receive a discount on the next window

   BOOST_REQUIRE_EQUAL( success(),
                        purchase( "bob"_n, sc("XYZ"), asset::from_string("32.5000 EOS") ) );

   REQUIRE_MATCHING_OBJECT( get_sym_config(3), mvo()
      ("symbol_length", 3)
      ("price", "29.2500 EOS")
      ("floor", "1.0000 EOS")
      ("window_start", "2020-01-03T04:00:09.000")
      ("window_duration", 86400)
      ("minted_in_window", 1)
      ("increase_threshold", 4)
      ("decrease_threshold", 2)
   );

   // check_credit_balance("bob"_n, "902.5000 EOS");
   // check_credit_balance("eosio.symbol"_n, "97.5000 EOS");



} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
