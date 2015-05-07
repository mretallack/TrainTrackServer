#!/bin/bash


while [ 1 ] 
do

./traintrack | netcat -l 4444

sleep 1 
done

