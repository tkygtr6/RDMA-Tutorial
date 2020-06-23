#!/bin/bash

for timeout in $(seq 1 21); do
    echo $timeout 
    RETRY_COUNT=7 TIMEOUT=$timeout mpiexec -n 2 ./rdma-tutorial | grep "Time" | awk '{print $2}'
done
