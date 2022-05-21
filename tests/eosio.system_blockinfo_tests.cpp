#include <functional>
#include <limits>

#include <boost/test/unit_test.hpp>

#include <eosio/chain/config.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/testing/tester.hpp>
#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>
#include <fc/time.hpp>

#include "eosio.system_tester.hpp"

#define TEST_INCLUDE

#include "../contracts/test_contracts/blockinfo_tester/include/blockinfo_tester/blockinfo_tester.hpp"

namespace {

struct block_info_record
{
   uint8_t        version = 0;
   uint32_t       block_height;
   fc::time_point block_timestamp;

   friend bool operator==(const block_info_record& lhs, const block_info_record& rhs)
   {
      return std::tie(lhs.version, lhs.block_height, lhs.block_timestamp) ==
             std::tie(rhs.version, rhs.block_height, rhs.block_timestamp);
   }
};

static constexpr uint32_t rolling_window_size = 10;

} // namespace

FC_REFLECT(block_info_record, (version)(block_height)(block_timestamp))

namespace {

using namespace eosio_system;
using namespace system_contracts::testing;

namespace blockinfo_tester = test_contracts::blockinfo_tester;

static const eosio::chain::name blockinfo_table_name = "blockinfo"_n;

// cspell:disable-next-line
static const eosio::chain::name blockinfo_tester_account_name = "binfotester"_n;

struct block_info_tester : eosio_system::eosio_system_tester
{
private:
   std::optional<eosio::chain::table_id_object::id_type> get_blockinfo_table_id() const
   {
      const auto* table_id_itr = control->db().find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>(
         boost::make_tuple(eosio::chain::config::system_account_name, eosio::chain::name{0}, blockinfo_table_name));

      if (!table_id_itr) {
         // No blockinfo table exists.
         return {};
      }

      return std::optional<eosio::chain::table_id_object::id_type>(table_id_itr->id);
   }

public:
   block_info_tester() : eosio_system_tester(eosio_system_tester::setup_level::deploy_contract) {}

   /**
    * Scans filtered rows in blockinfo table in order of ascending block height where filtering only picks rows
    * corresponding to block heights in the closed interval [start_block_height, end_block_height].
    *
    * For each row visited, its deserialized block_info_record structure is passed into the visitor function.
    * If a call to the visitor function returns false, scanning will stop and this function will return.
    *
    * @pre start_block_height <= end_block_height
    * @returns number of rows visited
    */
   unsigned int scan_blockinfo_table(uint32_t                               start_block_height,
                                     uint32_t                               end_block_height,
                                     std::function<bool(block_info_record)> visitor) const
   {
      FC_ASSERT(start_block_height <= end_block_height, "invalid inputs");

      auto t_id = get_blockinfo_table_id();
      if (!t_id) {
         // No blockinfo table exists, so there is nothing to scan through.
         return 0;
      }

      const auto& idx = control->db().get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();

      unsigned int rows_visited = 0;

      block_info_record r;

      for (auto itr = idx.lower_bound(boost::make_tuple(*t_id, static_cast<uint64_t>(start_block_height)));
           itr != idx.end() && itr->t_id == *t_id && itr->primary_key <= end_block_height; ++itr) //
      {
         fc::datastream<const char*> ds(itr->value.data(), itr->value.size());
         fc::raw::unpack(ds, r);
         ++rows_visited;
         if (!visitor(std::move(r))) {
            break;
         }
      }

      return rows_visited;
   }

   std::vector<block_info_record> get_blockinfo_table()
   {
      std::vector<block_info_record> result;

      scan_blockinfo_table(0, std::numeric_limits<uint32_t>::max(), [&result](const block_info_record& r) {
         result.push_back(r);
         return true;
      });

      return result;
   }

