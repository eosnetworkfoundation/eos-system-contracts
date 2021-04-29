add_contract(eosio.saving eosio.saving
   ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.saving.cpp
)

target_include_directories(eosio.saving
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.system/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.token/include )

set_target_properties(eosio.saving
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.system.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.system.contracts.md @ONLY )

# target_compile_options( eosio.system PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian)
