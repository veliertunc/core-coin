cmake_minimum_required(VERSION 3.20)
project(CoreCoin VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(corecoin src/sha256.cpp src/blockchain.cpp src/address.cpp src/genesis.cpp
                     src/wallet.cpp src/amount.cpp src/ledger.cpp src/chain_validator.cpp src/chain_store.cpp src/signing_key.cpp)
target_include_directories(corecoin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                                   PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(corecoin PUBLIC cxx_std_23)
target_link_libraries(corecoin PUBLIC OpenSSL::Crypto)
set_target_properties(corecoin PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(corecoin PRIVATE /W4 /permissive-)
else()
  target_compile_options(corecoin PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(corecoin-cli src/main.cpp)
target_link_libraries(corecoin-cli PRIVATE corecoin)

include(CTest)
if(BUILD_TESTING)
  add_executable(corecoin-tests tests/test_main.cpp tests/fixtures.cpp
    tests/crypto_tests.cpp tests/blockchain_tests.cpp tests/validation_tests.cpp
    tests/supply_tests.cpp tests/amount_tests.cpp tests/wallet_tests.cpp)
  target_link_libraries(corecoin-tests PRIVATE corecoin)
  # Tests may inspect private modules; consumers inherit only public headers.
  target_include_directories(corecoin-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  add_test(NAME corecoin-tests COMMAND corecoin-tests)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME corecoin-cli-wallets COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.py" $<TARGET_FILE:corecoin-cli>)
  endif()
endif()

if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
  add_custom_target(refresh-compilation-database ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/compile_commands.json"
      "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Refreshing root compile_commands.json")
  add_dependencies(corecoin refresh-compilation-database)
endif()
