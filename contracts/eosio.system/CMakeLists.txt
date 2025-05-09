add_contract(
  eosio.system
  eosio.system
  ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.system.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/delegate_bandwidth.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/exchange_state.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/finalizer_key.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/name_bidding.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/native.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/peer_keys.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/producer_pay.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/powerup.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rex.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/voting.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/limit_auth_changes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/block_info.cpp)

if(SYSTEM_CONFIGURABLE_WASM_LIMITS)
  target_compile_definitions(eosio.system PUBLIC SYSTEM_CONFIGURABLE_WASM_LIMITS)
endif()

if(SYSTEM_BLOCKCHAIN_PARAMETERS)
  target_compile_definitions(eosio.system PUBLIC SYSTEM_BLOCKCHAIN_PARAMETERS)
endif()

target_include_directories(eosio.system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.token/include)

set_target_properties(eosio.system PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_contract(rex.results rex.results ${CMAKE_CURRENT_SOURCE_DIR}/src/rex.results.cpp)
add_contract(powup.results powup.results ${CMAKE_CURRENT_SOURCE_DIR}/src/powerup.results.cpp)

target_include_directories(rex.results PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(powup.results PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(rex.results PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.rex")

set_target_properties(powup.results PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.powerup")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.system.contracts.md.in
               ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.system.contracts.md @ONLY)

target_compile_options(eosio.system PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian
                                           -R${CMAKE_CURRENT_BINARY_DIR}/ricardian)
