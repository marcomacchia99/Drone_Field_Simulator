#!/bin/bash

read lower_port upper_port < /proc/sys/net/ipv4/ip_local_port_range

while :
do
	PORT="`shuf -i $lower_port-$upper_port -n 1`"
	ss -lpn | grep -q ":$PORT " || break
done

echo $PORT

konsole -e "./master/master $PORT" &

sleep 1

konsole -e "./FE007_3_drone/drone $PORT" &