add_contract(eosio.wrap src/eosio.wrap.cpp)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.wrap.ricardian.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.wrap.ricardian.hpp @ONLY)
