#!/bin/sh

if [ "$#" -ne 2 ] ;
then
	echo "parameters were not specified"
	exit 1
fi

mkdir -p "$(dirname "$1")"
touch "$1"
echo "$2" > "$1"
