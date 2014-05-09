#!/bin/bash

for ((i=0; i<$2; i++))
do
	cat "$1/a$i" > /dev/null
	if ! [ $? -eq 0 ]
	then 
		echo "Nepodarilo se precist data ze souboru c. a$i"
		echo "Test se nezdaril."
		exit 1
	fi 
done
