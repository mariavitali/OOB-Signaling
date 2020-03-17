#!/bin/bash

echo "Start test..."

./supervisor 8 >>outsupervisor.log 2>>errsupervisor.log &

superv=$!

echo "Supervisor and servers running."

sleep 2

echo "Starting clients..."
for ((i=0 ; i < 10; i++)); do
    ./client 5 8 20 >>outclients.log &
    ./client 5 8 20 >>outclients.log &
    sleep 1
done

echo "Sending signals to supervisor every 10 seconds for 60 seconds..."
for ((i=0 ; i < 5 ; i++)); do
    sleep 10
    echo "10 seconds done: send SIGINT"
    kill -SIGINT $superv
done

sleep 10
echo "Last 10 seconds done: sending double SIGINT"
kill -SIGINT $superv
kill -SIGINT $superv

echo "supervisor closed."

echo "starting misura.sh"

bash ./misura.sh outsupervisor.log outclients.log