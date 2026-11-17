add_library(corecoin_ledger STATIC address.cpp address.hpp ledger.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/ledger.hpp)
corecoin_component(corecoin_ledger)
target_link_libraries(corecoin_ledger PUBLIC corecoin_amount corecoin_crypto corecoin_transaction)
