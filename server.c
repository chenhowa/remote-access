/*
 * Filename: server.c
 * Author: Howard Chen
 * Date Created: 8-1-2017
 * Description: Implements server side of a simple network application that allows for basic
 * 	file transfer. Very basic emulation of FTP 
 *
 * Citation: Heavy uses of Beej's guide to help me set up the sockets and connections
 * 	using the appropriate C libraries
 *
 * 	Used lecture notes from CS 344 to figure out how to read all the file names
 * 	in the current directory
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h> /* for open and create functions*/
#include <unistd.h> /*for the close function */
#include <assert.h>
#include <dirent.h>

#define MAX_CHAR 1000 /*max number of characters a command message from client can be */
#define MAX_ARG 100 /*Max number of space-delimited parameters a command from client can have */

/*Prototypes. See function implementations for comments */
int send_to (int socket, char* message);
void executeCommand(int data_socket, char* command_list[]);
int parse(char commands[], char* command_list[], int max);
int listen_on(char*  port);
int validate(int argc, char* argv[]);
void hearCommands(int client, char commands[], int count);
int confirmCommands(int client, char* commands[], int count, char* hostname);
int connect_to(char* hostname, char* portnum);

int main(int argc, char* argv[]) {
	int server; /*file descriptor of server */
	int valid;
	char* port; /*server port number */

	/*Needed to accept connections */
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int client;

	/*Needed for processing client commands */
	char commands[MAX_CHAR];
	char* command_list[MAX_ARG];
	int status;

	/*To store information about client, and to set up data connection */
	char clientname[1024];
	char service[20];
	int data_socket;
	char* data_port_num;

	/*Check that arguments are valid */
	valid = validate(argc, argv);
	if(valid == 0) {
		perror("Invalid command line arguments\n");
		exit(1);
	}

	/*Since arguments appear to be valid, attempt to listen on the specified port */
	port = argv[1];
	server = listen_on(port);
	printf("Server open on %s\n", port);

	/*Listen forever */
	while(1) {
		/*Accept just one connection */
		addr_size = sizeof(their_addr);
		client = accept(server, (struct sockaddr*)&their_addr, &addr_size);
		if(client == -1) {
			perror("Client somehow failed to connect\n");
			continue;
		}
		/*Get client host name */
		addr_size = sizeof(their_addr);
		status = getnameinfo((struct sockaddr*)&their_addr, addr_size, clientname,
			sizeof(clientname), service, sizeof(service), 0); 	
		if(status != 0) {
			perror("Failure to get client hostname\n");
			close(client);
			close(server);
			exit(1);
		}
		printf("\n\nConnection from %s\n", clientname);

		/*Get the commands from the client */
		memset(commands, '\0', sizeof(commands));
		hearCommands(client, commands, MAX_CHAR - 2);
		/*parse the commands */
		memset(command_list, 0, sizeof(command_list)); 
		argc = parse(commands, command_list, MAX_ARG - 2);
		if(argc == 0) {
			perror("No commands received!\n");
			close(client);
			continue;
		}

		/*Verify commands and inform client of verification (or failure) */
		status = confirmCommands(client, command_list, argc, clientname);
		/*If failure to confirm command, print error and terminate session with client */
		if(status < 0)  {
			printf("Client commands failed\n");	
			close(client);
			continue;
		} 

		
		/* Use the client hostname and the received data port to establish data connection */
		data_port_num = command_list[argc - 1];
		data_socket = connect_to(clientname, data_port_num);	

		/*Since we have the data socket, we can now execute the command over it */	
		executeCommand(data_socket, command_list);

		/*Now that the command has been executed, we can end the connection
 * 			and listen for the next connection */
		close(client);
		close(data_socket);
	}	

	return 0;
}

/* Description: Sends data to client based over the data connection based on command
 * Args: [1] data_socket: file descriptor to an open tcp connection between server and client
 *	[2] command_list: array of char* representing commands that client sent to server
 * pre: data_socket must be an open connection
 * ret: none
 * post: specified client commands will have been executed, and any needed data sent
 * 	to client over the socket. For more details, see implementation
 *
 * 	Citation: code for examining file names in a directory is from
 * 		lecture notes for CS 344
 */
