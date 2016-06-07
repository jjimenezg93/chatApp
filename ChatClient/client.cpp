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
bool connActive = false;
pthread_mutex_t t_connLock = PTHREAD_MUTEX_INITIALIZER;

void * thread_clientReader(void * socketHandle) {
	SOCKET socket = reinterpret_cast<SOCKET>(socketHandle);
	char buffer[BUFFER_SIZE];
	int32_t errorCode;
	int32_t rec;
	int32_t totalRec;
	while (1) {
		rec = 0;
		totalRec = 0;
		do {
			rec = recv(socket, buffer + totalRec, BUFFER_SIZE, 0);
			if (rec > 0) {
				totalRec += rec;
			} else if (rec == SOCKET_ERROR) {
				errorCode = WSAGetLastError();
				if (errorCode == WSAECONNRESET || errorCode == WSAECONNABORTED) {
					printf_s("Error receiving messages from server, closing connection");
					connActive = false;
					break;
				}
			}
		} while (totalRec > 0 && buffer[totalRec - 1] != '\0');
		printf_s("\n%s", buffer);
		pthread_mutex_lock(&t_connLock);
		if (!connActive) {
			pthread_mutex_unlock(&t_connLock);
			break;
		}
		pthread_mutex_unlock(&t_connLock);
	}
	pthread_exit(nullptr);
	return 0;
}

int main(int argc, char *argv[]) {
	SOCKET socketHandle;
	WSADATA wsaData;
	char inputBuffer[BUFFER_SIZE];
	memset(inputBuffer, 0, sizeof(inputBuffer));
	pthread_t t_reader;

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
		connActive = true;
		pthread_create(&t_reader, nullptr, thread_clientReader,
			reinterpret_cast<void *>(socketHandle));

		int32_t messageLength;
		int32_t bytesSent = 0;
		int32_t totalSent = 0;
		int32_t errorCode;

		printf_s("Welcome! Your name: ");
		while (1) {
			gets_s(inputBuffer, _countof(inputBuffer));
			if (inputBuffer == "") {
				printf_s("\nEnter a valid name, please. \n");
			} else {
				bytesSent = 0;
				messageLength = strlen(inputBuffer);
				do {
					bytesSent += send(socketHandle, inputBuffer + totalSent, messageLength + 1, 0);
					totalSent += bytesSent;
				} while (totalSent < messageLength);
				break;
			}
		}
		system("cls");
		printf_s("Type and press Enter to send your message: \n");
		while (1) {
			pthread_mutex_lock(&t_connLock);
			if (!connActive) {
				pthread_mutex_unlock(&t_connLock);
				break;
			}
			pthread_mutex_unlock(&t_connLock);
			//get input
			gets_s(inputBuffer, _countof(inputBuffer)); //when closing connection from reader thread,
			//this thread is still here, so it doesn't finish until next input
			totalSent = 0;
			bytesSent = 0;
			messageLength = strlen(inputBuffer);
			do {
				bytesSent = send(socketHandle, inputBuffer + totalSent, messageLength + 1, 0);
				totalSent += bytesSent;
				if (bytesSent > 0) {
					totalSent += bytesSent;
				} else if (bytesSent == SOCKET_ERROR) {
					errorCode = WSAGetLastError();
					if (errorCode == WSAECONNRESET || errorCode == WSAECONNABORTED) {
						printf_s("Error sending messages to the server, closing connection");
						connActive = false;
						break;
					}
				}
			} while (totalSent < messageLength);
			//exit command
			if (!strcmp(inputBuffer, EXIT_COMMAND) || !connActive) {
				pthread_mutex_lock(&t_connLock);
				connActive = false;
				pthread_mutex_unlock(&t_connLock);
				break;
			}
			memset(&inputBuffer, 0, BUFFER_SIZE);
		}
		closesocket(socketHandle);
	}
	getchar();
	return 0;
}