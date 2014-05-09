#!/bin/bash

for ((i=0; i<$2; i++))
do
	rmdir "$1/a$i" 2> /dev/null
	if ! [ $? -eq 0 ]
	then 
		echo "Nepodarilo se smazat adresar c. a$i"
		echo "Test se nezdaril."
		exit 1
	fi 
done
