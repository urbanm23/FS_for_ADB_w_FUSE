#!/bin/bash


trap uklid EXIT
function uklid(){
	rm -rf "$path"
}

if ! [ $# -eq 2 ]
then
	echo "Zadan spatny pocet parametru."
	echo "Pouziti: ./test.sh \"cesta_k_docasnemu_adresari\" \"pocet_iteraci\""
	exit 1
fi

path="$1/tmp"

echo "Testovaci skript bakalarske prace. Nazev: Implementace souboroveho systemu pro ADB s vyuzitim FUSE."
echo ""
echo "Test vytvoreni $2 adresaru."

mkdir "$path" 2> /dev/null;
if ! [ $? -eq 0 ]
then
	echo "Nemohu vytvorit docasny soubor. Adresar jiz existuje???"
	exit 1
else
	time ./mkdirtest.sh "$path" "$2"
fi
read

echo "Test prejmenovani $2 adresaru."
time ./renamedirtest.sh "$path" "$2"
read

echo "Test ziskani informaci o $2 adresarich."
time ./lstest.sh "$path"
read

echo "Test smazani $2 adresaru."
time ./rmdirtest.sh "$path" "$2"
read

echo "Test vytvoreni $2 souboru."
time ./createtest.sh "$path" "$2"
read

echo "Zmena metadat (touch) $2 souboru."
time ./touchtest.sh "$path" "$2"
read

echo "Test prejmenovani $2 souboru."
time ./renametest.sh "$path" "$2"
read

echo "Test zapisu dat (512b) do $2 souboru."
time ./truncatetest.sh "$path" "$2"
read

echo "Test cteni dat (512b) z $2 souboru."
time ./readtest.sh "$path" "$2"
read

echo "Test smazani $2 souboru."
time ./rmtest.sh "$path" "$2"
read

