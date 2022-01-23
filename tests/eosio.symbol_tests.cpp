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

      create_accounts( { N(alice), N(bob), N(carol), N(eosio.symbol) } );
      produce_blocks( 2 );

      set_code( N(eosio.symbol), contracts::symbol_wasm() );
      set_abi( N(eosio.symbol), contracts::symbol_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(eosio.symbol) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(eosio.symbol);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
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

   action_result addsym( account_name actor, uint32_t symlen, asset price ) {
      return push_action( actor, N(addsym), mvo()
           ( "symlen", symlen)
           ( "price", price)
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
                           const symbol_code& newsymbol,
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
};

BOOST_AUTO_TEST_SUITE(eosio_symbol_tests)


// fc::variant get_sym( const string& symbolname ) {
//    auto symb = eosio::chain::symbol::from_string(symbolname);
//    auto symbol_code = symb.to_symbol_code().value;
//    vector<char> data = get_row_by_account( N(eosio.symbol), name(symbol_code), N(stat), account_name(symbol_code) );
//    return data.empty() ? fc::variant() : token_abi_ser.binary_to_variant( "currency_stats", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
// }

BOOST_FIXTURE_TEST_CASE( can_add_symbol_as_admin, eosio_symbol_test ) try {

   
   // BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must issue positive quantity" ),
   //                      addsym( N(eosio.symbola), 3, asset::from_string("100.0000 MATE") )
   // );

   BOOST_REQUIRE_EQUAL( success(),
                        addsym( N(bob), 3, asset::from_string("100.0000 MATE") ) );

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
