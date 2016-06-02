#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"

#define SERVER_PORT 12345
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 10

struct Client {
	SOCKET socketId;
	std::string clientName;
};

void * clientThread(void * clientData) {
	Client * client = reinterpret_cast<Client *>(clientData);
	printf_s("Client's socket = %d || ", client->socketId);
	char buffer[BUFFER_SIZE];
	memset(&buffer, 0, BUFFER_SIZE);
	int32_t rec = 0;
	int32_t totalRec = 0;
	recv(client->socketId, buffer, BUFFER_SIZE, 0);
	client->clientName = buffer;
	memset(&buffer, 0, strlen(client->clientName.c_str()));
	printf_s("Client's name = %s\n", client->clientName.c_str());
	while (1) {
		totalRec = 0;
		rec = 0;
		memset(&buffer, 0, BUFFER_SIZE);
		do {
			rec = recv(client->socketId, buffer + totalRec, BUFFER_SIZE, 0);
			totalRec += rec;
			printf_s("\nrec:%d | totalRecv: %d\n", rec, totalRec);
			printf_s("Buffer: %s", buffer);
			printf_s("Buffer[totalRec-1]: %d", buffer[totalRec - 1]);
			printf_s("Buffer[totalRec]: %d", buffer[totalRec]);
		} while (buffer[totalRec] != '\0');
		//add to message's list, then send all pending messages (when can access this data
		std::string answer = std::string(client->clientName).append(": ").append(buffer);
		send(client->socketId, answer.c_str(), answer.size(), 0);
	}
	pthread_exit(nullptr);
	return 0;
}



int main() {
	WSADATA wsaData;
	int on = 1;
	SOCKET listeningSocket;
	std::vector<Client *> clients;
	sockaddr_in servInfo;
	sockaddr_in clientInfo;
	char buffer[BUFFER_SIZE];

	//init WSA
	int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaErr) {
		printf_s("Couldn't initialize WSA.\n");
	}

	memset(&servInfo, 0, sizeof(servInfo));
	servInfo.sin_family = AF_INET;
	servInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	servInfo.sin_port = htons(SERVER_PORT);

	listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listeningSocket < 0) {
		printf_s("Error creating listening socket.\n");
		WSACleanup();
		return -1;
	}

	setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));

	if (bind(listeningSocket, (sockaddr *)&servInfo, sizeof(servInfo)) < 0) {
		printf_s("Error binding listening port.\n");
		WSACleanup();
		return -1;
	}

	if (listen(listeningSocket, QUEUE_SIZE) < 0) {
		printf_s("Listen failed.\n");
		WSACleanup();
		return -1;
	}

	printf_s("CHAT CREATED SUCCESFULLY\n");

	while (1) {
		socklen_t sock_len = sizeof(clientInfo);
		memset(&clientInfo, 0, sizeof(clientInfo));
		Client * newClient = new Client();
		newClient->socketId = accept(listeningSocket, (sockaddr *)&clientInfo, &sock_len);
		if (newClient->socketId != INVALID_SOCKET) {
			char clientIP[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, sizeof(clientIP));
			printf_s("Accepted connection from %s\n", clientIP);
			clients.push_back(newClient);
			pthread_t newThread;
			pthread_create(&newThread, nullptr,
				clientThread, reinterpret_cast<void *>(newClient));
		}
	}

	getchar();
}