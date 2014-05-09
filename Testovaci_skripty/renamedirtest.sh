#!/bin/bash

for ((i=0; i<$2; i++))
do
	mv "$1/$i" "${1}/a$i" 2> /dev/null
	if ! [ $? -eq 0 ]
	then 
		echo "Nepodarilo se prejmenovat adresar c. $i"
		echo "Test se nezdaril."
		exit 1
	fi 
done
