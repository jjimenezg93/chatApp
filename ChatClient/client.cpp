#include <stdio.h>
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"

#define SERVER_PORT "12345"
#define BUFFER_SIZE 4096
const char * EXIT_COMMAND = "/q";

int main(int argc, char *argv[]) {
	SOCKET socketHandle;
	WSADATA wsaData;
	char inputBuffer[BUFFER_SIZE];
	memset(inputBuffer, 0, sizeof(inputBuffer));
	
	if (argc < 2)
		return -1;

	//init WSA
	int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaErr) {
		printf_s("Couldn't initialize WSA.\n");
	}

	addrinfo * servInfo;
	addrinfo hints; //restrictions
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//get all reachable servers info
	int servInfoErr = getaddrinfo(argv[1], SERVER_PORT, &hints, &servInfo);
	if (servInfoErr) {
		printf_s("Couldn't reach server");
		WSACleanup();
		return -1;
	}

	addrinfo * dstServer = servInfo;
	int32_t connInfo;
	while (dstServer != nullptr) {
		socketHandle = socket(dstServer->ai_family, dstServer->ai_socktype,
			dstServer->ai_protocol);
		if (socketHandle == INVALID_SOCKET) {
			printf_s("Invalid socket");
		} else {
			connInfo = connect(socketHandle, dstServer->ai_addr, (int)dstServer->ai_addrlen);
			if (connInfo == SOCKET_ERROR) {
				printf_s("Couldn't establish connection with socket %d", socketHandle);
				closesocket(connInfo);
				socketHandle = INVALID_SOCKET;
				return -1;
			}
			break;
		}
		dstServer = dstServer->ai_next;
	}
	freeaddrinfo(servInfo);
	if (socketHandle != INVALID_SOCKET) {
		int32_t messageLength;
		int32_t bytesSent = 0;
		int32_t totalSent = 0;

		printf_s("Welcome! Your name: ");
		while (1) {
			gets_s(inputBuffer, _countof(inputBuffer));
			if (inputBuffer == "") {
				printf_s("\nEnter a valid name, please. \n");
			} else {
				messageLength = strlen(inputBuffer);
				while (bytesSent < messageLength) {
					bytesSent += send(socketHandle, inputBuffer + bytesSent, messageLength, 0);
				}
				bytesSent = 0;
				break;
			}
		}
		system("cls");
		printf_s("Type and press Enter to send your message: \n");
		while (1) {
			//get input
			memset(&inputBuffer, 0, BUFFER_SIZE);
			gets_s(inputBuffer, _countof(inputBuffer));
			//exit command
			if (!strcmp(inputBuffer, EXIT_COMMAND)) {
				//send to the server a message to close
				break;
			}
			totalSent = 0;
			bytesSent = 0;
			messageLength = strlen(inputBuffer);
			do {
				bytesSent = send(socketHandle, inputBuffer + totalSent, messageLength, 0);
				totalSent += bytesSent;
			} while (totalSent < messageLength);
			memset(&inputBuffer, 0, BUFFER_SIZE);
			recv(socketHandle, inputBuffer, BUFFER_SIZE, 0);
			printf_s("%s\n", inputBuffer);
		}
		closesocket(socketHandle);
	}

	return 0;
}