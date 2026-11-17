add_library(corecoin_storage STATIC chain_store.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/chain_store.hpp)
corecoin_component(corecoin_storage)
target_link_libraries(corecoin_storage PUBLIC corecoin_chain)
