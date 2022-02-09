add_contract(eosio.bios src/eosio.bios.cpp)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.bios.ricardian.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.bios.ricardian.hpp @ONLY)
