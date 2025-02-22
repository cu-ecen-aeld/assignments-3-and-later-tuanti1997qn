#!/bin/sh

if [ "$#" -ne 2 ] ;
then
	echo "parameters were not specified"
	exit 1
fi

if ! [ -d "$1" ] ;
then
	echo "filesdir does not represent a directory on the filesystem"
	exit 1
fi

file_count_in_dir=$(find "$1" -type f | wc -l)
file_count_match=$(grep -r "$2" "$1" | wc -l) 
echo "The number of files are $file_count_in_dir and the number of matching lines are $file_count_match" 
