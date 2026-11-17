add_library(corecoin_crypto STATIC sha256.cpp sha256.hpp signing_key.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/signing_key.hpp)
corecoin_component(corecoin_crypto)
target_link_libraries(corecoin_crypto PUBLIC OpenSSL::Crypto)
