add_library(corecoin_rest STATIC json.cpp json.hpp http.cpp http.hpp router.cpp router.hpp
  node_service.cpp node_service.hpp presenter.cpp presenter.hpp api.cpp api.hpp
  http_server.cpp http_server.hpp rest_server.cpp ${PROJECT_SOURCE_DIR}/include/corecoin/rest_server.hpp)
corecoin_component(corecoin_rest)
target_link_libraries(corecoin_rest PUBLIC corecoin_peer corecoin_consensus corecoin_storage corecoin_chain)
if(WIN32)
  target_link_libraries(corecoin_rest PRIVATE ws2_32)
endif()