   std::pair<std::optional<blockinfo_tester::latest_block_batch_info_result>, eosio::chain::transaction_trace_ptr>
   get_latest_block_batch_info(blockinfo_tester::get_latest_block_batch_info request)
   {
      std::pair<std::optional<blockinfo_tester::latest_block_batch_info_result>, eosio::chain::transaction_trace_ptr>
         result;

      signed_transaction trx;
      trx.actions.emplace_back(std::vector<permission_level>{{config::system_account_name, config::active_name}},
                               blockinfo_tester_account_name, "call"_n,
                               fc::raw::pack(blockinfo_tester::input_type{std::move(request)}));
      trx.actions.emplace_back(std::vector<permission_level>{}, blockinfo_tester_account_name, "abort"_n,
                               std::vector<char>{});
      set_transaction_headers(trx);
      trx.sign(get_private_key(config::system_account_name, "active"), control->get_chain_id());

      result.second =
         push_transaction(trx, fc::time_point::maximum(), DEFAULT_BILLED_CPU_TIME_US, true); // no_throw set to true

      const auto& action_traces = result.second->action_traces;

      if (action_traces.size() < 3) {
         return result;
      }

      auto validate_action = [](const eosio::chain::action_trace& a, eosio::chain::name action_name) -> bool {
         if (a.receiver != blockinfo_tester_account_name || a.act.account != a.receiver) {
            // Only match on direct actions to blockinfo_tester_account_name.
            return false;
         }

         if (a.act.name != action_name) {
            // Only match on specified action name.
            return false;
         }

         return true;
      };

      // The first action in action_traces should be the call action.
      if (!validate_action(action_traces[0], "call"_n)) {
         // This should never mismatch, but is here just to be sure.
         return result;
      }

      // In addition, ensure the call action has not failed.
      if (!action_traces[0].receipt.has_value()) {
         return result;
      }

      uint32_t call_action_ordinal = action_traces[0].action_ordinal.value;

      if (!validate_action(action_traces[1], "abort"_n)) {
         // The second action in action_traces should be the call action. This should never mismatch, but is here just
         // to be sure.
         return result;
      }

      // In addition, this action should always fail. However, something else may have aborted the transaction earlier
      // in the execution. If that happened, we want to detect that and return the empty optional from this function to
      // indicate something went wrong.
      if (action_traces[1].receipt.has_value() || !action_traces[1].error_code.has_value() ||
          *action_traces[1].error_code != 0) {
         // Something also went wrong if it the abort action did not return an error code or returned an error code
         // other than 0.
         return result;
      }

      // Find the first return action generated by the above call action.
      size_t i = 2;
      for (; i < action_traces.size(); ++i) {
         if (validate_action(action_traces[i], "return"_n)) {
            // Ensure this action was in fact sent by the call action and not anything else.
            if (action_traces[i].creator_action_ordinal.value == call_action_ordinal) {
               if (!action_traces[i].receipt.has_value()) {
                  // The transaction should not fail at the return action.
                  // If it has, something has gone wrong in the contract.
                  return result;
               }

               break;
            }
         }
      }

      // No return action was found.
      if (i == action_traces.size()) {
         return result;
      }

      const auto& returned_payload = action_traces[i].act.data;

      // ilog("returned_payload: ${data}", ("data", returned_payload));
      // ilog("call action console:\n ${console}", ("console", action_traces[0].console));

      blockinfo_tester::output_type output;

      fc::datastream<const char*> ds(returned_payload.data(), returned_payload.size());
      fc::raw::unpack(ds, output);

      // Ensure the expected return type is returned by the contract.
      if (auto response_ptr = std::get_if<blockinfo_tester::latest_block_batch_info_result>(&output)) {
         result.first.emplace(std::move(*response_ptr));
         return result;
      }

      // Otherwise, something has gone wrong.
      return result;
   }
};

bool check_tables_match(const std::vector<block_info_record>& expected_table,
                        const std::vector<block_info_record>& actual_table)
{
   if (expected_table.size() != actual_table.size()) {
      return false;
   }

   bool match = std::equal(expected_table.cbegin(), expected_table.cend(), actual_table.cbegin(),
                           [&](const block_info_record& expected_row, const block_info_record& actual_row) -> bool {
                              return expected_row == actual_row;
                           });

   return match;
}

} // namespace

BOOST_AUTO_TEST_SUITE(eosio_system_blockinfo_tests)

