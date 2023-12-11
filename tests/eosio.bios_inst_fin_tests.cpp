#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>

#include <fc/variant_object.hpp>

#include "contracts.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

class eosio_bios_if_tester : public tester {
public:

   eosio_bios_if_tester() {
      create_accounts( { "iftester"_n } );
      produce_block();

      base_tester::push_action(config::system_account_name, "setpriv"_n,
                               config::system_account_name,  mutable_variant_object()
                               ("account", "iftester")
                               ("is_priv", 1)
      );

      set_code( "iftester"_n, contracts::bios_wasm() );
      set_abi( "iftester"_n, contracts::bios_abi().data() );
      produce_block();
   }
};

BOOST_AUTO_TEST_SUITE(eosio_bios_if_tests)

BOOST_FIXTURE_TEST_CASE( set_1_finalizer, eosio_bios_if_tester ) try {
   push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 2)
         ("finalizers", std::vector<mvo>{mvo()
            ("description", "set_1_finalizer")
            ("weight", 1)
            ("public_key", "PUB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==")
            ("pop", "SIG_BLS_qxozxdQngA4iDidRNJXwKML7VhawRi6XGMXeBc55MzDaAyixR5D3Ys7d72IiwroUkWDqEVQrPq+u/ukICWD9g+LeE9JNxn8IBMLpotXu728ezyal6g5tMoDf8PQuZSEP6yPSMGo7ajbHVe+ehcgWs+/zpxWH1WgCTgU3Bc5Qy32z6L0ztK3WLuW25OmK3EQLbIP5sPMv07gMWP4aDNLAor6IzQYMvxFaibiWsSqMt4YxB6eONetmdftCn5Om3NcHwW7Ueg==")})));
    signed_block_ptr cur_block = produce_block();
    fc::variant pretty_output;
    abi_serializer::to_variant( *cur_block, pretty_output, get_resolver(), fc::microseconds::maximum() );
    BOOST_REQUIRE(pretty_output.get_object().contains("proposed_finalizer_policy"));
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["generation"], 1);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["fthreshold"], 2);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"].size(), 1u);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(0)]["description"], "set_1_finalizer");
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(0)]["fweight"], 1);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(0)]["public_key"], "PUB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==");
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( set_2_finalizers, eosio_bios_if_tester ) try {
   push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 4)
         ("finalizers", std::vector<mvo>{mvo()
            ("description", "set_2_finalizer_1")
            ("weight", 1)
            ("public_key", "PUB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==")
            ("pop", "SIG_BLS_qxozxdQngA4iDidRNJXwKML7VhawRi6XGMXeBc55MzDaAyixR5D3Ys7d72IiwroUkWDqEVQrPq+u/ukICWD9g+LeE9JNxn8IBMLpotXu728ezyal6g5tMoDf8PQuZSEP6yPSMGo7ajbHVe+ehcgWs+/zpxWH1WgCTgU3Bc5Qy32z6L0ztK3WLuW25OmK3EQLbIP5sPMv07gMWP4aDNLAor6IzQYMvxFaibiWsSqMt4YxB6eONetmdftCn5Om3NcHwW7Ueg=="),

            mvo()("description", "set_2_finalizer_2")
            ("weight", 2)
            ("public_key", "PUB_BLS_UGcXVpLNrhdODrbI9Geaswu8wFnL+WMnphfTaCgehRxol5wI1qiU5zq6qHp9+CkFnmm2XWCcM/YEtqZYL6uTM1TXTpTm3LODI0s/ULO4iKSNYclsmDdh5cFSMmKKnloHudh3Zw==")
            ("pop", "SIG_BLS_bFLCSmXqVrFqWo2mqxCeuLO5iAevMu/8qxpS7HkXSMmXuVLEytt+shDHn8FpcrUMlMHjxEAHOyRVi0ckvXYrCpHMx6floRvljJ1vV2FphGqgm24DxuB1BE3E21Okc0QS3GH6UCISfVBXuqoK+EfmoSGz3ssi0kzevE8MQitzGgK9EW3zTlZtvdRryI1OTUoUXkjTo1Q2VQIbqJZ55W7SNHUcIBZO5Ih4AXc7usjWUBXv1BK0NNcort4EAfOXnkIN47iRLQ==")
            })));

    signed_block_ptr cur_block = produce_block();
    fc::variant pretty_output;
    abi_serializer::to_variant( *cur_block, pretty_output, get_resolver(), fc::microseconds::maximum() );
    BOOST_REQUIRE(pretty_output.get_object().contains("proposed_finalizer_policy"));
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["generation"], 1);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["fthreshold"], 4);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"].size(), 2u);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(1)]["description"], "set_2_finalizer_2");
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(1)]["fweight"], 2);
    BOOST_REQUIRE_EQUAL(pretty_output["proposed_finalizer_policy"]["finalizers"][size_t(1)]["public_key"], "PUB_BLS_UGcXVpLNrhdODrbI9Geaswu8wFnL+WMnphfTaCgehRxol5wI1qiU5zq6qHp9+CkFnmm2XWCcM/YEtqZYL6uTM1TXTpTm3LODI0s/ULO4iKSNYclsmDdh5cFSMmKKnloHudh3Zw==");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( empty_finalizers, eosio_bios_if_tester ) try {
   BOOST_REQUIRE_THROW(push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 2)
         ("finalizers", std::vector<mvo>{}))), eosio_assert_message_exception);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( public_key_not_started_with_PUB_BLS, eosio_bios_if_tester ) try {
   BOOST_REQUIRE_THROW(push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 2)
         ("finalizers", std::vector<mvo>{mvo()
            ("description", "set_1_finalizer")
            ("weight", 1)
            // PUB_BLS to UB_BLS
            ("public_key", "UB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==")
            ("pop", "SIG_BLS_qxozxdQngA4iDidRNJXwKML7VhawRi6XGMXeBc55MzDaAyixR5D3Ys7d72IiwroUkWDqEVQrPq+u/ukICWD9g+LeE9JNxn8IBMLpotXu728ezyal6g5tMoDf8PQuZSEP6yPSMGo7ajbHVe+ehcgWs+/zpxWH1WgCTgU3Bc5Qy32z6L0ztK3WLuW25OmK3EQLbIP5sPMv07gMWP4aDNLAor6IzQYMvxFaibiWsSqMt4YxB6eONetmdftCn5Om3NcHwW7Ueg==")}))), eosio_assert_message_exception);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( signature_not_started_with_SIG_BLS, eosio_bios_if_tester ) try {
   BOOST_REQUIRE_THROW(push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 2)
         ("finalizers", std::vector<mvo>{mvo()
            ("description", "set_1_finalizer")
            ("weight", 1)
            ("public_key", "PUB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==")
            // SIG_BLS changed to SIG_BL
            ("pop", "SIG_BL_qxozxdQngA4iDidRNJXwKML7VhawRi6XGMXeBc55MzDaAyixR5D3Ys7d72IiwroUkWDqEVQrPq+u/ukICWD9g+LeE9JNxn8IBMLpotXu728ezyal6g5tMoDf8PQuZSEP6yPSMGo7ajbHVe+ehcgWs+/zpxWH1WgCTgU3Bc5Qy32z6L0ztK3WLuW25OmK3EQLbIP5sPMv07gMWP4aDNLAor6IzQYMvxFaibiWsSqMt4YxB6eONetmdftCn5Om3NcHwW7Ueg==")}))), eosio_assert_message_exception);
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( pop_failed, eosio_bios_if_tester ) try {
   BOOST_REQUIRE_THROW(push_action("iftester"_n, "setfinalizer"_n, "iftester"_n, mvo()
      ("finalizer_policy", mvo()("threshold", 2)
         ("finalizers", std::vector<mvo>{mvo()
            ("description", "set_1_finalizer")
            ("weight", 1)
            ("public_key", "PUB_BLS_jpMTmybJZvf4k6fR7Bgu7wrNjwQLK/IBdjhZHWTjoohgdUMi8VpTGsyYpzP1+ekMzUuZ8LqFcnfO0myTwH9Y2YeabhhowSp7nzJJhgO4XWCpcGCLssOjVWh3/D9wMIISVUwfsQ==")
            // SIG_BLS_q changed to SIG_BLS_j
            ("pop", "SIG_BLS_jxozxdQngA4iDidRNJXwKML7VhawRi6XGMXeBc55MzDaAyixR5D3Ys7d72IiwroUkWDqEVQrPq+u/ukICWD9g+LeE9JNxn8IBMLpotXu728ezyal6g5tMoDf8PQuZSEP6yPSMGo7ajbHVe+ehcgWs+/zpxWH1WgCTgU3Bc5Qy32z6L0ztK3WLuW25OmK3EQLbIP5sPMv07gMWP4aDNLAor6IzQYMvxFaibiWsSqMt4YxB6eONetmdftCn5Om3NcHwW7Ueg==")}))), eosio_assert_message_exception);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
