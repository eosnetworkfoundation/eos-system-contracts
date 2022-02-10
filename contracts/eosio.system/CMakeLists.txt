set(EOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS Yes CACHE STRING "Enables use of the host functions activated by the CONFIGURABLE_WASM_LIMITS protocol feature")

set(EOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS Yes CACHE STRING "Enables use of the host functions activated by the BLOCKCHAIN_PARAMETERS protocol feature")

add_contract(eosio.system
   src/eosio.system.cpp
   src/delegate_bandwidth.cpp
   src/exchange_state.cpp
   src/name_bidding.cpp
   src/native.cpp
   src/producer_pay.cpp
   src/powerup.cpp
   src/rex.cpp
   src/voting.cpp
   src/limit_auth_changes.cpp
)

if(EOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS)
  target_compile_definitions(eosio.system PUBLIC EOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS)
  target_compile_definitions(eosio.system-debug PUBLIC EOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS)
  target_compile_definitions(eosio.system-abigen PUBLIC EOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS)
endif()

if(EOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS)
  target_compile_definitions(eosio.system PUBLIC EOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS)
  target_compile_definitions(eosio.system-debug PUBLIC EOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS)
  target_compile_definitions(eosio.system-abigen PUBLIC EOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS)
endif()

target_link_libraries(eosio.system eosio.token-headers)
target_link_libraries(eosio.system-debug eosio.token-headers)
target_link_libraries(eosio.system-abigen eosio.token-headers)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.system.ricardian.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.system.ricardian.hpp @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.system.clauses.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.system.clauses.hpp @ONLY)

generate_abi(rex.results .rex src/rex.results.cpp)
generate_abi(powup.results .powerup src/powerup.results.cpp)
