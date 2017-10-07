This document describes how to run the file transfer program using client.py and server.c

****NOTE****: When testing this document, the text files you use should NOT CONTAIN the string "@@@". I used it as a terminator to
let the client and server know when each was done sending data. Thus, I have included the text files "short.txt" and "long" that you can use to test the program's functionality. Neither file contains a single occurence of "@@@"

I have also included a makefile, whose use is described below under COMPILATION.

COMPILATION
To compiler server.c, run the command "make" in the command line to generate the executable "prog"
To restore the directory to its original state before the "make" command, run "make clean"

RUNNING THE APPLICATION
First, log in to the terminal at flip1.engr.oregonstate.edu, and navigate to the directory containing "prog".

To start up the server, enter a command in the form "./prog <port_number>" to start the server running 
on the local host on the specified port number. For example, the command "./prog 5045" will start the
server running on the local host on port 5045.
The server will continue listening for client connections until it is terminated using
Ctrl-C (SIGINT)

To start up the client (FIRST START THE SERVER), open a separate terminal and log in at flip3.engr.oregonstate.edu, and navigate to the directory containing "client.py". You have two options for valid commands:
	1. "python3 client.py <server_name> <server_port> -l <data_port>"
		For example, running
			"python3 client.py flip1.engr.oregonstate.edu 5045 -l 5066"
		will launch the client, which will connect to the server at
		flip1.engr.oregonstate.edu on port 5045, like described above. The server will then
		send a directory listing to the client using the client port 5066.

	2. "python3 client.py <server_name> <server_port> -g <filename> <data_port>"
		NOTE: <filename> refers to a text file. Binary files are not supported.
		NOTE: <data_port> must not be the same as <server_port>

		For example, running
			"python3 client.py flip1.engr.oregonstate.edu 5045 -g potato 5066"
		will launch the client, which will connect to the server at
		flip1.engr.oregonstate.edu on port 5045, like described above. The server will
		check whether or not it has a file with the name "potato".
		
		If the server doesn't have a file by this name, it will send an error message
		to the client, and the client terminates.

		If the server does have a file by this name, it will send the file data
		to the client, which will save the data under its original file name, if that
		file name is available in the current directory. If it is not available, the
		client will add a "_1" suffix to the end of the file name, and save it under
		this new file name. Then the client terminates.

