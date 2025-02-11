Alexei Doell cka067 11345642

After running make, these 2 executables will be placed as symlinks 
in the current directory:
 - receiver
 - sender

Receiver =======================================================================

In order to run the receiver, you must give it a port number to connect to. For 
the usask network, a port between 30000 and 40000 must be chosen.
$ ./receiver <port>

E.g. "./receiver 34920" will setup the receiver to begin waiting on messages on
port 34920.

Input --------------------------------------------------------------------------

Once a message is received by the receiver, you will be given this prompt: 

"Was the sender's message successfully received? (Y/N)"

Input a line starting with 'Y' treat the message as having been received successfully 
by the receiver. Any line with a different first character will treat the input as a NO.

If you say YES to the first prompt, you will then be prompted with:

"Was the ACK successfully sent back? (Y/N)"

Once again, type a line starting with 'Y' to send an ACK back to the sender.

Once the prompts have been answered, the receiver will go back to waiting for 
another message.

--------------------------------------------------------------------------------

The receiver cannot be closed through user input and will continue running 
indefinitely unless forcefully closed.

Sender =========================================================================

In order to run the sender, you must give it a timeout value in second that will
be used as the maximum amount of time the sender will wait for an ACK before 
resending its sending window, a value that will be used as the sending window size,
the hostname of the receiver, and the port of the receiver.

$ ./sender <timeout> <sending_window> <hostname> <port>

E.g. "./sender 10 5 tux7 34920" will start the sender with a timeout period of
10 seconds, a max sending window size of 5, and it will attempt to sent to the
port 34920 on tux7.

Input --------------------------------------------------------------------------

In order to send messages on the sender, simply input text and then input a newline.
This will cause your message to be sent to the receiver. If you would like to close
the sender, input a newline on a blank line.

--------------------------------------------------------------------------------
