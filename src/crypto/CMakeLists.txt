add_library(corecoin_crypto STATIC sha256.cpp sha256.hpp hex.cpp hex.hpp openssl.hpp secp256k1.cpp secp256k1.hpp
  ecdsa_signature.cpp ecdsa_signature.hpp key_address.cpp key_address.hpp key_file.cpp key_file.hpp signing_key.cpp
  ${PROJECT_SOURCE_DIR}/include/corecoin/signing_key.hpp)
corecoin_component(corecoin_crypto)
target_link_libraries(corecoin_crypto PUBLIC OpenSSL::Crypto)