BOOST_FIXTURE_TEST_CASE(blockinfo_table_tests, block_info_tester)
try {
   static_assert(rolling_window_size >= 2);

   produce_blocks(1);

   auto start_block_height    = control->head_block_num();
   auto start_block_timestamp = control->head_block_time();

   std::vector<block_info_record> expected_table;
   auto add_to_expected_table = [&expected_table](uint32_t block_height, fc::time_point block_timestamp) {
      expected_table.push_back(block_info_record{
         .block_height    = block_height,
         .block_timestamp = block_timestamp,
      });
   };

   add_to_expected_table(start_block_height, start_block_timestamp);

   auto actual_table = get_blockinfo_table();
   BOOST_REQUIRE(check_tables_match(expected_table, actual_table));

   // Produce enough blocks to fill up to (but not beyond) rolling window size.

   produce_blocks(rolling_window_size - 1);

   auto block_time_delta = fc::milliseconds(eosio::chain::config::block_interval_ms);

   uint32_t cur_block_height    = start_block_height + 1;
   auto     cur_block_timestamp = start_block_timestamp + block_time_delta;

   auto advance_cur_block = [&cur_block_height, &cur_block_timestamp, block_time_delta]() {
      ++cur_block_height;
      cur_block_timestamp += block_time_delta;
   };

   for (uint32_t end_block_height = start_block_height + rolling_window_size - 1; //
        cur_block_height <= end_block_height;                                     //
        advance_cur_block())                                                      //
   {
      add_to_expected_table(cur_block_height, cur_block_timestamp);
   }

   actual_table = get_blockinfo_table();
   BOOST_CHECK(rolling_window_size == actual_table.size());
   BOOST_CHECK(check_tables_match(expected_table, actual_table));

   // Producing one more block should erase the start block from the table.

   produce_blocks(1);

   expected_table.erase(expected_table.begin());
   add_to_expected_table(cur_block_height, cur_block_timestamp);
   advance_cur_block();

   actual_table = get_blockinfo_table();
   BOOST_CHECK(rolling_window_size == actual_table.size());
   BOOST_CHECK(check_tables_match(expected_table, actual_table));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(get_latest_block_batch_info_tests, block_info_tester)
try {
   static_assert(5 <= rolling_window_size && rolling_window_size <= 100000);

   // Deploy the blockinfo_tester contract.
   create_account_with_resources(blockinfo_tester_account_name, config::system_account_name,
                                 core_sym::from_string("10.0000"), false);
   set_code(blockinfo_tester_account_name, test_contracts::blockinfo_tester_wasm());

   auto latest_block_batch_info = [this](uint32_t batch_start_height_offset,
                                         uint32_t batch_size) -> blockinfo_tester::latest_block_batch_info_result //
   {
      auto result = get_latest_block_batch_info(blockinfo_tester::get_latest_block_batch_info{
         .batch_start_height_offset = batch_start_height_offset,
         .batch_size                = batch_size,
      });
      if (!result.first.has_value()) {
         const eosio::chain::transaction_trace& trace = *result.second;
         if (trace.except.has_value()) {
            elog(trace.except->to_detail_string());
         }
      }
      BOOST_REQUIRE(result.first.has_value());
      return *result.first;
   };

   auto require_latest_block_batch_info =
      [latest_block_batch_info](uint32_t batch_start_height_offset,
                                uint32_t batch_size) -> blockinfo_tester::block_batch_info //
   {
      auto result = latest_block_batch_info(batch_start_height_offset, batch_size);
      if (result.has_error()) {
         elog("require_latest_block_batch_info returned error: ${err}", ("err", result.get_error()));
      }
      BOOST_REQUIRE(!result.has_error());

      return *result.result;
   };

   // Cannot call get_latest_block_batch_info with a batch_size of 0.
   {
      auto result = latest_block_batch_info(0, 0);
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::invalid_input);
   }

   // Insufficient data if blockinfo table is empty.
   {
      auto result = latest_block_batch_info(0, 1);
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   produce_blocks(1); // Now there should be one entry in blockinfo table.

   auto start_block_height    = control->head_block_num();
   auto start_block_timestamp = control->head_block_time();

   BOOST_REQUIRE(8 < start_block_height); // Test assumes it is safe to subtract 8 from start_block_height.

   auto block_time_delta = [](unsigned int num_blocks) -> fc::microseconds {
      return fc::milliseconds(static_cast<int64_t>(num_blocks) * eosio::chain::config::block_interval_ms);
   };

   // Get block info of latest recorded block.
   {
      auto info = require_latest_block_batch_info(0, 1);
      BOOST_CHECK(info.batch_start_height == info.batch_current_end_height);
      BOOST_CHECK(info.batch_start_timestamp == info.batch_current_end_timestamp);
      BOOST_CHECK(info.batch_start_height == start_block_height);
      BOOST_CHECK(info.batch_start_timestamp == start_block_timestamp);
   }

   // Get block info of block looked up by block height.
   {
      auto info = require_latest_block_batch_info(start_block_height, std::numeric_limits<uint32_t>::max());
      BOOST_CHECK(info.batch_start_height == start_block_height);
      BOOST_CHECK(info.batch_start_timestamp == start_block_timestamp);

      // Fields batch_current_end_height and batch_current_end_timestamp are not relevant for this request.
      // But we will check them anyway given what is expected to be true about the state of the blockinfo table.
      BOOST_CHECK(info.batch_current_end_height == start_block_height);
      BOOST_CHECK(info.batch_current_end_timestamp == start_block_timestamp);
   }

   // Insufficient data to get block info for block past the last recorded block.
   {
      auto result = latest_block_batch_info(start_block_height + 1, 1);
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }
   {
      auto result = latest_block_batch_info(start_block_height + 1, std::numeric_limits<uint32_t>::max());
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   // Insufficient data to get block info for block prior to start block.
   {
      auto result = latest_block_batch_info(start_block_height - 1, std::numeric_limits<uint32_t>::max());
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   produce_blocks(2); // Now there should be three entries in the blockinfo table.

   // Still insufficient data to get block info for block prior to start block.
   {
      auto result = latest_block_batch_info(start_block_height - 1, std::numeric_limits<uint32_t>::max());
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   // Also it does not help to choose a reasonable sized batch_size if the chosen batch_size is still large enough that
   // when added to the batch_start_height_offset the sum exceeds the last recorded block height.
   {
      auto result = latest_block_batch_info(start_block_height - 1, 4);
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   // However, with an *appropriate* batch_size, batch_start_height_offset can be less than block height of the
   // earliest recorded block since get_latest_block_batch_info returns information about the *latest* batch which does
   // not need to start at batch_start_height_offset.
   //
   // Note: Assuming no missed blocks due to onblock failure and assuming enough blocks have been generated since the
   // updated system contract was first deployed to allow the number of entries in the blockinfo table to reach
   // rolling_window_size, then a batch_size between 1 and rolling_window_size, inclusively, should always be
   // *appropriate* in the sense of avoiding the insufficient_data error.
   {
      auto info = require_latest_block_batch_info(start_block_height - 1, 2);

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(1);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(2);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 1));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 2));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);
   }

   // Also there is no need for the found batch to be complete.
   {
      auto info = require_latest_block_batch_info(start_block_height - 2, 3);

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(1);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(2);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 1));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 2));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);
   }

   // Get block info for the second recorded block by looking up using block height.
   {
      auto info = require_latest_block_batch_info(start_block_height + 1, std::numeric_limits<uint32_t>::max());

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(1);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(2);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 1));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);

      // Fields batch_current_end_height and batch_current_end_timestamp are not relevant for this request.
      // But we will check them anyway given what is expected to be true about the state of the blockinfo table.
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 2));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);
   }

   produce_blocks(rolling_window_size - 3); // Now there should be rolling_window_size entries in the blockinfo table.

   // Assuming the batches of 5 started at block height (start_block_height - 8), find the info on the latest batch.
   {
      auto info = require_latest_block_batch_info(start_block_height - 8, 5);

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(7);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(9);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 7));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 9));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);

      // Notice that this latest batch is not yet complete.
   }

   produce_blocks(2); // This should cause the first two entries in the blockinfo table to be removed.

   // Now there should be insufficient data to look up the block info for the second block that was originally recorded
   // since it has now been pruned.
   {
      auto result = latest_block_batch_info(start_block_height + 1, std::numeric_limits<uint32_t>::max());
      BOOST_CHECK(result.has_error());
      BOOST_CHECK(result.get_error() ==
                  blockinfo_tester::latest_block_batch_info_result::error_code_enum::insufficient_data);
   }

   // Still assuming the batches of 5 started at block height (start_block_height - 8), find the info on the latest
   // batch again. It should be the same batch as last time except now completed.
   {
      auto info = require_latest_block_batch_info(start_block_height - 8, 5);

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(7);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(11);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 7));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 11));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);
   }

   // But if one more block is added, this causes the previous query to instead return information on the next batch.

   produce_blocks(1);

   {
      auto info = require_latest_block_batch_info(start_block_height - 8, 5);

      auto expected_batch_start_timestamp       = start_block_timestamp + block_time_delta(12);
      auto expected_batch_current_end_timestamp = start_block_timestamp + block_time_delta(12);

      BOOST_CHECK(info.batch_start_height == (start_block_height + 12));
      BOOST_CHECK(info.batch_start_timestamp == expected_batch_start_timestamp);
      BOOST_CHECK(info.batch_current_end_height == (start_block_height + 12));
      BOOST_CHECK(info.batch_current_end_timestamp == expected_batch_current_end_timestamp);
   }
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()