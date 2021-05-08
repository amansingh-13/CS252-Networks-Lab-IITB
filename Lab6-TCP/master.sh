#!/bin/bash

variants=("reno" "cubic")
delays=(10 50 100)
losses=(0.1 0.5 1)

printf "Delay,Loss,Variant,Mean,Stdev\n" > output.csv

# Loop through all the 18 possibilities and tabulate 
# all the results in the output.csv
for variant in "${variants[@]}"; do
    for delay in "${delays[@]}"; do
        for loss in "${losses[@]}"; do
            ./singlerun.sh $delay $loss $variant
        done
    done
done