void executeCommand(int data_socket, char* command_list[]) {
	char* command = command_list[0];
	
	DIR* dir;
	struct dirent *file;

	FILE *fp;
	char* filename;
	char message[500];

	int filenum = 0;

	if( strcmp(command, "-l") == 0) {
		/*Get the current directory and send filenames inside to the client */
		dir = opendir(".");
		if(dir != NULL) {
			file = readdir(dir);
			while( file != NULL) {
				/*Send filename into data_socket */
				if(filenum != 0) {
					send_to(data_socket, "\n");
				}
				send_to(data_socket, file->d_name);

				filenum++;
				file = readdir(dir);	
			}

		}
		
		/*Now that the entire dir listing has been sent, send the terminator */
		send_to(data_socket, "@@@");

		/*Close the directory */
		closedir(dir);
	}
	else if ( strcmp(command, "-g") == 0) {
		/*Open the file and send the stream to the server */
		filename = command_list[1];
		fp = fopen(filename, "r" );

		if(fp == NULL) {
			perror("Problem opening file\n");
			exit(1);
		}

		/*If file open was successful, read in contents a chunk at a time
 * 			and send them over to the client */
		/*While the read is successful, continue reading and sending */
		memset(message, '\0', sizeof(message));
		while( fgets(message, 500-5, fp) != NULL ) {
			send_to(data_socket, message); 
			memset(message, '\0', sizeof(message));
		}

		/*At this point all the characters in the file have been read,
 * 			so send the terminator string */
		send_to(data_socket, " @@@");

	}
	else {
		perror("An invalid command somehow was executed!\n");
		exit(1);
	}

	return;
}

/* Description: function for sending an entire string into a socket
 * args: [1] socket: a file descriptor to an opened tcp connection
 * 	[2] message: string to send through the socket
 * pre: socket should be valid and opened
 * ret: int: -1 if error occured; 0 otherwise
 * post: entire message will have been sent into socket  
 *
 *  	Citation: Borrowed largely from Beej's guide */
int send_to (int socket, char* message) {
	int total = 0;
	int bytesleft;
	int length;
	int n;

	length = strlen(message);
	bytesleft = length;

	/*While the total number of bytes sent is not the length of the message
 * 		keep sending the remaining bytes */
	while(total < length) {
		n = send(socket, message + total, bytesleft, 0);
		if( n == -1) { break; } /* -1 is returned if an error occurred*/
		total += n;
		bytesleft -= n;
	}

	if( n == -1) { return -1; }
	else { return 0; }
}


/* Description: connects to a host at a given port number
 * args: [1] hostname: name of the host
 *	[2] portnum: port number host is listening on
 * pre: to connect successfully, host must be listening on the port
 * ret: int: either a file descriptor to a new socket for a new connection
 * 	to the host, or a negative integer, indicating an error occured
 * post: socket was opened. Caller will need to close it
 * 	
 * 	Citation: largely borrowed from Beej's guide
 */
int connect_to(char* hostname, char* portnum) {

	int status; /*Holds status of attempt to get server info */
	struct addrinfo hints;
	struct addrinfo *serverinfo;

	int sockfd; /*Holds file descriptor for the client socket connected to server */

	/*First, get the address info for the specified hostname/port that the 
 		client is going to contact*/
	memset(&hints, 0, sizeof(hints)); /*initialize the struct to be empty*/
	hints.ai_family = AF_UNSPEC;	/*Either IPv4 or IPv6 is acceptable FOR NOW*/	
	hints.ai_socktype = SOCK_STREAM; /* Specify that I'm looking for a TCP stream socket*/
	hints.ai_flags = AI_PASSIVE;  /*fill in the client's IP automatically please */

	/*Next, attempt to get contact information of the specified server process. */
	status = getaddrinfo(hostname, portnum, &hints, &serverinfo);
	if(status != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}

	/*If it succeeds, create the socket based on the server info */
	sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
	status = connect(sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen);

	if(status < 0) {
		fprintf(stderr, "connect error!\n ");
		return status;
	}

	/*Give socket file descriptor to caller.*/
	return sockfd;
}

/* Description: parses a string of space-delimited commands
 * args: [1] commands: an array of chars containing the space-delimited commands
 * 	[2] command_list: an array of char*, where each char* will point to a command
 * 	[3] max number of commands that command_list can hold
 * pre: max should be no larger than the length of command_list
 * ret: int: the number of commands parsed 
 * post: none
 */
int parse(char commands[], char* command_list[], int max) {
	int count = 0;
	char* word = NULL;
	char* rest = commands;

	memset(command_list, 0, max);
	/*While there are more parameters and the command_list is not full,
 * 		keep getting and storing parameters */
	word = strtok_r(rest, " \t", &rest);
	while(word != NULL && count < max) {
		command_list[count] = word;
		count++;

		/*Get next parameter */
		word = strtok_r(rest, " \t", &rest);
	}

	/*Null-terminate the parameter list */
	if(count == max) {
		command_list[count - 1] = NULL;
		count--; /*Since we replaced the last parameter */
	}		
	else {
		command_list[count] = NULL;
	}

	return count;

}

/* Description: Confirms the validity or invalidity of the client's commands.
 * args: [1] client: open socket to the client
 * 	[2] commands: array of char* commands from the client
 * 	[3] count: number of commands in [2]
 * 	[4] hostname: name of the client
 * pre: client socket is open
 * 	count is no larger than the length of commands
 * ret: int: 0 if commands were good, negative integer otherwise
 * post: client will be informed of whether commands were good or bad
 */
