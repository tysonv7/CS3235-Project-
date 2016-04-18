#!/bin/bash

input="database.txt"
duplicate="duplicate.txt"
output="whatever.txt"
wiresharkfile="professionalname.pcap"
defaultheader="0000 01 01 01 02 ff fe 03 00 00 04 00 00 07 01 01"

echo "" > $output


while read line
do
	header=$defaultheader
	removebracket=${line:1:27}
	header+=" $removebracket"
	echo $header >> $output	 
done< $input

text2pcap $output $wiresharkfile -l 147
echo "Duplicated packet : Paging request (line number)"
echo ""`sort $input | uniq -cd` > $duplicate
duplicateline=$(head -n 1 $duplicate)
IFS=';'
for p in $duplicateline
do
	p=${p:4:27}
	currentduplicate=`grep -n "$p" $input | grep -Eo '^[^:]+'`
	duplicatelinenumber="$p ("
	IFS='
	'
	while read -r currentduplicateline
	do		
		duplicatelinenumber+=" $currentduplicate "
	done <<< "$p"
	duplicatelinenumber+=")"
	echo $duplicatelinenumber
done




