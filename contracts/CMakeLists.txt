cmake_minimum_required(VERSION 3.5)

project(contracts)

option(SYSTEM_CONFIGURABLE_WASM_LIMITS
       "Enables use of the host functions activated by the CONFIGURABLE_WASM_LIMITS protocol feature" ON)

option(SYSTEM_BLOCKCHAIN_PARAMETERS
       "Enables use of the host functions activated by the BLOCKCHAIN_PARAMETERS protocol feature" ON)

find_package(cdt)

set(CDT_VERSION_MIN "3.0")
set(CDT_VERSION_SOFT_MAX "3.0")
# set(CDT_VERSION_HARD_MAX "")

# Check the version of CDT
set(VERSION_MATCH_ERROR_MSG "")
CDT_CHECK_VERSION(VERSION_OUTPUT "${CDT_VERSION}" "${CDT_VERSION_MIN}" "${CDT_VERSION_SOFT_MAX}"
                    "${CDT_VERSION_HARD_MAX}" VERSION_MATCH_ERROR_MSG)
if(VERSION_OUTPUT STREQUAL "MATCH")
  message(STATUS "Using CDT version ${CDT_VERSION}")
elseif(VERSION_OUTPUT STREQUAL "WARN")
  message(
    WARNING
      "Using CDT version ${CDT_VERSION} even though it exceeds the maximum supported version of ${CDT_VERSION_SOFT_MAX}; continuing with configuration, however build may fail.\nIt is recommended to use CDT version ${CDT_VERSION_SOFT_MAX}.x"
  )
else() # INVALID OR MISMATCH
  message(
    FATAL_ERROR
      "Found CDT version ${CDT_VERSION} but it does not satisfy version requirements: ${VERSION_MATCH_ERROR_MSG}\nPlease use CDT version ${CDT_VERSION_SOFT_MAX}.x"
  )
endif(VERSION_OUTPUT STREQUAL "MATCH")

set(ICON_BASE_URL "http://127.0.0.1/ricardian_assets/eosio.contracts/icons")

set(ACCOUNT_ICON_URI "account.png#3d55a2fc3a5c20b456f5657faf666bc25ffd06f4836c5e8256f741149b0b294f")
set(ADMIN_ICON_URI "admin.png#9bf1cec664863bd6aaac0f814b235f8799fb02c850e9aa5da34e8a004bd6518e")
set(MULTISIG_ICON_URI "multisig.png#4fb41d3cf02d0dd2d35a29308e93c2d826ec770d6bb520db668f530764be7153")
set(RESOURCE_ICON_URI "resource.png#3830f1ce8cb07f7757dbcf383b1ec1b11914ac34a1f9d8b065f07600fa9dac19")
set(REX_ICON_URI "rex.png#d229837fa62a464b9c71e06060aa86179adf0b3f4e3b8c4f9702f4f4b0c340a8")
set(TOKEN_ICON_URI "token.png#207ff68b0406eaa56618b08bda81d6a0954543f36adc328ab3065f31a5c5d654")
set(TRANSFER_ICON_URI "transfer.png#5dfad0df72772ee1ccc155e670c1d124f5c5122f1d5027565df38b418042d1dd")
set(VOTING_ICON_URI "voting.png#db28cd3db6e62d4509af3644ce7d377329482a14bb4bfaca2aa5f1400d8e8a84")

add_subdirectory(eosio.bios)
add_subdirectory(eosio.boot)
add_subdirectory(eosio.msig)
add_subdirectory(eosio.system)
add_subdirectory(eosio.token)
add_subdirectory(eosio.wrap)

add_subdirectory(test_contracts)