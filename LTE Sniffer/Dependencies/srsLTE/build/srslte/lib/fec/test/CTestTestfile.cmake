# CMake generated Testfile for 
# Source directory: /home/tysonv7/srsLTE/srslte/lib/fec/test
# Build directory: /home/tysonv7/srsLTE/build/srslte/lib/fec/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(rm_conv_test_1 "rm_conv_test" "-t" "480" "-r" "1920")
ADD_TEST(rm_conv_test_2 "rm_conv_test" "-t" "1920" "-r" "480")
ADD_TEST(rm_turbo_test_1 "rm_turbo_test" "-e" "1920")
ADD_TEST(rm_turbo_test_2 "rm_turbo_test" "-e" "8192")
ADD_TEST(turbodecoder_test_504_1 "turbodecoder_test" "-n" "100" "-s" "1" "-l" "504" "-e" "1.0" "-t")
ADD_TEST(turbodecoder_test_504_2 "turbodecoder_test" "-n" "100" "-s" "1" "-l" "504" "-e" "2.0" "-t")
ADD_TEST(turbodecoder_test_6114_1_5 "turbodecoder_test" "-n" "100" "-s" "1" "-l" "6144" "-e" "1.5" "-t")
ADD_TEST(turbodecoder_test_known "turbodecoder_test" "-n" "1" "-s" "1" "-k" "-e" "0.5")
ADD_TEST(turbocoder_test_all "turbocoder_test")
ADD_TEST(viterbi_40_0 "viterbi_test" "-n" "1000" "-s" "1" "-l" "40" "-t" "-e" "0.0")
ADD_TEST(viterbi_40_2 "viterbi_test" "-n" "1000" "-s" "1" "-l" "40" "-t" "-e" "2.0")
ADD_TEST(viterbi_40_3 "viterbi_test" "-n" "1000" "-s" "1" "-l" "40" "-t" "-e" "3.0")
ADD_TEST(viterbi_40_4 "viterbi_test" "-n" "1000" "-s" "1" "-l" "40" "-t" "-e" "4.5")
ADD_TEST(viterbi_1000_0 "viterbi_test" "-n" "100" "-s" "1" "-l" "1000" "-t" "-e" "0.0")
ADD_TEST(viterbi_1000_2 "viterbi_test" "-n" "100" "-s" "1" "-l" "1000" "-t" "-e" "2.0")
ADD_TEST(viterbi_1000_3 "viterbi_test" "-n" "100" "-s" "1" "-l" "1000" "-t" "-e" "3.0")
ADD_TEST(viterbi_1000_4 "viterbi_test" "-n" "100" "-s" "1" "-l" "1000" "-t" "-e" "4.5")
ADD_TEST(crc_24A "crc_test" "-n" "5001" "-l" "24" "-p" "0x1864CFB" "-s" "1")
ADD_TEST(crc_24B "crc_test" "-n" "5001" "-l" "24" "-p" "0x1800063" "-s" "1")
ADD_TEST(crc_16 "crc_test" "-n" "5001" "-l" "16" "-p" "0x11021" "-s" "1")
ADD_TEST(crc_8 "crc_test" "-n" "5001" "-l" "8" "-p" "0x19B" "-s" "1")
