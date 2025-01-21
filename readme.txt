Alexei Doell cka067 11345642

After running make, these 5 executables will be placed as symlinks 
in the current directory:
 - tcp_server
 - tcp_proxy
 - tcp_client
 - udp_server
 - udp_proxy

By default, the following ports are used by default and bound
for each of the non client executables:
TCP server binds 34920.
TCP proxy binds 34921
UDP server binds 34922.
UDP proxy binds 34923.

In order to run the servers, there are no command line arguments needed:
$ ./tcp_server
$ ./udp_server

In order to run the proxies and the client, they must be given a hostname and
a port for messages to be sent to:
$ ./tcp_proxy <hostname> <port>
$ ./udp_proxy <hostname> <port>
$ ./tcp_client <hostname> <port>

E.g. "./tcp_proxy tux7 34920" would connect the tcp proxy to a server being
hosted on tux7 on port 34920.

In the client, a line will be sent to the proxy/server that the client is
connected to when a newline is inputted. If the line is empty except for the
newline, the client will close. When connecting to the UDP server/proxy, ensure
that you do not send more than 255 characters, as that is the max message length
that they can handle.

The servers and proxies cannot be closed through user input and will continue
to run until forcefully closed e.g. the processes are sent a sigkill.
