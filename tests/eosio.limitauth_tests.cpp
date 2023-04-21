#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <fc/log/logger.hpp>
#include <iostream>
#include <sstream>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

inline const auto owner = "owner"_n;
inline const auto active = "active"_n;
inline const auto admin = "admin"_n;
inline const auto admin2 = "admin2"_n;
inline const auto freebie = "freebie"_n;
inline const auto freebie2 = "freebie2"_n;

inline const auto alice = "alice1111111"_n;
inline const auto bob = "bob111111111"_n;

struct limitauth_tester: eosio_system_tester {
   action_result push_action(name code, name action, permission_level auth, const variant_object& data) {
      try {
         TESTER::push_action(code, action, {auth}, data);
         return "";
      } catch (const fc::exception& ex) {
         edump((ex.to_detail_string()));
         return ex.top_message();
      }
   }

   action_result limitauthchg(permission_level pl, const name& account, const std::vector<name>& allow_perms, const std::vector<name>& disallow_perms) {
      return push_action(
         config::system_account_name, "limitauthchg"_n, pl,
         mvo()("account", account)("allow_perms", allow_perms)("disallow_perms", disallow_perms));
   }

   template<typename... Ts>
   action_result push_action_raw(name code, name act, permission_level pl, const Ts&... data) {
      try {
         fc::datastream<size_t> sz;
         (fc::raw::pack(sz, data), ...);
         std::vector<char> vec(sz.tellp());
         fc::datastream<char*> ds(vec.data(), size_t(vec.size()));
         (fc::raw::pack(ds, data), ...);

         signed_transaction trx;
         trx.actions.push_back(action{{pl}, code, act, std::move(vec)});

         set_transaction_headers(trx, DEFAULT_EXPIRATION_DELTA, 0);
         trx.sign(get_private_key(pl.actor, pl.permission.to_string()), control->get_chain_id());
         push_transaction(trx);
         return "";
      } catch (const fc::exception& ex) {
         edump((ex.to_detail_string()));
         return ex.top_message();
      }
   }

   /////////////
   // This set tests using the native abi (no authorized_by)

   action_result updateauth(permission_level pl, name account, name permission, name parent, authority auth) {
      return push_action_raw(
         config::system_account_name, "updateauth"_n, pl, account, permission, parent, auth);
   }

   action_result deleteauth(permission_level pl, name account, name permission) {
      return push_action_raw(
         config::system_account_name, "deleteauth"_n, pl, account, permission);
   }

   action_result linkauth(permission_level pl, name account, name code, name type, name requirement) {
      return push_action_raw(
         config::system_account_name, "linkauth"_n, pl, account, code, type, requirement);
   }

   action_result unlinkauth(permission_level pl, name account, name code, name type) {
      return push_action_raw(
         config::system_account_name, "unlinkauth"_n, pl, account, code, type);
   }

   /////////////
   // This set tests using the extended abi (includes authorized_by)

   action_result updateauth(permission_level pl, name account, name permission, name parent, authority auth, name authorized_by) {
      return push_action_raw(
         config::system_account_name, "updateauth"_n, pl, account, permission, parent, auth, authorized_by);
   }

   action_result deleteauth(permission_level pl, name account, name permission, name authorized_by) {
      return push_action_raw(
         config::system_account_name, "deleteauth"_n, pl, account, permission, authorized_by);
   }

   action_result linkauth(permission_level pl, name account, name code, name type, name requirement, name authorized_by) {
      return push_action_raw(
         config::system_account_name, "linkauth"_n, pl, account, code, type, requirement, authorized_by);
   }

   action_result unlinkauth(permission_level pl, name account, name code, name type, name authorized_by) {
      return push_action_raw(
         config::system_account_name, "unlinkauth"_n, pl, account, code, type, authorized_by);
   }
}; // limitauth_tester

BOOST_AUTO_TEST_SUITE(eosio_system_limitauth_tests)

// alice hasn't opted in; she can still use the native abi (no authorized_by)
BOOST_FIXTURE_TEST_CASE(native_tests, limitauth_tester) try {
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie")));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie));

   // alice@freebie can create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie2, freebie, get_public_key(alice, "freebie2")));

   // alice@freebie can linkauth
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2));

   // alice@freebie can unlinkauth
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n));

   // alice@freebie can delete alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, freebie}, alice, freebie2));

   // bob, who has the published freebie key, attacks
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack")));
} // native_tests
FC_LOG_AND_RETHROW()

// alice hasn't opted in; she can use the extended abi, but set authorized_by to empty
BOOST_FIXTURE_TEST_CASE(extended_empty_tests, limitauth_tester) try {
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie"), ""_n));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie, ""_n));

   // alice@freebie can create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), ""_n));

   // alice@freebie can linkauth
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, ""_n));

   // alice@freebie can unlinkauth
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, ""_n));

   // alice@freebie can delete alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, freebie}, alice, freebie2, ""_n));

   // bob, who has the published freebie key, attacks
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), ""_n));
} // extended_empty_tests
FC_LOG_AND_RETHROW()

// alice hasn't opted in; she can use the extended abi, but set authorized_by to matching values
BOOST_FIXTURE_TEST_CASE(extended_matching_tests, limitauth_tester) try {
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/owner",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie"), owner));
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie"), active));

   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/owner",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie, owner));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie, active));

   // alice@freebie can create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), freebie));

   // alice@freebie can linkauth
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, freebie));

   // alice@freebie can unlinkauth
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, active));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie));

   // alice@freebie can delete alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      deleteauth({alice, freebie}, alice, freebie2, active));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, freebie}, alice, freebie2, freebie));

   // bob, who has the published freebie key, attacks
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), freebie));
} // extended_matching_tests
FC_LOG_AND_RETHROW()

