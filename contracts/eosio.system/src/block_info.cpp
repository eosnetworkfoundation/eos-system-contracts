#include <eosio.system/block_info.hpp>
#include <eosio.system/eosio.system.hpp>

namespace {

inline uint32_t block_height_from_id(const eosio::checksum256& block_id)
{
   auto arr = block_id.extract_as_byte_array();
   // 32-bit block height is encoded in big endian as the sequence of bytes: arr[0], arr[1], arr[2], arr[3]
   return ((arr[0] << 0x18) | (arr[1] << 0x10) | (arr[2] << 0x08) | arr[3]);
}

} // namespace

namespace eosiosystem {

void system_contract::add_to_blockinfo_table(const eosio::checksum256&    previous_block_id,
                                             const eosio::block_timestamp timestamp) const
{
   const uint32_t new_block_height    = block_height_from_id(previous_block_id) + 1;
   const auto     new_block_timestamp = static_cast<eosio::time_point>(timestamp);

   block_info::block_info_table t(get_self(), 0);

   if (block_info::rolling_window_size > 0) {
      // Add new entry to blockinfo table for the new block.
      t.emplace(get_self(), [&](block_info::block_info_record& r) {
         r.block_height    = new_block_height;
         r.block_timestamp = new_block_timestamp;
      });
   }

   // Erase up to two entries that have fallen out of the rolling window.

   const uint32_t last_prunable_block_height =
      std::max(new_block_height, block_info::rolling_window_size) - block_info::rolling_window_size;

   int count = 2;
   for (auto itr = t.begin(), end = t.end();                                        //
        itr != end && itr->block_height <= last_prunable_block_height && 0 < count; //
        --count)                                                                    //
   {
      itr = t.erase(itr);
   }
}

} // namespace eosiosystem