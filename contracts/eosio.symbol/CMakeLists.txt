add_contract(eosio.symbol eosio.symbol ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.symbol.cpp)

target_include_directories(eosio.symbol
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(eosio.symbol
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.symbol.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.symbol.contracts.md @ONLY )

target_compile_options( eosio.symbol PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
