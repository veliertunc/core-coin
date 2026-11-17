add_library(corecoin_wallet STATIC wallet.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/wallet.hpp)
corecoin_component(corecoin_wallet)
target_link_libraries(corecoin_wallet PUBLIC corecoin_chain corecoin_crypto)
