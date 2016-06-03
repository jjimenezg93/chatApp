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
#define EXIT_COMMAND "/q"

struct Client {
	Client(): socketId(0), clientName(""), lastMessageReceived(0) {}
	SOCKET socketId;
	std::string clientName;
	int32_t lastMessageReceived;
};

std::vector<Client *> gClients;
pthread_mutex_t t_clientsLock = PTHREAD_MUTEX_INITIALIZER;
std::vector<std::string> gChatHistory;
pthread_mutex_t t_chatHistoryLock = PTHREAD_MUTEX_INITIALIZER;

const uint32_t ClientsSize() {
	pthread_mutex_lock(&t_chatHistoryLock);
	uint32_t size = gClients.size();
	pthread_mutex_unlock(&t_chatHistoryLock);
	return size;
}

const uint32_t ChatHistorySize() {
	pthread_mutex_lock(&t_chatHistoryLock);
	uint32_t size = gChatHistory.size();
	pthread_mutex_unlock(&t_chatHistoryLock);
	return size;
}

const std::string ReadMessage(const int index) {
	pthread_mutex_lock(&t_chatHistoryLock);
	std::string retStr = gChatHistory.at(index);
	pthread_mutex_unlock(&t_chatHistoryLock);
	return retStr;
}

void InsertMessage(const std::string& str) {
	pthread_mutex_lock(&t_chatHistoryLock);
	gChatHistory.push_back(str);
	pthread_mutex_unlock(&t_chatHistoryLock);
}

//creates local copy of client's info and only needs access to gClients to set their name
void * thread_clientManager(void * clientData) {
	Client * clientPtr = reinterpret_cast<Client *>(clientData);
	Client client;
	char buffer[BUFFER_SIZE];
	memset(&buffer, 0, BUFFER_SIZE);
	std::string newMessage;
	int32_t lastMessageRead = 0;
	int32_t rec = 0;
	int32_t totalRec = 0;
	pthread_mutex_lock(&t_clientsLock);
	recv(clientPtr->socketId, buffer, BUFFER_SIZE, 0);
	clientPtr->clientName = buffer;
	client = *clientPtr;
	pthread_mutex_unlock(&t_clientsLock);
	printf_s("Client's socket = %d || ", client.socketId);
	printf_s("Client's name = %s\n", client.clientName.c_str());
	while (1) {
		totalRec = 0;
		rec = 0;
		memset(&buffer, 0, BUFFER_SIZE);
		do {
			rec = recv(client.socketId, buffer + totalRec, BUFFER_SIZE, 0);
			totalRec += rec;
			printf_s("\nrec:%d | totalRecv: %d\n", rec, totalRec);
		} while (buffer[totalRec] != '\0');
		if (!strcmp(buffer, EXIT_COMMAND)) {
			closesocket(client.socketId);
			printf_s("%s has disconnected\n", client.clientName.c_str());
			pthread_exit(nullptr);
			return 0;
		}
		//add message to history
		pthread_mutex_lock(&t_clientsLock);
		newMessage = client.clientName;
		pthread_mutex_unlock(&t_clientsLock);
		InsertMessage(newMessage.append(": ").append(buffer).append("\n"));
	}
	return 0;
}

//reads messages and sends new messages to each client
void * thread_serverReader(void * params) {
	int32_t messageLength;
	int32_t bytesSent = 0;
	int32_t totalSent = 0;
	int32_t clientIndex = 0;
	uint32_t lastMessage = 0;
	std::string message;
	while (1) {
		if (ClientsSize() != 0) {
			if (clientIndex >= ClientsSize()) {
				clientIndex = 0;
			}
			pthread_mutex_lock(&t_clientsLock);
			Client * clItr = gClients.at(clientIndex);
			lastMessage = clItr->lastMessageReceived;
			if (clItr != nullptr && clItr->clientName != "") {
				pthread_mutex_unlock(&t_clientsLock);
				uint32_t size = ChatHistorySize();
				if (lastMessage < size) {
					//if sent
					pthread_mutex_lock(&t_clientsLock);
					clItr->lastMessageReceived++;
					pthread_mutex_unlock(&t_clientsLock);
					printf_s("SENT");
				}
				/*pthread_mutex_lock(&t_clientsLock);
				while (clItr->lastMessageReceived < ChatHistorySize()) {
					pthread_mutex_unlock(&t_clientsLock);
					totalSent = 0;
					bytesSent = 0;
					pthread_mutex_lock(&t_clientsLock);
					message = ReadMessage(clItr->lastMessageReceived++).c_str();
					messageLength = strlen(message.c_str());
					do {
						bytesSent = send(clItr->socketId, message.c_str(), messageLength, 0);
						totalSent += bytesSent;
					} while (totalSent < messageLength);
					pthread_mutex_unlock(&t_clientsLock);
					printf_s("SENT %s\n", message.c_str());
				}*/
			} else {
				pthread_mutex_unlock(&t_clientsLock);
			}
			clientIndex++;
		}
	}
}

int main() {
	WSADATA wsaData;
	int on = 1;
	SOCKET listeningSocket;
	sockaddr_in servInfo;
	sockaddr_in clientInfo;
	char buffer[BUFFER_SIZE];
	//thread to send all updates to clients
	pthread_t t_reader;
	pthread_create(&t_reader, nullptr, thread_serverReader, nullptr);

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
			pthread_mutex_lock(&t_clientsLock);
			gClients.push_back(newClient);
			pthread_mutex_unlock(&t_clientsLock);
			pthread_t newThread;
			pthread_create(&newThread, nullptr,
				thread_clientManager, reinterpret_cast<void *>(newClient));
		}
	}

	getchar();
}