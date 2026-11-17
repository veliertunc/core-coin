cmake_minimum_required(VERSION 3.20)
project(CoreCoin VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

function(corecoin_component target)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include"
                                      PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(${target} PUBLIC cxx_std_23)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_subdirectory(src/amount)
add_subdirectory(src/crypto)
add_subdirectory(src/transaction)
add_subdirectory(src/ledger)
add_subdirectory(src/chain)
add_subdirectory(src/wallet)
add_subdirectory(src/storage)
add_library(corecoin INTERFACE)
target_link_libraries(corecoin INTERFACE corecoin_amount corecoin_crypto corecoin_ledger
                                         corecoin_chain corecoin_wallet corecoin_storage)
add_subdirectory(src/cli)

include(CTest)
if(BUILD_TESTING)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt")
    message(FATAL_ERROR "GoogleTest submodule missing; run git submodule update --init --recursive")
  endif()
  set(gtest_force_shared_crt ON CACHE BOOL "Use the parent project's MSVC runtime" FORCE)
  add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
  add_subdirectory(tests)
endif()

if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
  add_custom_target(refresh-compilation-database ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/compile_commands.json" "${PROJECT_SOURCE_DIR}/compile_commands.json"
    COMMENT "Refreshing root compile_commands.json")
  foreach(target corecoin_amount corecoin_crypto corecoin_transaction corecoin_ledger
                 corecoin_chain corecoin_wallet corecoin_storage corecoin_cli_input corecoin-cli)
    add_dependencies(${target} refresh-compilation-database)
  endforeach()
endif()
