#include <chrono>
#include <iostream>
#include "eosio.system_tester.hpp"

#include <boost/test/unit_test.hpp>

using namespace eosio_system;

struct v0_data {
   std::optional<public_key_type> pubkey;
};

FC_REFLECT(v0_data, (pubkey))

#if 0  // enable when spring PR #1276 is merged
// ------------------------------------------------------------
// below definitions from eosio::chain should match those in
// ../contracts/eosio.system/include/eosio.system/peer_keys.hpp
// ------------------------------------------------------------
using peerkeys_t        = eosio::chain::peerkeys_t;
using getpeerkeys_res_t = eosio::chain::getpeerkeys_res_t;
#else

   struct peerkeys_t {
      name                           producer_name;
      std::optional<public_key_type> peer_key;
   };
using _getpeerkeys_res_t = std::vector<::peerkeys_t>;

FC_REFLECT(::peerkeys_t, (producer_name)(peer_key))
#endif

BOOST_AUTO_TEST_SUITE(peer_keys_tests)



// ----------------------------------------------------------------------------------------------------
struct peer_keys_tester : eosio_system_tester {

   std::optional<fc::crypto::public_key> get_peer_key_info(const name& n) {
      vector<char> row_data = get_row_by_id( config::system_account_name, config::system_account_name, "peerkeys"_n, n.to_uint64_t() );
      if (row_data.empty())
         return {};
      fc::datastream<const char*> ds(row_data.data(), row_data.size());

      name                  row_name;
      block_timestamp_type  row_block_timestamp;
      std::variant<v0_data> v;

      fc::raw::unpack(ds, row_name);
      fc::raw::unpack(ds, v);
      auto& data = std::get<v0_data>(v);
      if (data.pubkey)
         return *data.pubkey;
      return {};
   }

   typename base_tester::action_result _push_action(action&& act, uint64_t authorizer, bool produce) {
      signed_transaction trx;
      if (authorizer) {
         act.authorization = vector<permission_level>{{account_name(authorizer), config::active_name}};
      }
      trx.actions.emplace_back(std::move(act));
      set_transaction_headers(trx);
      if (authorizer) {
         trx.sign(get_private_key(account_name(authorizer), "active"), control->get_chain_id());
      }
      try {
         push_transaction(trx);
      } catch (const fc::exception& ex) {
         edump((ex.to_detail_string()));
         return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
         //return error(ex.to_detail_string());
      }
      if (produce) {
         produce_block();
         BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
      }
      return success();
   }

   action_result regpeerkey( const name& proposer, const fc::crypto::public_key& key  ) {
      return push_action(proposer, "regpeerkey"_n, mvo()("proposer_finalizer_name", proposer)("key", key));
   }

   action_result delpeerkey( const name& proposer, const fc::crypto::public_key& key ) {
      return push_action(proposer, "delpeerkey"_n, mvo()("proposer_finalizer_name", proposer)("key", key));
   }

   _getpeerkeys_res_t getpeerkeys() {
      auto               perms = vector<permission_level>{};
      action             act(perms, config::system_account_name, "getpeerkeys"_n, {});
      signed_transaction trx;

      trx.actions.emplace_back(std::move(act));
      set_transaction_headers(trx);

      transaction_trace_ptr trace = push_transaction(trx, fc::time_point::maximum(), DEFAULT_BILLED_CPU_TIME_US,
                                                     false, transaction_metadata::trx_type::read_only);

      _getpeerkeys_res_t res;
      assert(!trace->action_traces.empty());
      const auto& act_trace = trace->action_traces[0];
      const auto& retval    = act_trace.return_value;

      fc::datastream<const char*> ds(retval.data(), retval.size());
      fc::raw::unpack(ds, res);
      return res;
   }
};

BOOST_FIXTURE_TEST_CASE(peer_keys_test, peer_keys_tester) try {
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n };

   const account_name alice     = accounts[0];
   const account_name bob       = accounts[1];
   const auto         alice_key = get_public_key(alice);
   const auto         bob_key   = get_public_key(bob);

   create_accounts_with_resources(accounts);

   // store Alice's peer key
   // ----------------------
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(alice, alice_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == alice_key);

   // replace Alices's key with a new one
   // -----------------------------------
   auto new_key = get_public_key("alice.new"_n);
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(alice, new_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == new_key);

   // Delete Alices's key
   // -------------------
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: Current key does not match the provided one"),
                       delpeerkey(alice, alice_key)); // not the right key, should be `new_key`
   BOOST_REQUIRE_EQUAL(success(), delpeerkey(alice, new_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == std::optional<fc::crypto::public_key>{});

   // But we can't delete it twice!
   // -----------------------------
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: Key not present for name: alice"),
                       delpeerkey(alice, new_key));

   // store Alice's and Bob's peer keys
   // ---------------------------------
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(alice, alice_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == alice_key);
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(bob, bob_key));
   BOOST_REQUIRE(get_peer_key_info(bob) == bob_key);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(getpeerkeys_test, peer_keys_tester) try {
   constexpr size_t num_producers = 25;
   auto prod_names = active_and_vote_producers(num_producers);

   for (size_t i=0; i<prod_names.size(); ++i) {
      auto n = prod_names[i];
      if (i % 2 == 0)
         BOOST_REQUIRE_EQUAL(success(), regpeerkey(n, get_public_key(n)));
   }

   auto peerkeys = getpeerkeys();
   BOOST_REQUIRE_EQUAL(peerkeys.size(), num_producers);

   for (size_t i=0; i<prod_names.size(); ++i) {
      auto n = prod_names[i];
      bool found = false;
      for (auto& p : peerkeys) {
         if (p.producer_name == n) {
            found = true;
            if (i % 2 == 0) {
               BOOST_REQUIRE(!!p.peer_key);
               BOOST_REQUIRE_EQUAL(get_public_key(n), *p.peer_key);
            } else {
               BOOST_REQUIRE(!p.peer_key);
            }
         }
      }
      BOOST_REQUIRE(found);
   }
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()