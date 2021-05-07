#!/bin/bash

if [ $# -ne 3 ]
then
	echo "USAGE: $0 delay loss tcp_variant"
	exit 1
fi


echo "sudo tc qdisc add dev <lo> root netem delay $1"
echo "sudo tc qdisc change dev <lo> root netem loss $2"

echo "./tmp/server $3 /tmp/inp" &


# give time to server to start
sleep 1 

for i in {1..20}
do
	echo "./tmp/client $3 /tmp/out"
done

echo "kill `pidof server`"
