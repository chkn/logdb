#!/bin/bash

FILE=$1
PROCS=$2
THREADS=$3
COUNT=$4

BINARY="../bin/Release/StressTest"
if [ ! -e $BINARY ]; then
	BINARY="../bin/Debug/StressTest"
	if [ ! -e $BINARY ]; then
		echo "Cannot find StressTest binary. Did you build first?"
		exit 1
	fi
fi

if [ "$FILE" == "" ] || [ "$PROCS" == "" ] || [ "$THREADS" == "" ] || [ "$COUNT" == "" ]; then
	echo "Usage: $0 [file] [processes] [threads]"
	echo "Where:"
	echo "   [file]      The name of the db file to open/create"
	echo "   [processes] Number of processes to spawn"
	echo "   [threads]   Number of threads per process"
	echo "   [count]     Number of iterations per thread"
	exit 1
fi

for i in $(seq 1 $PROCS); do
	$BINARY "$FILE" "p$i" "$THREADS" "$COUNT" &
done

wait
exit 0
