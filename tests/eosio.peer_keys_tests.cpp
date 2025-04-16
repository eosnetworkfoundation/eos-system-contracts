#include <chrono>
#include <iostream>
#include "eosio.system_tester.hpp"

#include <boost/test/unit_test.hpp>

using namespace eosio_system;

struct v0_data {
   std::optional<public_key_type> pubkey;
};

FC_REFLECT(v0_data, (pubkey))

// ------------------------------------------------------------
// below definitions from eosio::chain should match those in
// ../contracts/eosio.system/include/eosio.system/peer_keys.hpp
// ------------------------------------------------------------
using peerkeys_t        = eosio::chain::peerkeys_t;
using getpeerkeys_res_t = eosio::chain::getpeerkeys_res_t;

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

   getpeerkeys_res_t getpeerkeys() {
      auto   perms = vector<permission_level>{};
      action act(perms, config::system_account_name, "getpeerkeys"_n, {});
      signed_transaction trx;

      trx.actions.emplace_back(std::move(act));
      set_transaction_headers(trx);

      transaction_trace_ptr trace = push_transaction(trx, fc::time_point::maximum(), DEFAULT_BILLED_CPU_TIME_US,
                                                     false, transaction_metadata::trx_type::read_only);

      getpeerkeys_res_t res;
      assert(!trace->action_traces.empty());
      const auto& act_trace = trace->action_traces[0];
      const auto& retval    = act_trace.return_value;

      fc::datastream<const char*> ds(retval.data(), retval.size());
      fc::raw::unpack(ds, res);
      return res;
   }

   struct ProducerSpec {
      std::string name;
      uint32_t    percent_of_stake; // 0 to 1000
      bool        key = false;
   };

   struct PeerKey {
      std::string         name;
      std::optional<bool> key_present_and_valid; // optional empty if key not returned

      bool operator==(const PeerKey&) const = default;
      friend std::ostream& operator << (std::ostream& os, const PeerKey& k) {
         os << "{\"" << k.name << "\""
            << (k.key_present_and_valid ? (*k.key_present_and_valid ? ", true" : ", false") : "") << "}";
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
            continue;                        // doesn't get any vote
         assert(p.percent_of_stake <= 10000); // this is a per 10,000
         auto prod_name      = name(p.name);
         auto stake_quantity = int64_t(total_voting_power * ((double)p.percent_of_stake / 10000.0));
         auto amt            = asset(stake_quantity, symbol(CORE_SYM));
         auto fund_amt       = asset(stake_quantity * 4, symbol(CORE_SYM));

         transfer("eosio"_n, prod_name, fund_amt, "eosio"_n);
         BOOST_REQUIRE_EQUAL(success(), stake(prod_name, amt, amt));
         
         push_action(prod_name, "voteproducer"_n,
                     mvo()("voter", p.name)("proxy", name(0).to_string())("producers", vector<name>{prod_name}));

         // register a key if requested
         if (p.key)
            BOOST_REQUIRE_EQUAL(success(), regpeerkey(prod_name, get_public_key(prod_name)));

         // remove producer if requested (so it is not active)
         if (p.name[0] == 'p')
            BOOST_REQUIRE_EQUAL(success(), push_action("eosio"_n, "rmvproducer"_n, mvo()("producer", prod_name)));
      }

      produce_block();
      produce_block(fc::seconds(1000));

      // run the `getpeerkeys` action, and verify we get the expected result
      // -------------------------------------------------------------------
      auto peerkeys = getpeerkeys();
      check_out actual;
      std::transform(peerkeys.begin(), peerkeys.end(), std::back_inserter(actual),
                     [](auto& k) {
                        auto p {k.producer_name};
                        if (!k.peer_key)
                           return PeerKey{p.to_string()};
                        bool key_present { get_public_key(p) == *k.peer_key };
                        return PeerKey{p.to_string(), key_present};
                     });

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
   //     - producers whose name starts with 'p' are paused, 
   //     - second number is per 10 thousand of stake (total should be less than 10000)
   //     - third entry (if present} is a bool specifying if we should register a peer key for this producer
   //
   // check_out:  vector mirroring the output of `getpeerkeys`_n
   //     - if producer name is followed by `true`, it means that `getpeerkeys` returned a correct
   //       public key for this producer
   // ------------------------------------------------------------------------------------------------------

   // first, let's purposefully specify a wrong expected result, just to make sure that the `pkt().check()`
   // function detects errors and works as expected
   // -----------------------------------------------------------------------------------------------------
   auto res = pkt().check(
      pkt::check_in {{"a1", 9}, {"a2", 7, true}, {"a3", 6}, {"a4", 1}, {"p1", 0}, {"p2", 3}, {"p3", 8}},
      pkt::check_out{{{"a1"}, {"p1"}, {"a2"}, {"a3"}, {"p1"}}});

   BOOST_REQUIRE(!!res && *res == R"(expected: {{"a1"}, {"p1"}, {"a2"}, {"a3"}, {"p1"}}; )"
                                  R"(actual: {{"a1"}, {"p3"}, {"a2", true}, {"a3"}, {"p2"}, {"a4"}})");

   // Now do the same test specifying the correct result
   // --------------------------------------------------
   res = pkt().check(
      pkt::check_in {{"a1", 9}, {"a2", 7, true}, {"a3", 6}, {"a4", 1}, {"p1", 0}, {"p2", 3}, {"p3", 8}},
      pkt::check_out{{{"a1"}, {"p3"}, {"a2", true}, {"a3"}, {"p2"}, {"a4"}}});

   BOOST_REQUIRE_MESSAGE(!res, *res);

   // -----------------------
   // tests with 21 producers
   // -----------------------

   // all producers active
   // --------------------
   auto in = pkt::check_in{
      {"a",  79}, {"b",  78}, {"c",  77}, {"d",  76}, {"e",  75}, {"f",  74}, {"g",  73}, {"h",  72}, {"j",  71}, {"k",  70},
      {"aa", 69}, {"ab", 68}, {"ac", 67}, {"ad", 66}, {"ae", 65}, {"af", 64}, {"ag", 63}, {"ah", 62}, {"aj", 61}, {"ak", 60},
      {"ba", 59}, {"bb", 58}, {"bc", 57}, {"bd", 56}, {"be", 55}, {"bf", 54}, {"bg", 53}, {"bh", 52}, {"bj", 51}, {"bk", 50},
      {"ca", 49}, {"cb", 48}, {"cc", 47}, {"cd", 46}, {"ce", 45}, {"cf", 44}, {"cg", 43}, {"ch", 42}, {"cj", 41}, {"ck", 40},
      {"da", 39}, {"db", 38}, {"dc", 37}, {"dd", 36}, {"de", 35}, {"df", 34}, {"dg", 33}, {"dh", 32}, {"dj", 31}, {"dk", 30},
      {"ea", 29}, {"eb", 28}, {"ec", 27}, {"ed", 26}, {"ee", 25}, {"ef", 24}, {"eg", 23}, {"eh", 22}, {"ej", 21}, {"ek", 20}};

   auto out = pkt::check_out{{
      {"a"},  {"b"},  {"c"},  {"d"},  {"e"},  {"f"},  {"g"},  {"h"},  {"j"},  {"k"},
      {"aa"}, {"ab"}, {"ac"}, {"ad"}, {"ae"}, {"af"}, {"ag"}, {"ah"}, {"aj"}, {"ak"},
      {"ba"}, {"bb"}, {"bc"}, {"bd"}, {"be"}, {"bf"}, {"bg"}, {"bh"}, {"bj"}, {"bk"},
      {"ca"}, {"cb"}, {"cc"}, {"cd"}, {"ce"}, {"cf"}, {"cg"}, {"ch"}, {"cj"}, {"ck"},
      {"da"}, {"db"}, {"dc"}, {"dd"}, {"de"}, {"df"}, {"dg"}, {"dh"}, {"dj"}, {"dk"}}};

   res = pkt().check(in, out);
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // producers whose name end with 'c' non-active
   // --------------------------------------------
   in = pkt::check_in{
      {"a",  79}, {"b",  78}, {"pc",  77}, {"d",  76}, {"e",  75}, {"f",  74}, {"g",  73}, {"h",  72}, {"j",  71}, {"k",  70},
      {"aa", 69}, {"ab", 68}, {"pac", 67}, {"ad", 66}, {"ae", 65}, {"af", 64}, {"ag", 63}, {"ah", 62}, {"aj", 61}, {"ak", 60},
      {"ba", 59}, {"bb", 58}, {"pbc", 57}, {"bd", 56}, {"be", 55}, {"bf", 54}, {"bg", 53}, {"bh", 52}, {"bj", 51}, {"bk", 50},
      {"ca", 49}, {"cb", 48}, {"pcc", 47}, {"cd", 46}, {"ce", 45}, {"cf", 44}, {"cg", 43}, {"ch", 42}, {"cj", 41}, {"ck", 40},
      {"da", 39}, {"db", 38}, {"pdc", 37}, {"dd", 36}, {"de", 35}, {"df", 34}, {"dg", 33}, {"dh", 32}, {"dj", 31}, {"dk", 30},
      {"ea", 29}, {"eb", 28}, {"pec", 27}, {"ed", 26}, {"ee", 25}, {"ef", 24}, {"eg", 23}, {"eh", 22}, {"ej", 21}, {"ek", 20}};

   out = pkt::check_out{{
      {"a"},  {"b"},  {"pc"},  {"d"},  {"e"},  {"f"},  {"g"},  {"h"},  {"j"},  {"k"},
      {"aa"}, {"ab"}, {"pac"}, {"ad"}, {"ae"}, {"af"}, {"ag"}, {"ah"}, {"aj"}, {"ak"},
      {"ba"}, {"bb"}, {"pbc"}, {"bd"}, {"be"}, {"bf"}, {"bg"}, {"bh"}, {"bj"}, {"bk"},
      {"ca"}, {"cb"}, {"pcc"}, {"cd"}, {"ce"}, {"cf"}, {"cg"}, {"ch"}, {"cj"}, {"ck"},
      {"da"}, {"db"}, {"pdc"}, {"dd"}, {"de"}, {"df"}, {"dg"}, {"dh"}, {"dj"}, {"dk"}}};

   res = pkt().check(in, out);
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // producers whose name end with 'c' non-active, producers whose name end with 'd'  with a peer key
   // ------------------------------------------------------------------------------------------------
   in = pkt::check_in{
      {"a",  79}, {"b",  78}, {"pc",  77}, {"d",  76, true}, {"e",  75}, {"f",  74}, {"g",  73}, {"h",  72}, {"j",  71}, {"k",  70},
      {"aa", 69}, {"ab", 68}, {"pac", 67}, {"ad", 66, true}, {"ae", 65}, {"af", 64}, {"ag", 63}, {"ah", 62}, {"aj", 61}, {"ak", 60},
      {"ba", 59}, {"bb", 58}, {"pbc", 57}, {"bd", 56, true}, {"be", 55}, {"bf", 54}, {"bg", 53}, {"bh", 52}, {"bj", 51}, {"bk", 50},
      {"ca", 49}, {"cb", 48}, {"pcc", 47}, {"cd", 46, true}, {"ce", 45}, {"cf", 44}, {"cg", 43}, {"ch", 42}, {"cj", 41}, {"ck", 40},
      {"da", 39}, {"db", 38}, {"pdc", 37}, {"dd", 36, true}, {"de", 35}, {"df", 34}, {"dg", 33}, {"dh", 32}, {"dj", 31}, {"dk", 30},
      {"ea", 29}, {"eb", 28}, {"pec", 27}, {"ed", 26, true}, {"ee", 25}, {"ef", 24}, {"eg", 23}, {"eh", 22}, {"ej", 21}, {"ek", 20}};

   out = pkt::check_out{{
      {"a"},  {"b"},  {"pc"},  {"d",  true}, {"e"},  {"f"},  {"g"},  {"h"},  {"j"},  {"k"},
      {"aa"}, {"ab"}, {"pac"}, {"ad", true}, {"ae"}, {"af"}, {"ag"}, {"ah"}, {"aj"}, {"ak"},
      {"ba"}, {"bb"}, {"pbc"}, {"bd", true}, {"be"}, {"bf"}, {"bg"}, {"bh"}, {"bj"}, {"bk"},
      {"ca"}, {"cb"}, {"pcc"}, {"cd", true}, {"ce"}, {"cf"}, {"cg"}, {"ch"}, {"cj"}, {"ck"},
      {"da"}, {"db"}, {"pdc"}, {"dd", true}, {"de"}, {"df"}, {"dg"}, {"dh"}, {"dj"}, {"dk"}}};

   res = pkt().check(in, out);
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // edge case - no producers
   // ------------------------
   res = pkt().check(pkt::check_in {}, pkt::check_out{});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // edge case - only producers with no votes
   // ----------------------------------------
   res = pkt().check(pkt::check_in {{"a1", 0}, {"a2", 0}}, pkt::check_out{});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // edge case - only paused producers with no votes
   // -----------------------------------------------
   res = pkt().check(pkt::check_in {{"p1", 0}, {"p2", 0}}, pkt::check_out{});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // no active producer
   // ------------------
   res = pkt().check(
      pkt::check_in {{"p1", 9}, {"p2", 7, true}, {"p3", 6}},
      pkt::check_out{{{"p1"}, {"p2", true}, {"p3"}}});

   BOOST_REQUIRE_MESSAGE(!res, *res);

   // one active
   // ----------
   res = pkt().check(pkt::check_in{{"a1", 9}}, pkt::check_out{{{"a1"}}});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // one paused
   // ----------
   res = pkt().check(pkt::check_in{{"p1", 9}}, pkt::check_out{{{"p1"}}});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // one active, one paused, paused with more votes
   // ----------------------------------------------
   res = pkt().check(pkt::check_in{{"a1", 9}, {"p1", 20}}, pkt::check_out{{{"p1"}, {"a1"}}});
   BOOST_REQUIRE_MESSAGE(!res, *res);   

   // one active, one paused, active with more votes
   // ----------------------------------------------
   res = pkt().check(pkt::check_in{{"a1", 20}, {"p1", 10}}, pkt::check_out{{{"a1"}, {"p1"}}});
   BOOST_REQUIRE_MESSAGE(!res, *res);

   // check that producers with no votes are not returned
   // ---------------------------------------------------
   res = pkt().check(pkt::check_in {{"a1", 0}, {"a2", 10}, {"p1", 0}, {"p2", 10}}, pkt::check_out{{{"a2"}, {"p2"}}});
   BOOST_REQUIRE_MESSAGE(!res, *res);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()