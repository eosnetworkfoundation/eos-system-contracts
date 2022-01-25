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
   }

   fc::variant get_account_wallet( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(eosio.token), acc, N(accounts), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser2.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
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




   abi_serializer abi_ser;
   abi_serializer abi_ser2;

};

BOOST_AUTO_TEST_SUITE(eosio_symbol_tests)


symbol_code sc(std::string symbol_code) {
   return asset::from_string("100.0000 " + symbol_code).get_symbol().to_symbol_code();
}

BOOST_FIXTURE_TEST_CASE( can_add_symbol_as_admin, eosio_symbol_test ) try {


   create_token_contract("eosio.token"_n, asset::from_string("10000000.0000 EOS"));
   issue("eosio.token"_n, asset::from_string("10000.0000 EOS"), "");
   transfer("eosio.token"_n, "bob"_n, asset::from_string("1000.0000 EOS"), "");

   auto bob_balance = get_account_wallet(N(bob), "4,EOS");

   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "1000.0000 EOS")
   );

   cout << bob_balance << "\n";


   BOOST_REQUIRE_EQUAL( error("missing authority of eosio.symbol"),
                        setsym( N(bob), 3, asset::from_string("100.0000 EOS"), asset::from_string("90.0000 EOS"), 24, 22, 300 ) );
   
   BOOST_REQUIRE_EQUAL( success(),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 EOS"), asset::from_string("90.0000 EOS"), 24, 22, 300 ) );


   BOOST_REQUIRE_EQUAL( wasm_assert_msg("invalid token symbol"),
                        setsym( N(eosio.symbol), 3, asset::from_string("100.0000 MATE"), asset::from_string("90.0000 EOS"), 24, 22, 300 ) );
   
   auto initial_config = get_sym_config(3);
   cout << "Current table" << "\n";
   cout << initial_config << "\n";

// produce_block( fc::days(1) );
// produce_blocks(1);

   REQUIRE_MATCHING_OBJECT( initial_config, mvo()
      ("symbol_length", 3)
      ("price", "100.0000 EOS")
      ("floor", "90.0000 EOS")
      ("window_start", "2020-01-01T00:00:06.500")
      ("window_duration", 300)
      ("minted_in_window", 0)
      ("increase_threshold", 24)
      ("decrease_threshold", 22)
   );

   // random account cannot attempt a purchase without an pre-existing balance
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("account not found"),
                        purchase( N(bob), sc("CAT"), asset::from_string("100.0000 EOS") ) );

   // eosio.symbol can make a purchase for free and without counting towards mint
   BOOST_REQUIRE_EQUAL( success(),
                        purchase( N(eosio.symbol), sc("DOG"), asset::from_string("100.0000 EOS") ) );

   auto after_config = get_sym_config(3);

   REQUIRE_MATCHING_OBJECT( after_config, mvo()
      ("symbol_length", 3)
      ("price", "100.0000 EOS")
      ("floor", "90.0000 EOS")
      ("window_start", "2020-01-01T00:00:06.500")
      ("window_duration", 300)
      ("minted_in_window", 0)
      ("increase_threshold", 24)
      ("decrease_threshold", 22)
   );


} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
