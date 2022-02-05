add_contract(eosio.msig src/eosio.msig.cpp)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.msig.ricardian.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.msig.ricardian.hpp @ONLY)
