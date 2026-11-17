add_library(corecoin_consensus STATIC consensus.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/consensus.hpp)
corecoin_component(corecoin_consensus)
target_link_libraries(corecoin_consensus PUBLIC corecoin_chain)
