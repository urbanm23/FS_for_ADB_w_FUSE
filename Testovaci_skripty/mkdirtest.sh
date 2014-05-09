#!/bin/bash

for ((i=0; i<$2; i++))
do
	mkdir "$1/$i" 2> /dev/null
	if ! [ $? -eq 0 ]
	then 
		echo "Nepodarilo se vytvorit adresar c. $i"
		echo "Test se nezdaril."
		exit 1
	fi 
done
