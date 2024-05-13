add_contract(eosio.fees eosio.fees ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.fees.cpp)

target_include_directories(eosio.fees PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.system/include)

set_target_properties(eosio.fees
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( eosio.fees PUBLIC )