int confirmCommands(int client, char* commands[], int count, char* hostname) {
	char* comm = commands[0];
	char* message = NULL;
	FILE *fp;
	char* filename;
	char* data_port;

	int status;

	data_port = commands[count - 1];
	if (strcmp(comm, "-g") == 0) {	
		/*Attempt to open the specified file. If failure, tell client */
		filename = commands[1];
		printf("File \"%s\" requested on port %s.\n", filename, data_port);		

		fp = fopen(filename, "r");

		/*If file could not be opened, send error message to client */
		if(fp == NULL) {
			message = "FILE NOT FOUND";
			status = -1;
			printf("File not found. Sending error message to %s:%s\n", hostname, data_port);
		}
		else {
			/*Otherwise commands are good! */
			message = "GOOD";
			status = 0;
			printf("Sending \"%s\" to %s:%s\n", filename, hostname, data_port);
		}
	}

	/*Since no failure if command was recognized, and filename was good, we are good to go */
	else if (strcmp(comm, "-l") == 0) {
		message = "GOOD";
		status = 0;
		printf("List directory requested on port %s\n", data_port);
		printf("Sending directory contents to %s:%s\n", hostname, data_port);
	}
	else {
		message = "BAD";
		status = -2;
		printf("Unknown error. Sending error message to %s:%s\n", hostname, data_port);
	}

	/*Now that message has been determined, send it */
	send_to(client, message);
	send_to(client, "@@@");
	return status;
}

/* Description: gets a string of commands from the client
 * args: [1] client: open socket to the client
 * 	[2] commands: an array for storing commands fread from client socket
 * 	[3] count: max number of characters that can be stored in commands
 * pre: client socket should be open
 * 	count is no larger than actual length of commands array
 * ret: none
 * post: commands sent by client will be stored in the commands array
 */
void hearCommands(int client, char commands[], int count) {
	int totalBytes = 0;
	int bytesRead = 0;
	char* start;
	int index;

	memset(commands, '\0', count);
	start = commands;

	/*While the buffer isn't full and the terminating string hasn't been received,
 * 		continue reading commands */
	while(totalBytes < count && strstr(start, "@@@") == NULL ) {
		/*Point start to the next open portion of commands */
		start = start + strlen(commands);

		bytesRead = recv(client, (void*)start, count - totalBytes - 2, 0);
		if(bytesRead == -1) {
			perror("Failed to receive commands\n");	
			exit(1);
		}	

		totalBytes = totalBytes + bytesRead;
	}

	/*At this point, the entire command list should have been read */
	/*Get rid of the terminator and return */
	start = strstr(start, "@@@");
	for(index = 0; index < 3; index++) {
		start[index] = '\0';
	}

	return;
}

/* Description: listen on the localhost at the specified port
 * args: [1] port: the port to listen on
 * pre: none
 * ret: file descriptor to a socket that is listening on the specified port
 * post: caller must close the returned file descriptor
 *
 * 	Citation: This code is almost entirely from Beej's guide */
int listen_on(char* port) {
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	int socketFD;

	char hostname[1000];
	memset(hostname, '\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname));

	memset(&hints, 0, sizeof(hints) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/*First, get info of the localhost at the desired port */
	status = getaddrinfo(NULL, port, &hints, &servinfo);
	if(status != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status) );
		exit(1);
	}

	/*Now that we have the localhost info, use it to create a socket */
	socketFD = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);	
	if( socketFD < 0) {
		perror("Failed to create a socket\n");
		exit(1);
	}
	
	/*Now that we have a socket, we bind it to a local port*/
	status = bind(socketFD, servinfo->ai_addr, servinfo->ai_addrlen);
	if(status < 0) {
		perror("Failed to bind socket to a port\n");
		exit(1);
	}

	/*Now we can listen! Let queue go up to 10 */
	status = listen(socketFD, 10);
	if(status < 0) {
		perror("Failed to listen correctly\n");
		exit(1);
	}

	/*We have a file descriptor to a listening socket. So return it */

	return socketFD;
}

/* Description: Validates the command line arguments given to the program
 * args: [1] argc: number of arguments in argv
 * 	[2] argv: array of char* arguments given on the command line
 * pre: none
 * ret: if more or less than 2 arguments, return 0
 * 	if 2nd argument cannot be converted to an integer, return 0
 * 	otherwise return 1
 */
int validate(int argc, char* argv[]) {
	/*Check that there are exactly two arguments */
	if(argc != 2) {
		return 0;
	}

	/*Check that the second argument can be casted as an integer */
	if(atoi(argv[1]) == 0) {
		return 0;
	}

	return 1;

}
