#!/bin/bash

read lower_port upper_port < /proc/sys/net/ipv4/ip_local_port_range

while :
do
	PORT="`shuf -i $lower_port-$upper_port -n 1`"
	ss -lpn | grep -q ":$PORT " || break
done

echo "Port number $PORT selected" 

konsole -p 'TerminalColumns=44' -p 'TerminalRows=84' -e "./src/dpm403_master/master $PORT" & echo "PID shell of master        : $!"

sleep 1

konsole -e "./src/FE007_3_drone/drone $PORT" & echo "PID shell of drone FE007   : $!"
konsole -e "./src/al9_3_drone/drone $PORT" & echo "PID shell of drone al9     : $!"
konsole -e "./src/ML99_3_drone/drone $PORT" & echo "PID shell of drone ML99    : $!"
konsole -e "./src/drone_ale_fab/drone $PORT" & echo "PID shell of drone ale_fab : $!"