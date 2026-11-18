add_executable(corecoin-tests fixtures.cpp crypto_tests.cpp blockchain_tests.cpp
  validation_tests.cpp supply_tests.cpp amount_tests.cpp wallet_tests.cpp transaction_tests.cpp consensus_tests.cpp rest_tests.cpp)
target_include_directories(corecoin-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(corecoin-tests PRIVATE corecoin GTest::gtest_main)
add_test(NAME corecoin-tests COMMAND corecoin-tests)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME corecoin-cli-wallets COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/cli_tests.py" $<TARGET_FILE:corecoin-cli>)
endif()

if(Python3_Interpreter_FOUND)
  add_test(NAME corecoin-peer-sync COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/peer_tests.py" $<TARGET_FILE:corecoin-cli>)
endif()

if(Python3_Interpreter_FOUND)
  add_test(NAME corecoin-rest-api COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/rest_api_tests.py" $<TARGET_FILE:corecoin-cli>)
endif()
