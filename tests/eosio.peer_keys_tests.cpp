#include <chrono>
#include <iostream>
#include "eosio.system_tester.hpp"

#if __has_include(<eosio/chain/peer_keys_db.hpp>)
   #include <eosio/chain/peer_keys_db.hpp>
   #define _has_peer_keys_db
#endif

#include <boost/test/unit_test.hpp>

using namespace eosio_system;

#ifdef _has_peer_keys_db
using v0_data = eosio::chain::peer_keys_db_t::v0_data;
#else
struct v0_data {
   std::optional<public_key_type> pubkey;
};

FC_REFLECT(v0_data, (pubkey))
#endif

BOOST_AUTO_TEST_SUITE(peer_keys_tests)


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
      fc::raw::unpack(ds, row_block_timestamp);
      fc::raw::unpack(ds, v);
      auto& data = std::get<v0_data>(v);
      if (data.pubkey)
         return *data.pubkey;
      return {};
   }

#ifdef _has_peer_keys_db
   size_t update_peer_keys() {
      peer_keys_db.set_active(true);
      return peer_keys_db.update_peer_keys(*control, control->head().timestamp());
   }

   std::optional<public_key_type> get_peer_key(name n) const {
      return peer_keys_db.get_peer_key(n);
   }

   size_t peer_key_map_size() const {
      return peer_keys_db.size();
   }
#endif

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

#ifdef _has_peer_keys_db
   peer_keys_db_t peer_keys_db;
#endif
};

BOOST_FIXTURE_TEST_CASE(peer_keys_test, peer_keys_tester) try {
   const std::vector<account_name> accounts = { "alice"_n, "bob"_n };

   const account_name alice     = accounts[0];
   const account_name bob       = accounts[1];
   const auto         alice_key = get_public_key(alice);
   const auto         bob_key   = get_public_key(bob);

   create_accounts_with_resources(accounts);

#ifdef _has_peer_keys_db
   // check `update_peer_keys()` before any key created
   // -------------------------------------------------
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 0);
   BOOST_REQUIRE_EQUAL(peer_key_map_size(), 0);
#endif

   // store Alice's peer key
   // ----------------------
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(alice, alice_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == alice_key);
#ifdef _has_peer_keys_db
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 1);                    // we should find one new row
   BOOST_REQUIRE(get_peer_key(alice) == alice_key);               // yes, got it
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 0);                    // `regpeerkey()` was not called again, so no new row.
#endif

   // replace Alices's key with a new one
   // -----------------------------------
   auto new_key = get_public_key("alice.new"_n);
   BOOST_REQUIRE_EQUAL(success(), regpeerkey(alice, new_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == new_key);
#ifdef _has_peer_keys_db
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 1);                    // we should find one updated
   BOOST_REQUIRE(get_peer_key(alice) == new_key);                 // yes, got it
#endif

   // Delete Alices's key
   // -------------------
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: Current key does not match the provided one"),
                       delpeerkey(alice, alice_key)); // not the right key, should be `new_key`
   BOOST_REQUIRE_EQUAL(success(), delpeerkey(alice, new_key));
   BOOST_REQUIRE(get_peer_key_info(alice) == std::optional<fc::crypto::public_key>{});
#ifdef _has_peer_keys_db
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 0);                    // we don't delete in the memory hash map
#endif

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
#ifdef _has_peer_keys_db
   BOOST_REQUIRE_EQUAL(update_peer_keys(), 2);                    // both are updated
   BOOST_REQUIRE(get_peer_key(alice) == alice_key);
   BOOST_REQUIRE(get_peer_key(bob) == bob_key);
#endif

} FC_LOG_AND_RETHROW()


#ifdef _has_peer_keys_db
class basic_stopwatch {
public:
   basic_stopwatch(std::string msg) : _msg(std::move(msg)) {  start(); }

   ~basic_stopwatch() {
      if (!_msg.empty())
         std::cout << _msg << get_time_us()/1000 << "ms\n";
   }

   void start() { _start = clock::now(); } // overwrite start time if needed

   double get_time_us() const {
      using duration_t = std::chrono::duration<double, std::micro>;
      return std::chrono::duration_cast<duration_t>(clock::now() - _start).count();
   }

   using clock = std::chrono::high_resolution_clock;
   using point = std::chrono::time_point<clock>;

   std::string _msg;
   point       _start;
};

// ------------------------------------------------
// example output (release build, AMD 7950x)
// requires changing
// `cfg.state_size = 1024*1024*160;` in tester.hpp
// ------------------------------------------------
// running 1 test case...
// update 10000 keys: 1.71573ms
// update 32 keys out of 10032: 0.059452ms
// ------------------------------------------------
// --- updated peer_keys_db to flip/flop between 2 maps, 100'000 keys
// Running 1 test case...
// update 100000 keys: 38.5003ms
// update 32 keys out of 100032: 0.014688ms  <= much better
// ------------------------------------------------
// sizeof(peer_key_map_t::value_type) == 88
// so 10k keys use ~1MB
// ------------------------------------------------
BOOST_FIXTURE_TEST_CASE(peer_keys_perf, peer_keys_tester) try {
   constexpr size_t num_extra = 32;
   constexpr size_t num_accounts = 100;
   std::vector<account_name> accounts;
   accounts.reserve(num_accounts);

   auto num_to_alpha = [](size_t i) {
      std::string res;
      while (i) {
         res += 'a' + (i % 26);
         i /= 26;
      }
      std::reverse(res.begin(), res.end());
      return res;
   };

   for (size_t i=0; i< num_accounts + num_extra; ++i) {
      account_name acct("alice" + num_to_alpha(i));
      accounts.push_back(acct);
      create_account_with_resources(acct, config::system_account_name);
      if (i % 20 == 0)
         produce_block();
   }
   produce_block();

   for (size_t i=0; i< num_accounts; ++i) {
      auto acct = accounts[i];
      BOOST_REQUIRE_EQUAL(success(), regpeerkey(acct, get_public_key(acct)));
   }

   {
      basic_stopwatch sw("update " + std::to_string(num_accounts) + " keys: ");
      auto num_updated = update_peer_keys();
      BOOST_REQUIRE_EQUAL(num_updated, num_accounts);
   }

   for (size_t i=num_accounts; i< num_accounts + num_extra; ++i) {
      auto acct = accounts[i];
      BOOST_REQUIRE_EQUAL(success(), regpeerkey(acct, get_public_key(acct)));
   }

   {
      basic_stopwatch sw("update " + std::to_string(num_extra) + " keys out of " + std::to_string(num_accounts + num_extra) + ": ");
      auto num_updated = update_peer_keys();
      BOOST_REQUIRE_EQUAL(num_updated, num_extra);
   }

   auto prev = accounts[num_accounts-1];
   auto next = accounts[num_accounts];
   BOOST_REQUIRE(get_peer_key(prev) == get_public_key(prev));
   BOOST_REQUIRE(get_peer_key(next) == get_public_key(next));
   BOOST_REQUIRE(peer_key_map_size() == num_accounts + num_extra);

} FC_LOG_AND_RETHROW()

#endif


BOOST_AUTO_TEST_SUITE_END()