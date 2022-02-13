### REPORT ###

This is a drone flying simulation. 

Such simulation takes place in a 80 x 40 meters field and the battery 
drain/charge is simulated too. While flying, the battery drains 
and when the drone runs out of battery, it stops and starts recharging. 
Moreover, you can set the drone's velocity by selecting the following commands:

	[1] SLOW
	[2] DEFAULT
	[3] FAST
	[3] VERY FAST

Obviously, the faster the drone is, the faster the battery drain will be!

You can have a GUI feedback of the battery state and of the drone visited/non-visited 
position too. Therefore a graphical tool with 'ncurses.h' library has been implemented! 
Runtime we modify and show a 80x40 bool matrix: 
	
	- Green '0' -> 	Positions not visited yet
	- Red 	'1' -> 	Visited positions

The drone has a coverage algorithm too: at first we increment its 'x' coordinate 
always by one and randomly the 'y' coordinate is incremented in a -1 , +1 range. 
When the drone runs out of battery, it stops, it starts recharging and 
finally it change the non-randomic visiting direction: the 'y' coordinate is always 
incremented by one and randomly the 'x' coordinate is incremented in a -1 , +1 range.
The drone always tries to move towards the position not yet visited but when this is
not possible it simply moves in the above described way. 

Finally, the drone movement relies on the master process that actually 
acts as a server for all the drones that connects with the master 
process through socket. A communication protocol has been established 
among all the assignment partecipants and the result is that all the drones
correctly move into the 80x40 field without 
