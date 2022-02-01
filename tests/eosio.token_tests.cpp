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

symbol_code sc(std::string symbol_code) {
   return asset::from_string("100.0000 " + symbol_code).get_symbol().to_symbol_code();
}

class eosio_token_tester : public tester {
public:

   eosio_token_tester() {
      produce_blocks( 2 );

      create_accounts( { "alice"_n, "bob"_n, "carol"_n, "eosio.token"_n } );
      produce_blocks( 2 );

      set_code( "eosio.token"_n, contracts::token_wasm() );
      set_abi( "eosio.token"_n, contracts::token_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( "eosio.token"_n );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = "eosio.token"_n;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   fc::variant get_stats( const string& symbolname )
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( "eosio.token"_n, name(symbol_code), "stat"_n, account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "currency_stats", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_account( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( "eosio.token"_n, acc, "accounts"_n, account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   action_result create( account_name issuer,
                         asset        maximum_supply ) {

      return push_action( "eosio.token"_n, "create"_n, mvo()
           ( "issuer", issuer)
           ( "maximum_supply", maximum_supply)
      );
   }

   action_result update( account_name actor,
                         symbol_code sym,
                         bool recall,
                         bool authorize,
                         name authorizer,
                         uint64_t daily_inf_per_limit,
                         uint64_t yearly_inf_per_limit,
                         asset allowed_daily_inflation ) {
      return push_action( actor, N(update), mvo()
         ( "sym", sym)
         ( "recall", recall)
         ( "authorize", authorize)
         ( "authorizer", authorizer)
         ( "daily_inf_per_limit", daily_inf_per_limit)
         ( "yearly_inf_per_limit", yearly_inf_per_limit)
         ( "allowed_daily_inflation", allowed_daily_inflation)
      );
   }

   action_result issue( account_name issuer, asset quantity, string memo ) {
      return push_action( issuer, "issue"_n, mvo()
           ( "to", issuer)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result retire( account_name issuer, asset quantity, string memo ) {
      return push_action( issuer, "retire"_n, mvo()
           ( "quantity", quantity)
           ( "memo", memo)
      );

   }

   action_result transfer( account_name actor,
                           account_name from,
                           account_name to,
                           asset        quantity,
                           string       memo ) {
      return push_action( actor, N(transfer), mvo()
           ( "from", from)
           ( "to", to)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result open( account_name owner,
                       const string& symbolname,
                       account_name ram_payer    ) {
      return push_action( ram_payer, "open"_n, mvo()
           ( "owner", owner )
           ( "symbol", symbolname )
           ( "ram_payer", ram_payer )
      );
   }

   action_result close( account_name owner,
                        const string& symbolname ) {
      return push_action( owner, "close"_n, mvo()
           ( "owner", owner )
           ( "symbol", "0,CERO" )
      );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(eosio_token_tests)

BOOST_FIXTURE_TEST_CASE( create_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000.000 TKN"));
   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "0.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000000)
      ("avg_daily_inflation", 0)
      ("avg_yearly_inflation", 0)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_negative_max_supply, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "max-supply must be positive" ),
      create( "alice"_n, asset::from_string("-1000.000 TKN"))
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( symbol_already_exists, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("100 TKN"));
   auto stats = get_stats("0,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "0 TKN")
      ("max_supply", "100 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 100)
      ("avg_daily_inflation", 0)
      ("avg_yearly_inflation", 0)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "token with symbol already exists" ),
                        create( "alice"_n, asset::from_string("100 TKN"))
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_max_supply, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("4611686018427387903 TKN"));
   auto stats = get_stats("0,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "0 TKN")
      ("max_supply", "4611686018427387903 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 4611686018427387903)
      ("avg_daily_inflation", 0)
      ("avg_yearly_inflation", 0)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );
   produce_blocks(1);

   asset max(10, symbol(SY(0, NKT)));
   share_type amount = 4611686018427387904;
   static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
   static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
   memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

   BOOST_CHECK_EXCEPTION( create( "alice"_n, max) , asset_type_exception, [](const asset_type_exception& e) {
      return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
   });


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_max_decimals, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1.000000000000000000 TKN"));
   auto stats = get_stats("18,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "0.000000000000000000 TKN")
      ("max_supply", "1.000000000000000000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000000000000000000)
      ("avg_daily_inflation", 0)
      ("avg_yearly_inflation", 0)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );
   produce_blocks(1);

   asset max(10, symbol(SY(0, NKT)));
   //1.0000000000000000000 => 0x8ac7230489e80000L
   share_type amount = 0x8ac7230489e80000L;
   static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
   static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
   memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

   BOOST_CHECK_EXCEPTION( create( "alice"_n, max) , asset_type_exception, [](const asset_type_exception& e) {
      return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
   });

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( issue_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000.000 TKN"));
   produce_blocks(1);

   issue( "alice"_n, asset::from_string("500.000 TKN"), "hola" );

   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "500.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000000)
      ("avg_daily_inflation", 500000)
      ("avg_yearly_inflation", 500000)
      ("last_update", "2020-01-01T00:00:05")
      ("authorizer", "")
   );

   auto alice_balance = get_account("alice"_n, "3,TKN");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "500.000 TKN")
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "quantity exceeds available supply" ),
                        issue( "alice"_n, asset::from_string("500.001 TKN"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must issue positive quantity" ),
                        issue( "alice"_n, asset::from_string("-1.000 TKN"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( success(),
                        issue( "alice"_n, asset::from_string("1.000 TKN"), "hola" )
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( retire_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000.000 TKN"));
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( success(), issue( "alice"_n, asset::from_string("500.000 TKN"), "hola" ) );

   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "500.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000000)
      ("avg_daily_inflation", 500000)
      ("avg_yearly_inflation", 500000)
      ("last_update", "2020-01-01T00:00:05")
      ("authorizer", "")
   );

   auto alice_balance = get_account("alice"_n, "3,TKN");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "500.000 TKN")
   );

   BOOST_REQUIRE_EQUAL( success(), retire( "alice"_n, asset::from_string("200.000 TKN"), "hola" ) );
   stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "300.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000000)
      ("avg_daily_inflation", 300000)
      ("avg_yearly_inflation", 300000)
      ("last_update", "2020-01-01T00:00:05")
      ("authorizer", "")
   );
   alice_balance = get_account("alice"_n, "3,TKN");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "300.000 TKN")
   );

   //should fail to retire more than current supply
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( "alice"_n, asset::from_string("500.000 TKN"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(alice), N(bob), asset::from_string("200.000 TKN"), "hola" ) );
   //should fail to retire since tokens are not on the issuer's balance
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( "alice"_n, asset::from_string("300.000 TKN"), "hola" ) );
   //transfer tokens back
   BOOST_REQUIRE_EQUAL( success(), transfer( N(bob), N(bob), N(alice), asset::from_string("200.000 TKN"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), retire( "alice"_n, asset::from_string("300.000 TKN"), "hola" ) );
   stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "0.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation",1000000)
      ("avg_daily_inflation", -7)
      ("avg_yearly_inflation",0)
      ("last_update", "2020-01-01T00:00:07")
      ("authorizer", "")
   );
   alice_balance = get_account("alice"_n, "3,TKN");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "0.000 TKN")
   );

   //trying to retire tokens with zero supply
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( "alice"_n, asset::from_string("1.000 TKN"), "hola" ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( transfer_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000 CERO"));
   produce_blocks(1);

   issue( "alice"_n, asset::from_string("1000 CERO"), "hola" );

   auto stats = get_stats("0,CERO");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "1000 CERO")
      ("max_supply", "1000 CERO")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 1000)
      ("avg_daily_inflation", 1000)
      ("avg_yearly_inflation", 1000)
      ("last_update", "2020-01-01T00:00:05")
      ("authorizer", "")
   );

   auto alice_balance = get_account("alice"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   transfer( N(alice), N(alice), N(bob), asset::from_string("300 CERO"), "hola" );

   alice_balance = get_account("alice"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "700 CERO")
      ("frozen", 0)
      ("whitelist", 1)
   );

   auto bob_balance = get_account("bob"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "300 CERO")
      ("frozen", 0)
      ("whitelist", 1)
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "overdrawn balance" ),
      transfer( N(alice), N(alice), N(bob), asset::from_string("701 CERO"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must transfer positive quantity" ),
      transfer( N(alice), N(alice), N(bob), asset::from_string("-1000 CERO"), "hola" )
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( open_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000 CERO"));

   auto alice_balance = get_account("alice"_n, "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("tokens can only be issued to issuer account"),
                        push_action( "alice"_n, "issue"_n, mvo()
                                     ( "to",       "bob")
                                     ( "quantity", asset::from_string("1000 CERO") )
                                     ( "memo",     "") ) );
   BOOST_REQUIRE_EQUAL( success(), issue( "alice"_n, asset::from_string("1000 CERO"), "issue" ) );

   alice_balance = get_account("alice"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   auto bob_balance = get_account("bob"_n, "0,CERO");
   BOOST_REQUIRE_EQUAL(true, bob_balance.is_null() );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("owner account does not exist"),
                        open( "nonexistent"_n, "0,CERO", "alice"_n ) );
   BOOST_REQUIRE_EQUAL( success(),
                        open( "bob"_n,         "0,CERO", "alice"_n ) );

   bob_balance = get_account("bob"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "0 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(alice), N(bob), asset::from_string("200 CERO"), "hola" ) );

   bob_balance = get_account("bob"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "200 CERO")
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "symbol does not exist" ),
                        open( "carol"_n, "0,INVALID", "alice"_n ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "symbol precision mismatch" ),
                        open( "carol"_n, "1,CERO", "alice"_n ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( close_tests, eosio_token_tester ) try {

   auto token = create( "alice"_n, asset::from_string("1000 CERO"));

   auto alice_balance = get_account("alice"_n, "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

   BOOST_REQUIRE_EQUAL( success(), issue( "alice"_n, asset::from_string("1000 CERO"), "hola" ) );

   alice_balance = get_account("alice"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(alice), N(bob), asset::from_string("1000 CERO"), "hola" ) );

   alice_balance = get_account("alice"_n, "0,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "0 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), close( "alice"_n, "0,CERO" ) );
   alice_balance = get_account("alice"_n, "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( inflation_limiting, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );


   auto alice_balance = get_account(N(alice), "4,MATE");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000.0000 MATE"), "hola" ) );

   alice_balance = get_account(N(alice), "4,MATE");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000.0000 MATE")
   );

  

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "1000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 10000000)
      ("avg_yearly_inflation", 10000000)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );

   produce_block( fc::hours(5) );
   produce_block( fc::minutes(59) );
   produce_block( fc::seconds(59) );
   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 17500000)
      ("avg_yearly_inflation", 19993160)
      ("last_update", "2020-01-01T06:00:04")
      ("authorizer", "")
   );

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "3000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 27500000)
      ("avg_yearly_inflation", 29993160)
      ("last_update", "2020-01-01T06:00:04")
      ("authorizer", "")
   );

   cout << get_stats("4,MATE") << "\n";

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("10000.0000 MATE"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), update( N(alice), 
                                           sc("MATE"),
                                           true,
                                           true,
                                           ""_n,
                                           150000,
                                           150000,
                                           asset::from_string("3000.0000 MATE") ));

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "13000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 150000)
      ("yearly_inf_per_limit", 150000)
      ("allowed_daily_inflation", 30000000)
      ("avg_daily_inflation", 127499697)
      ("avg_yearly_inflation", 129993160)
      ("last_update", "2020-01-01T06:00:05")
      ("authorizer", "")
   );

   // BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1950.0000 MATE"), "hola" ) );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( no_ram_modify_issues, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );
   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("10000.0000 MATE"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), transfer( "alice"_n, "alice"_n, "bob"_n, asset::from_string("50.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_account("bob"_n, "4,MATE"), mvo()
      ("balance", "50.0000 MATE")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( "alice"_n, "bob"_n, "carol"_n, asset::from_string("10.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_account("bob"_n, "4,MATE"), mvo()
      ("balance", "40.0000 MATE")
   );

   REQUIRE_MATCHING_OBJECT( get_account("carol"_n, "4,MATE"), mvo()
      ("balance", "10.0000 MATE")
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( no_transfer_of_owner_issue, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );
   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("10000.0000 MATE"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), transfer( "alice"_n, "alice"_n, "bob"_n, asset::from_string("50.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_account("bob"_n, "4,MATE"), mvo()
      ("balance", "50.0000 MATE")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( "bob"_n, "bob"_n, "carol"_n, asset::from_string("10.0000 MATE"), "hola" ) );
   BOOST_REQUIRE_EQUAL( success(), transfer( "bob"_n, "bob"_n, "alice"_n, asset::from_string("5.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_account("bob"_n, "4,MATE"), mvo()
      ("balance", "35.0000 MATE")
   );

   REQUIRE_MATCHING_OBJECT( get_account("carol"_n, "4,MATE"), mvo()
      ("balance", "10.0000 MATE")
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( cannot_recall_after_disable, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );
   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("10000.0000 MATE"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), transfer( "alice"_n, "alice"_n, "bob"_n, asset::from_string("50.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_account("bob"_n, "4,MATE"), mvo()
      ("balance", "50.0000 MATE")
   );

   BOOST_REQUIRE_EQUAL(success(),
       update("alice"_n,
           sc("MATE"),
           false,
           true,
           ""_n,
           150000,
           150000,
           asset::from_string("3000.0000 MATE")));

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("cannot enable recall once disabled"),
       update(N(alice),
           sc("MATE"),
           true,
           true,
           ""_n,
           150000,
           150000,
           asset::from_string("3000.0000 MATE")));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "invalid authority" ), transfer( "alice"_n, "bob"_n, "carol"_n, asset::from_string("10.0000 MATE"), "hola" ) );
   BOOST_REQUIRE_EQUAL( success(), transfer( "bob"_n, "bob"_n, "carol"_n, asset::from_string("10.0000 MATE"), "hola" ) );
   BOOST_REQUIRE_EQUAL( success(), transfer( "bob"_n, "bob"_n, "alice"_n, asset::from_string("10.0000 MATE"), "hola" ) );


} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( retire_calc, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );


   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("2000.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 20000000)
      ("avg_yearly_inflation", 20000000)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );

   produce_block( fc::hours(5) );
   produce_block( fc::minutes(59) );
   produce_block( fc::seconds(59) );


   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("300.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2300.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 18000000)
      ("avg_yearly_inflation", 22986320)
      ("last_update", "2020-01-01T06:00:04")
      ("authorizer", "")
   );

   BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("300.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 15000000)
      ("avg_yearly_inflation", 19986320)
      ("last_update", "2020-01-01T06:00:04")
      ("authorizer", "")
   );


} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( can_issue_in_negatives, eosio_token_tester ) try {

   BOOST_REQUIRE_EQUAL( success(), create( N(alice), asset::from_string("1000000.0000 MATE")) );


   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("2000.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2000.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 20000000)
      ("avg_yearly_inflation", 20000000)
      ("last_update", "2020-01-01T00:00:04")
      ("authorizer", "")
   );

   produce_block( fc::hours(23) );
   produce_block( fc::minutes(59) );
   produce_block( fc::seconds(59) );


   BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("300.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "1700.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", -3000000)
      ("avg_yearly_inflation", 16945220)
      ("last_update", "2020-01-02T00:00:04")
      ("authorizer", "")
   );

   produce_block( fc::hours(12) );
   // produce_block( fc::minutes(59) );
   // produce_block( fc::seconds(59) );

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("50.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "1750.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", -1000000)
      ("avg_yearly_inflation", 17422021)
      ("last_update", "2020-01-02T12:00:04")
      ("authorizer", "")
   );

   produce_block( fc::hours(2) );
   produce_block( fc::minutes(59) );
   produce_block( fc::seconds(59) );

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("470.0000 MATE"), "hola" ) );

   REQUIRE_MATCHING_OBJECT( get_stats("4,MATE"), mvo()
      ("supply", "2220.0000 MATE")
      ("max_supply", "1000000.0000 MATE")
      ("issuer", "alice")
      ("recall", 1)
      ("authorize", 1)
      ("daily_inf_per_limit", 10000000000000000000)
      ("yearly_inf_per_limit", 10000000000000000000)
      ("allowed_daily_inflation", 10000000000)
      ("avg_daily_inflation", 3825000)
      ("avg_yearly_inflation", 22116062)
      ("last_update", "2020-01-02T15:00:04")
      ("authorizer", "")
   );


} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()



