Alexei Doell cka067 11345642

After running make, one executable will be placed as a symlink
in the current directory:
 - router

Router =========================================================================

In order to run the receiver, you must give it a port number to connect to, as
well as ports for each of its neighbors and costs associated with each neighbor. 
For the usask network, a port between 30000 and 40000 must be chosen for all ports.
$ ./router <self_port> <other_port_1> <other_cost_1> ... <other_port_n> <other_cost_n> 

E.g. "./router 34920 33000 5" will setup the router to bind to port 34920 and
start sending and receiving from its neighbor on port 33000 associated with a
cost of 5.

All routers must be ran on one single machine, as hostnames are not taken into
account.

--------------------------------------------------------------------------------

In order to close the router, simply input ctrl-C which will cause the program to
cleanup itself then exit.
