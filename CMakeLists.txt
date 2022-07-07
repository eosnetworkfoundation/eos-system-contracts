cmake_minimum_required(VERSION 3.5)

project(eosio_contracts)

set(VERSION_MAJOR 3)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION_SUFFIX rc1)

if(VERSION_SUFFIX)
  set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_SUFFIX}")
else()
  set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
endif()

message(STATUS "Building eosio.contracts v${VERSION_FULL}")

include(ExternalProject)

find_package(cdt)

option(SYSTEM_CONFIGURABLE_WASM_LIMITS
       "Enables use of the host functions activated by the CONFIGURABLE_WASM_LIMITS protocol feature" ON)

option(SYSTEM_BLOCKCHAIN_PARAMETERS
       "Enables use of the host functions activated by the BLOCKCHAIN_PARAMETERS protocol feature" ON)

ExternalProject_Add(
  contracts_project
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/contracts
  BINARY_DIR ${CMAKE_BINARY_DIR}/contracts
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
             -DCMAKE_TOOLCHAIN_FILE=${CDT_ROOT}/lib/cmake/cdt/CDTWasmToolchain.cmake
             -DSYSTEM_CONFIGURABLE_WASM_LIMITS=${SYSTEM_CONFIGURABLE_WASM_LIMITS}
             -DSYSTEM_BLOCKCHAIN_PARAMETERS=${SYSTEM_BLOCKCHAIN_PARAMETERS}
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_ALWAYS 1)

if(APPLE)
  set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif(UNIX)
  set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

if(APPLE)
  set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif(UNIX)
  set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
  message(STATUS "Building unit tests.")
  add_subdirectory(tests)
else()
  message(STATUS "Unit tests will not be built. To build unit tests, set BUILD_TESTS to ON.")
endif()
