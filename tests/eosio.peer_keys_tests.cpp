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

   action_result regpeerkey( const name& proposer, const fc::crypto::public_key& key  ) {
      return push_action(proposer, "regpeerkey"_n, mvo()("proposer_finalizer_name", proposer)("key", key));
   }

   action_result delpeerkey( const name& proposer, const fc::crypto::public_key& key ) {
      return push_action(proposer, "delpeerkey"_n, mvo()("proposer_finalizer_name", proposer)("key", key));
   }

   _getpeerkeys_res_t getpeerkeys(uint32_t num_top_producers, uint32_t max_return, uint8_t percent)
   {
      auto   perms = vector<permission_level>{};
      action act(perms, config::system_account_name, "getpeerkeys"_n, {});
      
      string action_type_name = abi_ser.get_action_type("getpeerkeys"_n);
      act.data = abi_ser.variant_to_binary(
         action_type_name, mvo()("num_top_producers", num_top_producers)("max_return", max_return)("percent", percent),
         abi_serializer_max_time);

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

   struct ProducerSpec
   {
      std::string name;
      uint8_t     percent_of_stake; // 0 to 100
      bool        key = false;
   };

   struct PeerKey
   {
      std::string                name;
      std::optional<std::string> key_src;

      bool operator==(const PeerKey&) const = default;
      friend std::ostream& operator << (std::ostream& os, const PeerKey& k) {
         os << "{\"" << k.name << "\"" << (k.key_src ? ", \"" + *k.key_src + "\"" : "") << "}";
         return os;
      }
   };

   using check_in = std::vector<ProducerSpec>;

   struct check_out : public std::vector<PeerKey> {
      check_out() = default;
      check_out(std::vector<PeerKey>&& v) : std::vector<PeerKey>(std::move(v)) {}
      
      friend std::ostream& operator<<(std::ostream& os, const check_out& v)
      {
         os << "{";
         for (size_t i=0; i<v.size(); ++i) {
            os << v[i];
            if (i+1 != v.size())
               os << ", ";
         }
         os << "}";
         return os;
      }
   };

   template <uint32_t num_top_producers, uint32_t num_selected_producers, uint8_t percent>
   std::optional<std::string> check(const check_in& prods, const check_out& expected) {
      // stake more than 15% of total EOS supply to activate chain
      // ---------------------------------------------------------
      auto alice =  "alice1111111"_n;
      transfer("eosio"_n, alice, core_sym::from_string("750000000.0000"), config::system_account_name);
      BOOST_REQUIRE_EQUAL(success(), stake(alice, alice, core_sym::from_string("300000000.0000"),
                                           core_sym::from_string("300000000.0000")));

      // create, setup and register producer accounts
      // --------------------------------------------
      std::vector<name> producers;
      std::transform(prods.begin(), prods.end(), std::back_inserter(producers), [](auto p){ return name(p.name); });
      auto res_amount = core_sym::from_string("1000.0000");
      setup_producer_accounts(producers, res_amount, res_amount, res_amount);
      for (const auto& p: producers) {
         transfer("eosio"_n, p, res_amount, config::system_account_name);
         BOOST_REQUIRE_EQUAL(success(), regproducer(p));
      }

      produce_block();
      produce_block(fc::seconds(1000));

      update_producers_auth();

      // for each producer, fund him, stake his share of the total voting power, and vote for himself.
      // --------------------------------------------------------------------------------------------
      int64_t total_voting_power = 1000000;
      for (const auto& p : prods) {
         if (p.percent_of_stake == 0)
            continue;                       // doesn't get any vote
         assert(p.percent_of_stake <= 100); // this is a percentage
         auto prod_name      = name(p.name);
         auto stake_quantity = int64_t(total_voting_power * ((double)p.percent_of_stake / 100.0));
         auto amt            = asset(stake_quantity, symbol(CORE_SYM));
         auto fund_amt       = asset(stake_quantity * 4, symbol(CORE_SYM));

         transfer("eosio"_n, prod_name, fund_amt, "eosio"_n);
         BOOST_REQUIRE_EQUAL(success(), stake(prod_name, amt, amt));
         
         push_action(prod_name, "voteproducer"_n,
                     mvo()("voter", p.name)("proxy", name(0).to_string())("producers", vector<name>{prod_name}));
      }

      // run the `getpeerkeys` action, and verify we get the expected result
      // -------------------------------------------------------------------
      auto peerkeys = getpeerkeys(num_top_producers, num_selected_producers, percent);
      check_out actual;
      std::transform(peerkeys.begin(), peerkeys.end(), std::back_inserter(actual),
                     [](auto& k) {
                        auto p {k.producer_name};
                        if (!k.peer_key)
                           return PeerKey{p.to_string()};
                        std::string key_src { get_public_key(p) == *k.peer_key ? p.to_string() : "error" };
                        return PeerKey{p.to_string(), key_src};
                     });

      std::cout << "actual:   " << actual << '\n';
      std::cout << "expected: " << expected << '\n';

      if (actual == expected)
         return {};

      // create error string to return
      std::ostringstream oss;
      oss << "expected: " << expected << "; actual: " << actual;

      return oss.str();
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

   auto peerkeys = getpeerkeys(21, 50, 50);
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

BOOST_AUTO_TEST_CASE(getpeerkeys_test2) try {
   using pkt = peer_keys_tester;

   // ------------------------------------------------------------------------------------------------------
   // `pkt().check()` verifies that, given a set of producers (active or not), "getpeerkeys"_n returns the
   // expected result.
   // It returns an optional, empty on success, and containing an error string on failure
   //
   // parameters:
   //
   // check_in:
   //     - producers whose name starts with 'a' are active
   //     - producers whose name starts with 'p' are paused
   //     - second number is percentage of stake (total should be less than 100)
   //     - third entry (if present} is a bool specifying if we should register a peer key for this producer
   //
   // check_out:  vector mirroring the output of `getpeerkeys`_n
   // ------------------------------------------------------------------------------------------------------

   // first, let's purposefully specify a wrong expected result, just to make sure that the `pkt().check()`
   // function detects errors and works as expected
   // -----------------------------------------------------------------------------------------------------
   auto res = pkt().check<3, 5, 50>(
      pkt::check_in {{"a1", 9}, {"a2", 7}, {"a3", 6}, {"a4", 1}, {"p1", 0}, {"p2", 3}, {"p3", 8}},
      pkt::check_out{{{"a1"}, {"p1"}, {"a2"}, {"a3"}, {"p1"}}});

   std::cout << *res <<  '\n';
      
   BOOST_REQUIRE(!!res && *res == std::string(R"(expected: {{"a1"}, {"p1"}, {"a2"}, {"a3"}, {"p1"}}; actual: {{"a1"}, {"p3"}, {"a2"}, {"a3"}})"));

   //BOOST_REQUIRE_MESSAGE(!res, *res);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()