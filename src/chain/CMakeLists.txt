add_library(corecoin_chain STATIC blockchain.cpp genesis.cpp genesis.hpp chain_validator.cpp chain_validator.hpp
  ${PROJECT_SOURCE_DIR}/include/corecoin/blockchain.hpp ${PROJECT_SOURCE_DIR}/include/corecoin/network.hpp)
corecoin_component(corecoin_chain)
target_link_libraries(corecoin_chain PUBLIC corecoin_ledger corecoin_crypto corecoin_amount)
