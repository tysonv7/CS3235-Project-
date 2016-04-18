[SNIFFING PAGING REQUESTS]
1. Extract executable pdsch_ue from /srsLTE/build/srslte/exampels
2. Run ./pdsch_ue, configure settings
	
	./pdsch_ue [agpPoOcildDnruv] -f rx_frequency (in Hz) | -i input_file
	-a RF args [Default ]
	-g RF fix RX gain [Default AGC]
	-i input_file [Default use RF board]
	-o offset frequency correction (in Hz) for input file [Default 0.0 Hz]
	-O offset samples for input file [Default 0]
	-p nof_prb for input file [Default 25]
	-P nof_ports for input file [Default 1]
	-c cell_id for input file [Default 0]
	-r RNTI in Hex [Default 0xffff]
	-l Force N_id_2 [Default best]
	-C Disable CFO correction [Default Enabled]
	-t Add time offset [Default 0]
	 plots are disabled. Graphics library not available
	-n nof_subframes [Default -1]
	-s remote UDP port to send input signal (-1 does nothing with it) [Default -1]
	-S remote UDP address to send input signal [Default 127.0.0.1]
	-u remote TCP port to send data (-1 does nothing with it) [Default -1]
	-U remote TCP address to send data [Default 127.0.0.1]
	-v [set srslte_verbose to debug, default none]
3. Enable paging requests by using the "-r" flag, followed by RNTI in hex. 
	In our test scenarios, most RNTIs used by cell towers are 65534,
	which is 0xfffe in hexademical format.
4. From your results of the cell tower scan, choose a frequency with the "-f" flag.
	In our test secnario, we use the 1815Mhz frequency.
	The frequency must be entered in "hz".
5. An example run would be "./pdsch_ue -r 0xfffe -f 1815000000"
6. The program will attempt to locate a cell tower in the area, and if found - listen to paging requests broadcasted.
7. At termination through "ctrl -c", make sure the resulting file "database.txt" and the bash script "RawToWireshark"
	are in the same location.
8. Run "bash RawToWireshark"
9. Open resulting pcap file in wireshark to view results.
