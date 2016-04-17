LTE Sniffer Installation Instructions

1) If this is a clean PC with none of the typical dependencies installed, I would suggest using this script to help you install basic dependencies such as gnuradio, uhd etc. 

mkdir gnuradio
cd gnurdio
wget http://www.sbrac.org/files/build-gnuradio
chmod a+x build-gnuradio

./build-gnuradio -v

2) Next we install srsLTE. Follow the instructions on the main website.
https://github.com/srsLTE/srsLTE
2.1) However use the files that we had already included in the Dependencies folder as these codes have been customised to work for the main software. 
2.2) Navigate to the build folder
2.3) cmake ../
2.4) make
2.5) sudo make install
2.6) Copy the executable cell_search into the main directory

3) Next we install srsUE. Follow the instructions on the main website

https://github.com/srsLTE/srsUE

3.1) However use the files that we had already included in the Dependencies folder as these codes have been customised to work for the main software. 
3.2) Navigate to the build folder
3.3) cmake ../
3.4) make
3.5) sudo make install
3.6) Copy the executable ue and the file ue.conf into the main directory

4) Next we install mySQL.
4.1) sudo apt-get install mysql-server

Be sure to remember the username and password from this step

5) Next we install mySQL Connector
5.1) Ensure that the python version in this computer is 3.4. Otherwise update the python version to 3.4

5.2) Proceed to https://dev.mysql.com/downloads/connector/python/ and download the python 3.4 version of the mysql Connector

6) Next we install php
6.1) sudo apt-get install php

7) Next we run the script to create database from the main directory. 
7.1) Before that use a text editor and edit a couple of lines. For example if we use vim
7.2) Edit line 4 with the correct username
7.3) Edit line 5 with the correct password
7.4) Run this command
php CreateDatabase.php

7.5) You can access your sql server and check if the table has been properly initialised


8) Install kivy from this website https://kivy.org/#download

9) From the main directory, use your favourite text editor and edit these lines
9.1) Edit line 24 with the mysql username and password and then save the file. 
9.2) Run chmod +x main.py
9.3) Test with ./main.py

10) Run the entire LTE Sniffer
10.1) Use your favourite text editor to verify line 12 to ensure that the LTE Sniffer will check the bands that you want to scan for. 
10.2) Run bash FileSniffer.sh
10.3) Highly recommended to let the scan finish fully. Once it is done, the GUI which is the main.py will pop up

11) For subsequent uses, if no scanning is needed, i.e just to see previous results, can just run main.py. Results will be removed after 20 days if the frequency is not detected after 20 days. 
