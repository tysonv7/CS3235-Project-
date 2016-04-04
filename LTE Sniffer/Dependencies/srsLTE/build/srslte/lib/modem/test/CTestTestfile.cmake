# CMake generated Testfile for 
# Source directory: /home/tysonv7/srsLTE/srslte/lib/modem/test
# Build directory: /home/tysonv7/srsLTE/build/srslte/lib/modem/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(modem_bpsk "modem_test" "-n" "1024" "-m" "1")
ADD_TEST(modem_qpsk "modem_test" "-n" "1024" "-m" "2")
ADD_TEST(modem_qam16 "modem_test" "-n" "1024" "-m" "4")
ADD_TEST(modem_qam64 "modem_test" "-n" "1008" "-m" "6")
ADD_TEST(modem_bpsk_soft "modem_test" "-n" "1024" "-m" "1")
ADD_TEST(modem_qpsk_soft "modem_test" "-n" "1024" "-m" "2")
ADD_TEST(modem_qam16_soft "modem_test" "-n" "1024" "-m" "4")
ADD_TEST(modem_qam64_soft "modem_test" "-n" "1008" "-m" "6")
