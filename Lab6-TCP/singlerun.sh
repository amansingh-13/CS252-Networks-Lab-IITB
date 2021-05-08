#!/bin/bash

if [ $# -ne 3 ]; then
    echo "USAGE: $0 <delay> <loss> <tcp_variant>"
    exit 1
fi

# Check if rule exists if it doesnt then use "add" otherwise use "change".
netem_exists=$(tc qdisc show dev lo | grep netem | awk '{print $2}')
if [[ $netem_exists=="netem" ]]; then
    tc qdisc del dev lo root netem
fi
tc qdisc add dev lo root netem delay $1ms loss $2%

# Compile Code
gcc server.c -o server
gcc client.c -o client

# Declare array for storing the throughputs
throughputs=()

# Run the server and client parallely
./server $3 send.txt &
sleep 1

# Runs the experiment for 20 times
for i in {1..20}; do
    throughputs+=("$(./client $3 recv.txt)")
    if [[ "$(diff send.txt recv.txt)" != "" ]]; then
        echo "Transmission Failure: SENT and RCVD file MISMATCH."
    fi
done

# Close the server gracefully so as to avoid binding issues.
kill $(pidof server)

# Use the python scripts for calculating the standard deviation
# and mean of the calculated throughputs array.
stdev="$(python3 stddev.py "${throughputs[@]}")"
mean="$(python3 mean.py "${throughputs[@]}")"

# Add a row in the output.csv
printf '%s\n' $1 $2 $3 $mean $stdev | paste -sd ',' >>output.csv
