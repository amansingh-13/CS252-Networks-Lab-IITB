#!/bin/sh

if [ $# -ne 1 ]
then
	echo "# This script just creates a 5 MB file"
	echo "USAGE: $0 path/to/file"
	exit 1
fi

touch $1
python3 -c "print('A'*(1024*1024*5-1))" > $1
