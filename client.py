
# Program Name: client.py
# Author: Howard Chen
# Date created: 7-15-2017
#
# Description:	Client for a network application that emulates a basic FTP
#
# Sources for this program:
#	1. https://docs.python.org/3.5/library/socket.html, for explanations of how to
#		use the socket functions, and how to read/write to file
#	2. www.tutorialspoint.com/python3/python_networking.htm, for explanations of how to
#		use the socket functions
#	3. Computer Networking: A Top-Down Approach, by Kurose
#		and Ross, for examples of socket API usage
#	4. http://stackoverflow.com/questions/23459095/check-for-file-existence-in-python-3
#		for how to check if a file with a specified filename already exists

from socket import *
import readline
import sys
import os

def main():
	# Ensure that arguments are nominally valid
	valid = check_args(sys.argv)
	if valid == False:
		print("Incorrect arguments")
		sys.exit(1)

	#First, attempt to connect to the specified server on the specified port
	serverName = sys.argv[1]
	serverPort = sys.argv[2]
	try:
		control_sock = connect_to(serverName, serverPort)
	except:
		print("Failure to connect to specified host on specified port")
		sys.exit(1)

	#Next, attempt to listen on the specified port for the possible data connection
	try:
		data_port = sys.argv[len(sys.argv) - 1]
		data_sock = listen_on(data_port)
	except:
		print("Failure to listen on specified port")
		control_sock.close()
		sys.exit(1)

	#Since we have a control socket to the server, we can send it the command parameters
	commands = parse(sys.argv)
	sendCommands(control_sock, commands)
	
	#Now that all the commands have been sent, check with the server that the commands are okay
	result = check(control_sock, serverName, serverPort)
	if result == False:
		data_sock.close()
		control_sock.close()
		sys.exit(1)
	
	#If commands were okay, keep trying to accept the server's attempt to connect to the data socket
	while True:
		connect_sock, addr = data_sock.accept()
		break

	#Do some printing for the hooman so they know what's going on
	command = sys.argv[3] 
	if command == "-l":
		print("Receiving directory structure from " + serverName + ":" + data_port)
	elif command == "-g":
		filename = sys.argv[4]
		print("Receiving " + filename + " from " + serverName + ":" + data_port)
	else:
		print("Invalid command")
		connect_sock.close()
		data_sock.close()
		control_sock.close()
		sys.exit(1)
			

	#Then read all the data the server has to send
	message = receive(connect_sock)

	#Now write the message to a file, or print to console, depending on whether
	# command was "-l" or "g-"
	if command == "-l":
		print(message)
	elif command == "-g":
		filename = sys.argv[4]
		write_to_file(message, filename)
	else:
		print("Invalid command error")
	
	#Done, so close all sockets
	connect_sock.close()
	data_sock.close()
	control_sock.close()


	sys.exit(0)

# Description: writes a string to the specified file
# args: [1] message: string to write to file
#	[2] filename: file to write to
# pre: none
# ret: none
# post: if filename is unique in current directory, the message
# 	will be written to it. Otherwise the filename will be
# 	extended by * until it is unique, and then the message
#	will be written to it.
def write_to_file(message, filename):
	#Extend the filename until it is unique in the current directory
	if os.path.exists(filename) == True:
		print("Duplicated file name...")
	while os.path.exists(filename) == True:
		filename = filename + "_1"
	with open(filename, 'w') as f:
		f.write(message)	
	print("File transfer complete. File was written to \"" + filename + "\"")

# Description: Receives a confirmation message from server, prints it, and returns it
# args: [1] socket: open socket to the server
#	[2] serverName: name of server host
#	[3] serverPort: port server host is listening on
# pre: socket must be valid and open to server
#	Server must send a confirmation message in sync with the call to this function
# ret: True if confirmation was GOOD, False otherwise
# post: message sent by server will be written to console.
def check(socket, serverName, serverPort):
	# Receive and print message
	message = receive(socket)

	if message == "GOOD":
		return True
	else:
		print(serverName + ":" + serverPort + " says")
		print(message)
		return False

# Description: recevies data from socket until null terminator @@@ is received
# args: [1] socket: open socket to some host
# pre: socket is open, other host usese @@@ to terminate data chunks
# ret: string containing the data that the other host sent
# post: none
def receive(socket):
	# Get the data
	message = ""
	chunk = ""
	while "@@@" not in chunk:
		chunk = socket.recv(1024)
		chunk = chunk.decode()
		message = message + chunk
	#Then strip off the terminator
	if message.endswith("@@@") == True:
		message = message[:-3]	
	return message

# Description: Makes localhost listen on the specified port
# args: [1] port: a string representing the port number to listen on
# Pre: none
# ret: A socket object representing the local host listening on the port
# post: caller will have to close the returned socket object
def listen_on(port):
	server_sock = socket(AF_INET, SOCK_STREAM)	
	server_sock.bind(('', int(port)))
	server_sock.listen(5) #For now, only allow 5 requests to queue!

	return server_sock

# Description: parses the command line parameters of this program for the actual arguments to
#		be sent to the server
# args: [1] argv: list of command line arguments
# pre: everything from argv[3] and onward is a desired argument. Everything argv[2] and below
# 	is not desired
# ret: a list containing the desired commands
# post: none
def parse(argv):
	argc = len(argv)
	commands = []
	# build a list of actual commands to send to the server
	for index in range(3, argc):
		commands = commands + [argv[index]]
	return commands

#Description: Sends each command to the server
# Args: [1] control_sock: open socket to the server
#	[2] commands: list of commands to send to server
# Pre: Server should be using @@@ to know that a data chunk is complete
# ret: none
# post: none
def sendCommands(control_sock, commands):
	for item in commands:
		control_sock.send(item.encode())
		control_sock.send(" ".encode())
	control_sock.send("@@@".encode())  #Lets receiver know when all commands have been sent

# Description: Connects to a host at a given port
# args: [1] host: string representing host to connect to
#	[2] port: string representing port the host is listening on
# pre: host should be listening on the specified hostname and port, already
# ret: socket object representing connection to the other host
# post: caller will have to close this socket object to free the resources
def connect_to(host, port):
	client_socket = socket(AF_INET, SOCK_STREAM)
	client_socket.connect((host, int(port))) #This should raise an exception if it fails

	return client_socket

# Description: checks command line arguments to the program
# args: [1] argv: list of command line arguments (which should be strings)
# pre: command line arguments should all be strings
# ret: True if commands are properly formatted. False otherwise.
# post: none.
def check_args(argv):
	#Check the number of arguments and make sure the commands are correct
	argc = len(argv)
	if argc == 5:
		if argv[3] != "-l":
			return False
	elif argc == 6:
		if argv[3] != "-g":
			return False
	else:
		return False
	# Return 1 if everything worked correctly
	return True


main()
