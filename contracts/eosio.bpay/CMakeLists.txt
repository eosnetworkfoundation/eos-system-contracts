add_contract(eosio.bpay eosio.bpay ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.bpay.cpp)

target_include_directories(eosio.bpay PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.system/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.token/include)

set_target_properties(eosio.bpay
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( eosio.bpay PUBLIC )
