#!/bin/bash

read lower_port upper_port < /proc/sys/net/ipv4/ip_local_port_range

while :
do
	PORT="`shuf -i $lower_port-$upper_port -n 1`"
	ss -lpn | grep -q ":$PORT " || break
done

echo "Port number $PORT selected" 

konsole -p 'TerminalColumns=44' -p 'TerminalRows=84' -e "./src/dpm403_master/master $PORT" & 

sleep 1

konsole -e "./src/FE007_3_drone/drone $PORT" &
konsole -e "./src/al9_3_drone/drone $PORT" &
konsole -e "./src/ML99_3_drone/drone $PORT" &
konsole -e "./src/drone_ale_fab/drone $PORT" &