add_contract(eosio.token src/eosio.token.cpp)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.token.ricardian.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.token.ricardian.hpp @ONLY)
