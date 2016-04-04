include_directories(hdr)
add_library(lte
  src/liblte_common.cc
  src/liblte_rrc.cc
  src/liblte_mme.cc
  src/liblte_security.cc
)
target_link_libraries(lte ${POLARSSL_LIBRARIES})
