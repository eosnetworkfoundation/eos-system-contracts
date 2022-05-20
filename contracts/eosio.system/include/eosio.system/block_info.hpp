#pragma once

#include <eosio/multi_index.hpp>
#include <eosio/name.hpp>
#include <eosio/time.hpp>

#include <limits>
#include <optional>

namespace eosiosystem::block_info {

static constexpr uint32_t rolling_window_size = 10;

/**
 * The blockinfo table holds a rolling window of records containing information for recent blocks.
 *
 * Each record stores the height and timestamp of the correspond block.
 * A record is added for a new block through the onblock action.
 * The onblock action also erases up to two old records at a time in an attempt to keep the table consisting of only
 * records for blocks going back a particular block height difference backward from the most recent block.
 * Currently that block height difference is hardcoded to 10.
 */
struct [[eosio::table, eosio::contract("eosio.system")]] block_info_record
{
   uint8_t           version = 0;
   uint32_t          block_height;
   eosio::time_point block_timestamp;

   uint64_t primary_key() const { return block_height; }

   EOSLIB_SERIALIZE(block_info_record, (version)(block_height)(block_timestamp))
};

using block_info_table = eosio::multi_index<"blockinfo"_n, block_info_record>;

struct block_batch_info
{
   uint32_t          batch_start_height;
   eosio::time_point batch_start_timestamp;
   uint32_t          batch_current_end_height;
   eosio::time_point batch_current_end_timestamp;
};

struct latest_block_batch_info_result
{
   enum error_code_enum : uint32_t
   {
      no_error,
      invalid_input,
      unsupported_version,
      insufficient_data
   };

   std::optional<block_batch_info> result;
   error_code_enum                 error_code = no_error;
};

/**
 * Get information on the latest block batch.
 *
 * A block batch is a contiguous range of blocks of a particular size.
 * A sequence of blocks can be partitioned into a sequence of block batches, where all except for perhaps the last batch
 * in the sequence have the same size. The last batch in the sequence can have a smaller size if the
 * blocks of the blockchain that would complete that batch have not yet been generated or recorded.
 *
 * This function enables the caller to specify a particular partitioning of the sequence of blocks into a sequence of
 * block batches of a particular non-zero size (`batch_size`) and then isolates the last block batch in that sequence
 * and returns the information about that latest block batch if possible. The partitioning will ensure that
 * `batch_start_height_offset` will be equal to the starting block height of exactly one of block batches in the
 * sequence.
 *
 * The information about the latest block batch is the same data captured in `block_batch_info`.
 * Particularly, it returns the height and timestamp of starting and ending blocks within that latest block batch.
 * Note that the range spanning from the start to end block of the latest block batch may be less than batch_size
 * because latest block batch may be incomplete.
 * Also, it is possible for the record capturing info for the starting block to not exist in the blockinfo table. This
 * can either be due to the records being erased as they fall out of the rolling window or, in rare cases, due to gaps
 * in block info records due to failures of the onblock action. In such a case, this function will be unable to return a
 * `block_batch_info` and will instead be forced to return the `insufficient_data` error code.
 * Furthermore, if `batch_start_height_offset` is greater than the height of the latest block for which
 * information is recorded in the blockinfo table, there will be no latest block batch identified for the function to
 * return information about and so it will again be forced to return the `insufficient_data` error code instead.
 */
latest_block_batch_info_result get_latest_block_batch_info(uint32_t    batch_start_height_offset,
                                                           uint32_t    batch_size,
                                                           eosio::name system_account_name = "eosio"_n)
{
   latest_block_batch_info_result result;

   if (batch_size == 0) {
      result.error_code = latest_block_batch_info_result::invalid_input;
      return result;
   }

   block_info_table t(system_account_name, 0);

   // Find information on latest block recorded in the blockinfo table.

   if (t.cbegin() == t.cend()) {
      // The blockinfo table is empty.
      result.error_code = latest_block_batch_info_result::insufficient_data;
      return result;
   }

   auto latest_block_info_itr = --t.cend();

   if (latest_block_info_itr->version != 0) {
      // Compiled code for this function within the calling contract has not been updated to support new version of
      // the blockinfo table.
      result.error_code = latest_block_batch_info_result::unsupported_version;
      return result;
   }

   uint32_t latest_block_batch_end_height = latest_block_info_itr->block_height;

   if (latest_block_batch_end_height < batch_start_height_offset) {
      // Caller asking for a block batch that has not even begun to be recorded yet.
      result.error_code = latest_block_batch_info_result::insufficient_data;
      return result;
   }

   // Calculate height for the starting block of the latest block batch.

   uint32_t latest_block_batch_start_height =
      latest_block_batch_end_height - ((latest_block_batch_end_height - batch_start_height_offset) % batch_size);

   // Note: 1 <= (latest_block_batch_end_height - latest_block_batch_start_height + 1) <= batch_size

   if (latest_block_batch_start_height == latest_block_batch_end_height) {
      // When batch_size == 1, this function effectively simplifies to just returning the info of the latest recorded
      // block. In that case, the start block and the end block of the batch are the same and there is no need for
      // another lookup. So shortcut the rest of the process and return a successful result immediately.
      result.result.emplace(block_batch_info{
         .batch_start_height          = latest_block_batch_start_height,
         .batch_start_timestamp       = latest_block_info_itr->block_timestamp,
         .batch_current_end_height    = latest_block_batch_end_height,
         .batch_current_end_timestamp = latest_block_info_itr->block_timestamp,
      });
      return result;
   }

   // Find information on start block of the latest block batch recorded in the blockinfo table.

   auto start_block_info_itr = t.find(latest_block_batch_start_height);
   if (start_block_info_itr == t.cend() || start_block_info_itr->block_height != latest_block_batch_start_height) {
      // Record for information on start block of the latest block batch could not be found in blockinfo table.
      // This is either because of:
      //    * a gap in recording info due to a failed onblock action;
      //    * a requested start block that was processed by onblock prior to deployment of the system contract code
      //    introducing the blockinfo table;
      //    * or, most likely, because the record for the requested start block was pruned from the blockinfo table as
      //    it fell out of the rolling window.
      result.error_code = latest_block_batch_info_result::insufficient_data;
      return result;
   }

   if (start_block_info_itr->version != 0) {
      // Compiled code for this function within the calling contract has not been updated to support new version of
      // the blockinfo table.
      result.error_code = latest_block_batch_info_result::unsupported_version;
      return result;
   }

   // Successfully return block_batch_info for the found latest block batch in its current state.

   result.result.emplace(block_batch_info{
      .batch_start_height          = latest_block_batch_start_height,
      .batch_start_timestamp       = start_block_info_itr->block_timestamp,
      .batch_current_end_height    = latest_block_batch_end_height,
      .batch_current_end_timestamp = latest_block_info_itr->block_timestamp,
   });
   return result;
}

} // namespace eosiosystem::block_info
