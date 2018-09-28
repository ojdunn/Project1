/*
 * Michael Weir, Owen Dunn, Anthony Nguyen
 * Programming Project 1: Reliable File Transfer over UDP
 * Cis 457
 * Fall 2018
 *
 * Server:
 * When started, the server asks for a port number. The server then listens
 * for connections on that port. The sliding window protocol is implemented
 * to allow reliable file transfer with UDP. A window size of 5 is used.
 * 
 * Run: ./exename portnumber 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include "project1.h"  // implementation in .h file

int createUDPSocket(int portnumber, struct sockaddr_in serveraddr);

int main(int argc, char** argv) {
	printf("--------------UDP file transfer server--------------\n");
	printf("Sent file requests must be in the current directory.\n");
	printf("Run: ./exename portnumber\n");

	int portnumber;
	swpState swp;  // holds state values for sliding window protocol
	// Allow command line input. (save time testing)
	if (argc == 2) {
		portnumber = atoi(argv[1]);
		if (!isValidPort(portnumber)) {
			return -1;
		}
		printf("Using port: %d\n", portnumber);
	} else {
		printf("Invalid command line input.\n");
		printf("Run: ./exename portnumber \n");
	}

	FILE *fp;	
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("There was an error creating the socket.\n");
		return 1;
	}

	struct timeval timeout;
	timeout.tv_sec = 5;   // seconds
	timeout.tv_usec = 0;  // micro sec

	//set options sockfd, option1, option2
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, 
		sizeof(timeout));

	struct sockaddr_in serveraddr, clientaddr;

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(portnumber);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	int b = bind(sockfd,(struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (b < 0) {
		printf("failed to bind\n");
		return 2;
	}

	long fileSize = 0;
	struct stat fileStats;
	int elementsRead = 0;
	int packetsSent = 0;
	int n;  	  // wait timing variable
	swp.LAR = 0;  // seqnum of last ack received
	swp.LFS = 0;  // seqnum of last frame (packet) sent
	int send_i; // how many frames sent and not ack yet
	int server_seqnum = 0;  // start at 1
	// char hdr[3];
	// createHeader(hdr, 1, 1);
	// printf("%s\n", hdr);

	swpState serverState;  // has variables to handle the sliding window protocol
	int transferComplete;  // flag to mark end of file transfer
	int sizeReceived;      // flag to know if file size received by client
	int acksReceived;	   // acknowledgements received
	while(1) {
		socklen_t  len = sizeof(clientaddr);
		char line[PACKET_DATA_SIZE];
		char buffer[PACKET_DATA_SIZE];
		// Stop and wait for a file request. 
		// (timer used to quit waiting after 5s)
		int n = recvfrom(sockfd, line, WINDOW_SIZE * PACKET_DATA_SIZE, 0, 
			(struct sockaddr*)&clientaddr, &len);
		if(n == -1) {  // waited 5s
			printf("Time out on receive (5 seconds)\n");
		}
		else {
			printf("Got file request from client: %s\n", line);
			if((fp = fopen(line, "r")) == NULL){  // returns file pointer (fp)
				printf("File does not exist\n");
			}	
			else{
				// Get the file size.
				int status = stat(line, &fileStats);
				if (status == 0) {
					fileSize = fileStats.st_size;
					printf("File \"%s\" size: %ld bytes\n", line, fileSize);
				} else {
					printf("Failed to read file size.\n");
					return -1;
				}
				// Send file size to client.
				sprintf(buffer, "%ld", fileSize);
				//printf("file size: %s\n", buffer);
				sizeReceived = 0;
				while ( !sizeReceived ) {
					n = sendto(sockfd, buffer, strlen(buffer)+1, 0, 
							(struct sockaddr*)&clientaddr, sizeof(clientaddr));
					
					// TODO Wait for an ack for the file size msg, resend if missed.
					sizeReceived = 1;  // TODO, leave 1 for now 
				}
				clearBuffer(buffer);
				// Read and send the file: packet by packet. 
				// (how many data packets)
				int totalPackets = 
					(int)ceil((double)fileSize / PACKET_DATA_SIZE);
				
				// Read the first 5 packets.
				// if (totalPackets >= 5) {
				// 	fread(buffer, 1, PACKET_DATA_SIZE, fp);
				// } else if (totalPackets < 5 && totalPackets != 0) {
				// 	fread(buffer, 1, PACKET_DATA_SIZE, fp);
				// } else {
				// 	printf("No data to be read!");
				// 	continue;
				// }
				// while (  ) {

				// }

				// Read and send all the file data 1024 bytes at a time.
				// Add header necessary data to handle error and use queues to
				// resend data when necessary (loop over q looking for needed
				// packet).
				transferComplete = 0;  // true when last ACK received
				while ( !transferComplete ) { // ack received for all packets
					// if (elementsRead != PACKET_DATA_SIZE) {
					// 	printf("ERROR! File read error!");
					// 	return -1;
					// }
					//printf("Read %d bytes from a file.\n", elementsRead);

					if ( !feof(fp) ) {  // if haven't read all of file yet
						// If read at least one element.
						if ((elementsRead = fread(buffer, 1, PACKET_DATA_SIZE, fp)) > 0 ) {
							sendto(sockfd, buffer, elementsRead, 0, 
								(struct sockaddr*)&clientaddr, sizeof(clientaddr));
							printf("Sent packet with this data.\n");
							packetsSent++;  

							// add data to the sendQ
							strcpy(swp.sendQ[send_i].msg, buffer);
							// add the header (using struct for now)
							//createHeaderStructure(&swp.sendQ[send_i].hdr, server_seqnum, 0);
							char hdr[3];
							createHeader(hdr, server_seqnum, 0);  // TODO
							server_seqnum = (server_seqnum + 1) % 10;  // move server seqnum up one
						} else {
							printf("File completely read at server.\n");
						}
					} 

					

					if ( acksReceived == totalPackets ) {
						transferComplete = 1;
					}
					if (packetsSent == totalPackets) {  // TODO, leave until swp done
						transferComplete = 1;
					}

					clearBuffer(buffer);
				}
				//printf("Sent %d packets\n", packetsSent);
				packetsSent = 0;

				// Close the socket when done reading file to allow client to
				// complete its file transaction.
				//close(sockfd);
			
				//printf("%s\n", buffer);  //  should only run for text files

				// Append a header to the front of the file? (first x bits)

				fclose(fp);  // close the file
				// create a new socket to allow another client
				//sockfd = createUDPSocket(portnumber, serveraddr);
			}	
		}
	}

	return 0;
}

int createUDPSocket(int portnumber, struct sockaddr_in serveraddr) {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("There was an error creating the socket.\n");
		exit(1);
	}
	struct timeval timeout;
	timeout.tv_sec = 5;   // seconds
	timeout.tv_usec = 0;  // micro sec

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, 
			   sizeof(timeout));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(portnumber);
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	int b = bind(sockfd,(struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (b < 0) {
		printf("failed to bind\n");
		exit(1);
	}

	return sockfd;
}
