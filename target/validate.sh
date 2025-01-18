#!/bin/bash

exec1=hello.ll
exec2=hello2.ll

# Generate a list of test inputs
inputs=(1 2 3 4 5 10 15 20 25 50 75 100 -1 -10 -25 -50 123 -123 999 -999 42 -42 0 200 -200 333 -333 78 -78 11 -11 444 -444)
# inputs=(11 22 33 890)

# Iterate over each input and compare the outputs
for input in "${inputs[@]}"; do
    output1=$(../../llvm-project/build/bin/lli $exec1 $input)
    output2=$(../../llvm-project/build/bin/lli $exec2 $input)

    # Extract the "Final value of n" from the output
    result1=$(echo "$output1" | grep -oE "Final value of n: [-0-9]+" | awk '{print $NF}')
    result2=$(echo "$output2" | grep -oE "Final value of n: [-0-9]+" | awk '{print $NF}')

    # Compare the results
    if [ "$result1" == "$result2" ]; then
        echo "Input $input: Match ($result1)"
    else
        echo "Input $input: Mismatch ($result1 != $result2)"
    fi

done