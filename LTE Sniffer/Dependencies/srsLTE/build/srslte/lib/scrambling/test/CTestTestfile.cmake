# CMake generated Testfile for 
# Source directory: /home/tysonv7/srsLTE/srslte/lib/scrambling/test
# Build directory: /home/tysonv7/srsLTE/build/srslte/lib/scrambling/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(scrambling_pbch_bit "scrambling_test" "-s" "PBCH" "-c" "50")
ADD_TEST(scrambling_pbch_float "scrambling_test" "-s" "PBCH" "-c" "50" "-f")
ADD_TEST(scrambling_pbch_e_bit "scrambling_test" "-s" "PBCH" "-c" "50" "-e")
ADD_TEST(scrambling_pbch_e_float "scrambling_test" "-s" "PBCH" "-c" "50" "-f" "-e")
