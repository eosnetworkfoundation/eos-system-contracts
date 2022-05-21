#pragma once

#ifdef TEST_INCLUDE

#include <fc/io/varint.hpp>
#include <fc/time.hpp>

#else

#include <eosio/time.hpp>
#include <eosio/varint.hpp>

#include <eosio.system/block_info.hpp>

#endif

#include <cstdint>
#include <optional>
#include <variant>

namespace system_contracts::testing::test_contracts::blockinfo_tester {

#ifdef TEST_INCLUDE

using time_point = fc::time_point;
using varint     = fc::unsigned_int;

#else

using time_point = eosio::time_point;
using varint     = eosio::unsigned_int;

#endif

/**
 * @brief Input data structure for `get_latest_block_batch_info` RPC
 *
 * @details Use this struct as the input for a call to the `get_latest_block_batch_info` RPC. That call will return the
 * result as the `latest_block_batch_info_result` struct.
 */
struct get_latest_block_batch_info
{
   uint32_t batch_start_height_offset;
   uint32_t batch_size;
};

#ifdef TEST_INCLUDE

struct block_batch_info
{
   uint32_t   batch_start_height;
   time_point batch_start_timestamp;
   uint32_t   batch_current_end_height;
   time_point batch_current_end_timestamp;
};

#else

using eosiosystem::block_info::block_batch_info;

#endif

/**
 * @brief Output data structure for `get_latest_block_batch_info` RPC
 */
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
   varint                          error_code = no_error;

   bool has_error() const { return !(error_code.value == no_error && result.has_value()); }

   error_code_enum get_error() const { return static_cast<error_code_enum>(error_code.value); }

#ifndef TEST_INCLUDE

   EOSLIB_SERIALIZE(latest_block_batch_info_result, (result)(error_code))

#endif
};

using input_type = std::variant<get_latest_block_batch_info>;

using output_type = std::variant<latest_block_batch_info_result>;

} // namespace system_contracts::testing::test_contracts::blockinfo_tester

#ifdef TEST_INCLUDE

FC_REFLECT(system_contracts::testing::test_contracts::blockinfo_tester::get_latest_block_batch_info,
           (batch_start_height_offset)(batch_size))
FC_REFLECT(system_contracts::testing::test_contracts::blockinfo_tester::block_batch_info,
           (batch_start_height)(batch_start_timestamp)(batch_current_end_height)(batch_current_end_timestamp))
FC_REFLECT_ENUM(
   system_contracts::testing::test_contracts::blockinfo_tester::latest_block_batch_info_result::error_code_enum,
   (no_error)(invalid_input)(unsupported_version)(insufficient_data))
FC_REFLECT(system_contracts::testing::test_contracts::blockinfo_tester::latest_block_batch_info_result,
           (result)(error_code))

#endif