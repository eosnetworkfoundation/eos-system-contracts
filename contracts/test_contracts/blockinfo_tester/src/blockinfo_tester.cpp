#include <blockinfo_tester/blockinfo_tester.hpp>

#include <type_traits>
#include <vector>

#include <eosio/action.hpp>
#include <eosio/check.hpp>
#include <eosio/datastream.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>

namespace {

namespace block_info = eosiosystem::block_info;

}
namespace system_contracts::testing::test_contracts::blockinfo_tester {

auto process(get_latest_block_batch_info request) -> latest_block_batch_info_result
{
   latest_block_batch_info_result response;

   block_info::latest_block_batch_info_result res =
      block_info::get_latest_block_batch_info(request.batch_start_height_offset, request.batch_size);

   response.result           = std::move(res.result);
   response.error_code.value = static_cast<uint32_t>(res.error_code);

   eosio::print("get_latest_block_batch_info: response error_code = ", response.error_code.value, "\n");
   if (response.result.has_value()) {
      const auto& result = *response.result;
      eosio::print("get_latest_block_batch_info: response result:\n");
      eosio::print("    batch_start_height          = ", result.batch_start_height, "\n");
      eosio::print("    batch_current_end_height    = ", result.batch_current_end_height, "\n");
   }

   return response;
}

output_type process_call(input_type input)
{
   return std::visit([](auto&& arg) -> output_type { return process(std::move(arg)); }, std::move(input));
}

} // namespace system_contracts::testing::test_contracts::blockinfo_tester

[[eosio::wasm_entry]] extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
   namespace ns = system_contracts::testing::test_contracts::blockinfo_tester;

   if (receiver == code) {
      if (action == "call"_n.value) {
         ns::input_type input;

         {
            std::vector<char> buffer;
            buffer.resize(eosio::action_data_size());
            eosio::read_action_data(buffer.data(), buffer.size());

            eosio::datastream<const char*> input_ds(static_cast<const char*>(buffer.data()), buffer.size());
            input_ds >> input;
         }

         auto output = ns::process_call(std::move(input));
         static_assert(std::is_same_v<decltype(output), ns::output_type>);

         {
            eosio::action return_action;
            return_action.account = eosio::name{receiver};
            return_action.name    = "return"_n;

            eosio::datastream<size_t> output_size_ds;
            output_size_ds << output;
            return_action.data.resize(output_size_ds.tellp());
            eosio::datastream<char*> output_ds(static_cast<char*>(return_action.data.data()),
                                               return_action.data.size());
            output_ds << output;

            return_action.send();
         }
      } else if (action == "abort"_n.value) {
         eosio::check(false, 0ull);
      }
   }
}