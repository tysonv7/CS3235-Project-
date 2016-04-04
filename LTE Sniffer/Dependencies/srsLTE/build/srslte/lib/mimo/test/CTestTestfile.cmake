# CMake generated Testfile for 
# Source directory: /home/tysonv7/srsLTE/srslte/lib/mimo/test
# Build directory: /home/tysonv7/srsLTE/build/srslte/lib/mimo/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(layermap_single "layermap_test" "-n" "1000" "-m" "single" "-c" "1" "-l" "1")
ADD_TEST(layermap_diversity_2 "layermap_test" "-n" "1000" "-m" "diversity" "-c" "1" "-l" "2")
ADD_TEST(layermap_diversity_4 "layermap_test" "-n" "1000" "-m" "diversity" "-c" "1" "-l" "4")
ADD_TEST(layermap_multiplex_11 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "1" "-l" "1")
ADD_TEST(layermap_multiplex_12 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "1" "-l" "2")
ADD_TEST(layermap_multiplex_13 "layermap_test" "-n" "1002" "-m" "multiplex" "-c" "1" "-l" "3")
ADD_TEST(layermap_multiplex_14 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "1" "-l" "4")
ADD_TEST(layermap_multiplex_15 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "1" "-l" "5")
ADD_TEST(layermap_multiplex_16 "layermap_test" "-n" "1002" "-m" "multiplex" "-c" "1" "-l" "6")
ADD_TEST(layermap_multiplex_17 "layermap_test" "-n" "994" "-m" "multiplex" "-c" "1" "-l" "7")
ADD_TEST(layermap_multiplex_18 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "1" "-l" "8")
ADD_TEST(layermap_multiplex_22 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "2" "-l" "2")
ADD_TEST(layermap_multiplex_23 "layermap_test" "-n" "1002" "-m" "multiplex" "-c" "2" "-l" "3")
ADD_TEST(layermap_multiplex_24 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "2" "-l" "4")
ADD_TEST(layermap_multiplex_25 "layermap_test" "-n" "1002" "-m" "multiplex" "-c" "2" "-l" "5")
ADD_TEST(layermap_multiplex_26 "layermap_test" "-n" "1002" "-m" "multiplex" "-c" "2" "-l" "6")
ADD_TEST(layermap_multiplex_27 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "2" "-l" "7")
ADD_TEST(layermap_multiplex_28 "layermap_test" "-n" "1000" "-m" "multiplex" "-c" "2" "-l" "8")
ADD_TEST(precoding_single "precoding_test" "-n" "1000" "-m" "single")
ADD_TEST(precoding_diversity2 "precoding_test" "-n" "1000" "-m" "diversity" "-l" "2" "-p" "2")
ADD_TEST(precoding_diversity4 "precoding_test" "-n" "1024" "-m" "diversity" "-l" "4" "-p" "4")