// alice protects her account using allow_perms
BOOST_FIXTURE_TEST_CASE(allow_perms_tests, limitauth_tester) try {
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111",
      limitauthchg({bob, active}, alice, {owner, active, admin}, {}));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: allow_perms does not contain owner",
      limitauthchg({alice, active}, alice, {active, admin}, {}));
   BOOST_REQUIRE_EQUAL(
      "",
      limitauthchg({alice, active}, alice, {owner, active, admin}, {}));

   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, admin, active, get_public_key(alice, "admin"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie, active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, admin, active));

   // Bob, who has the published freebie key, tries using updateauth to modify alice@freebie
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack")));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by does not appear in allow_perms",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), active));

   // alice@freebie can't create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by does not appear in allow_perms",
      updateauth({alice, freebie}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), freebie));

   // alice@active can create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), active));

   // Bob, who has the published freebie key, tries using linkauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by does not appear in allow_perms",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, active));

   // Bob, who has the published freebie key, tries using deleteauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      deleteauth({alice, freebie}, alice, freebie2));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      deleteauth({alice, freebie}, alice, freebie2, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by does not appear in allow_perms",
      deleteauth({alice, freebie}, alice, freebie2, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/owner",
      deleteauth({alice, freebie}, alice, freebie2, owner));

   // Bob, who has the published freebie key, tries using unlinkauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by does not appear in allow_perms",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, active));

   // alice@admin can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, admin}, alice, admin2, admin, get_public_key(alice, "admin2"), admin));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, admin}, alice, "eosio.null"_n, "dosomething"_n, admin2, admin));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, admin}, alice, "eosio.null"_n, "dosomething"_n, admin));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, admin}, alice, admin2, admin));

   // alice@active can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, admin2, admin, get_public_key(alice, "admin2"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, admin2, active));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, active));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, active}, alice, admin2, active));

   // alice@owner can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, owner}, alice, admin2, admin, get_public_key(alice, "admin2"), owner));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, owner}, alice, "eosio.null"_n, "dosomething"_n, admin2, owner));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, owner}, alice, "eosio.null"_n, "dosomething"_n, owner));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, owner}, alice, admin2, owner));
} // allow_perms_tests
FC_LOG_AND_RETHROW()

// alice protects her account using disallow_perms
BOOST_FIXTURE_TEST_CASE(disallow_perms_tests, limitauth_tester) try {
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111",
      limitauthchg({bob, active}, alice, {}, {freebie, freebie2}));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: disallow_perms contains owner",
      limitauthchg({alice, active}, alice, {}, {freebie, owner, freebie2}));
   BOOST_REQUIRE_EQUAL(
      "",
      limitauthchg({alice, active}, alice, {}, {freebie, freebie2}));

   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie, active, get_public_key(alice, "freebie"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, admin, active, get_public_key(alice, "admin"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "noop"_n, freebie, active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, admin, active));

   // Bob, who has the published freebie key, tries using updateauth to modify alice@freebie
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack")));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by appears in disallow_perms",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      updateauth({alice, freebie}, alice, freebie, active, get_public_key(bob, "attack"), active));

   // alice@freebie can't create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by appears in disallow_perms",
      updateauth({alice, freebie}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), freebie));

   // alice@active can create alice@freebie2
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, freebie2, freebie, get_public_key(alice, "freebie2"), active));

   // Bob, who has the published freebie key, tries using linkauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by appears in disallow_perms",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      linkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie2, active));

   // Bob, who has the published freebie key, tries using deleteauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      deleteauth({alice, freebie}, alice, freebie2));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      deleteauth({alice, freebie}, alice, freebie2, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by appears in disallow_perms",
      deleteauth({alice, freebie}, alice, freebie2, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/owner",
      deleteauth({alice, freebie}, alice, freebie2, owner));

   // Bob, who has the published freebie key, tries using unlinkauth
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by is required for this account",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, ""_n));
   BOOST_REQUIRE_EQUAL(
      "assertion failure with message: authorized_by appears in disallow_perms",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, freebie));
   BOOST_REQUIRE_EQUAL(
      "missing authority of alice1111111/active",
      unlinkauth({alice, freebie}, alice, "eosio.null"_n, "noop"_n, active));

   // alice@admin can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, admin}, alice, admin2, admin, get_public_key(alice, "admin2"), admin));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, admin}, alice, "eosio.null"_n, "dosomething"_n, admin2, admin));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, admin}, alice, "eosio.null"_n, "dosomething"_n, admin));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, admin}, alice, admin2, admin));

   // alice@active can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, active}, alice, admin2, admin, get_public_key(alice, "admin2"), active));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, admin2, active));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, active}, alice, "eosio.null"_n, "dosomething"_n, active));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, active}, alice, admin2, active));

   // alice@owner can do these
   BOOST_REQUIRE_EQUAL(
      "",
      updateauth({alice, owner}, alice, admin2, admin, get_public_key(alice, "admin2"), owner));
   BOOST_REQUIRE_EQUAL(
      "",
      linkauth({alice, owner}, alice, "eosio.null"_n, "dosomething"_n, admin2, owner));
   BOOST_REQUIRE_EQUAL(
      "",
      unlinkauth({alice, owner}, alice, "eosio.null"_n, "dosomething"_n, owner));
   BOOST_REQUIRE_EQUAL(
      "",
      deleteauth({alice, owner}, alice, admin2, owner));
} // disallow_perms_tests
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
