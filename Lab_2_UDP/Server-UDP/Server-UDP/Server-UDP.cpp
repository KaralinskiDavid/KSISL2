#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

//iResult = bind(ListenSocket, (SOCKADDR*)& service, sizeof(service));

//sockaddr_in service;
//service.sin_family = AF_INET;
//service.sin_addr.s_addr = inet_addr("25.34.154.129");
//service.sin_port = htons(27015);


#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "commands.h"


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27015

typedef struct _MessageStruct
{
	char senderId, destId;
	int size;
	char message[MAX_MESSAGE_LEN];
} MessageStruct, * pMessageStruct;


BOOL initializeWinsock()
{
	int iResult;

	// Initialize Winsock
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return FALSE;
	}
	return TRUE;
}

BOOL createListenSocket(SOCKET* ListenSocket)
{
	// Create a SOCKET for connecting to server
	*ListenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}
	return TRUE;
}

BOOL setupBind(SOCKET* ListenSocket, sockaddr_in* info)
{
	int iResult;

	// Setup the TCP listening socket
	iResult = bind(*ListenSocket, (SOCKADDR*)info, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(*ListenSocket);
		WSACleanup();
		return FALSE;
	}
	return TRUE;
}

BOOL initialize(SOCKET* ListenSocket)
{
	if (!initializeWinsock())
		return FALSE;


	if (!createListenSocket(ListenSocket))
		return FALSE;

	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(DEFAULT_PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (!setupBind(ListenSocket, &server))
		return FALSE;

	return TRUE;
}

BOOL sendRequest(SOCKET ConnectSocket, sockaddr_in* recvAddr, char request)
{
	int iResult;
	iResult = sendto(ConnectSocket, &request, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}
	return TRUE;
}

BOOL sendUserDestSize(SOCKET ConnectSocket, sockaddr_in* recvAddr, char user, char dest, int size)
{
	char sendbuf[6] = {};

	sendbuf[0] = user;
	sendbuf[1] = dest;

	*((int*)(&sendbuf[2])) = size;

	int iResult;

	iResult = sendto(ConnectSocket, sendbuf, 6, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	return TRUE;
}

int recieveRequest(SOCKET ClientSocket, sockaddr_in* sender)
{
	int iResult;
	int fromLen = sizeof(sockaddr_in);
	char requestType;

	iResult = recvfrom(ClientSocket, &requestType, 1, 0, (sockaddr*)sender, &fromLen);
	if (iResult > 0) {
		printf("Request received: %d\n", requestType);
		return requestType;
	}
	else {
		printf("recv failed with error: %d\n", WSAGetLastError());
		return ERROR_REQUEST;
	}
}

BOOL getSenderInfo(SOCKET ClientSocket, char* sendId, char* destId, int* size)
{
	int iResult;

	char recvbuf[6] = {};

	iResult = recvfrom(ClientSocket, recvbuf, 6, 0, NULL, NULL);
	if (iResult != 6)
	{
		puts(ERROR_RECV);
		return FALSE;
	}

	*sendId = recvbuf[0];
	*destId = recvbuf[1];
	*size = *((int*)&recvbuf[2]);

	return TRUE;
}

void recieveMessage(SOCKET ClientSocket, pMessageStruct* messageArr, int* messagesAmount)
{
	if (*messagesAmount >= MAX_MESSAGES)
	{
		puts("Message oveflow");
		return;
	}

	int iResult = 0;

	char sendId, destId;
	int size;

	if (!getSenderInfo(ClientSocket, &sendId, &destId, &size))
		return;

	messageArr[*messagesAmount] = (pMessageStruct)calloc(1, sizeof(MessageStruct));

	iResult = recvfrom(ClientSocket, messageArr[*messagesAmount]->message, size, 0, NULL, NULL);
	if (iResult != size)
	{
		puts(ERROR_RECV);
		free(&messageArr[*messagesAmount]);
		return;
	}

	messageArr[*messagesAmount]->senderId = sendId;
	messageArr[*messagesAmount]->destId = destId;
	messageArr[*messagesAmount]->size = size;
	puts(messageArr[*messagesAmount]->message);
	(*messagesAmount)++;
}

BOOL sendMessage(SOCKET ConnectSocket, sockaddr_in* recvAddr, pMessageStruct message)
{
	int iResult;

	char sendbuf[MAX_MESSAGE_LEN] = {};

	if (!sendRequest(ConnectSocket, recvAddr, SEND_MESSAGE))
		return FALSE;

	if (!sendUserDestSize(ConnectSocket, recvAddr, message->senderId, message->destId, message->size))
		return FALSE;

	strcpy_s(sendbuf, message->size + 1, message->message);

	//Send an initial buffer
	iResult = sendto(ConnectSocket, sendbuf, message->size, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	return TRUE;
}

void trimArray(pMessageStruct* messageArr, int* messagesAmount)
{
	int amount = 0;
	for (int i = 0; i < MAX_MESSAGES; i++)
		if (messageArr[i])
			amount++;

	for (int i = 0; i < amount; i++)
		while (messageArr[i] == NULL)
		{
			for (int j = i; j < *messagesAmount - 1; j++)
				messageArr[j] = messageArr[j + 1];
			messageArr[*messagesAmount - 1] = NULL;
		}

	*messagesAmount = amount;

	//for (int i = 0; i < MAX_MESSAGES; i++)
	//{
	//	if (messageArr[i])
	//		printf("%d ", messageArr[i]->destId);
	//	else
	//		printf("0 ");
	//}
	//printf("\n");
}

void sendMessages(SOCKET ClientSocket, sockaddr_in* recvAddr, pMessageStruct* messageArr, int* messagesAmount)
{
	char userId = 0;

	int iResult;
	iResult = recv(ClientSocket, &userId, 1, 0);
	if (iResult != 1) {
		puts(ERROR_RECV);
		return;
	}

	for (int i = 0; i < *messagesAmount; i++)
	{
		if (messageArr[i]->destId == userId)
		{
			if (sendMessage(ClientSocket, recvAddr, messageArr[i]))
			{
				free(messageArr[i]);
				messageArr[i] = NULL;
			}
		}
	}

	trimArray(messageArr, messagesAmount);

	char endSend = SEND_END;
	iResult = sendto(ClientSocket, &endSend, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
}

void recieveFile(SOCKET ClientSocket, pMessageStruct* fileArr, int* filesAmount)
{
	if (*filesAmount >= MAX_MESSAGES)
	{
		puts("Files oveflow");
		return;
	}

	char sendId, destId;
	int size;

	if (!getSenderInfo(ClientSocket, &sendId, &destId, &size))
		return;

	fileArr[*filesAmount] = (pMessageStruct)calloc(1, sizeof(MessageStruct));


	int iResult;
	iResult = recvfrom(ClientSocket, fileArr[*filesAmount]->message, 100, 0, NULL, NULL);
	if (iResult < 0)
	{
		puts(ERROR_RECV);
		return;
	}

	FILE* file;
	int err = fopen_s(&file, fileArr[*filesAmount]->message, "w+b");
	if (err != 0) {
		printf("Error creating file...");
		free(&fileArr[*filesAmount]);
		return;
	}


	char buffer[UDP_FILE_BUF_SIZE];
	do {
		if (size < UDP_FILE_BUF_SIZE)
		{
			iResult = recvfrom(ClientSocket, buffer, size, 0, NULL, NULL);
		}
		else
		{
			iResult = recvfrom(ClientSocket, buffer, UDP_FILE_BUF_SIZE, 0, NULL, NULL);
		}
		if (iResult > 0) {
			fwrite(buffer, sizeof(char), iResult, file);
			size -= iResult;
		}
	} while (iResult > 0 && size);


	fclose(file);

	fileArr[*filesAmount]->senderId = sendId;
	fileArr[*filesAmount]->destId = destId;
	fileArr[*filesAmount]->size = size;
	printf("File recieved: ");
	puts(fileArr[*filesAmount]->message);
	(*filesAmount)++;

	return;
}

BOOL sendFile(SOCKET ConnectSocket, sockaddr_in* recvAddr, pMessageStruct fileStrcut)
{
	char sendbuf[100] = {};

	FILE* file;
	fopen_s(&file, fileStrcut->message, "rb");
	if (file == NULL)
		return FALSE;

	fseek(file, 0L, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	if (!sendRequest(ConnectSocket, recvAddr, SEND_FILE))
		return FALSE;

	if (!sendUserDestSize(ConnectSocket, recvAddr, fileStrcut->senderId, fileStrcut->destId, fileSize))
		return FALSE;


	int fileNameLen = strlen(fileStrcut->message);

	strcpy_s(sendbuf, fileNameLen + 1, fileStrcut->message);

	int iResult;
	iResult = sendto(ConnectSocket, sendbuf, fileNameLen, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		return FALSE;
	}

	int buffer_size = UDP_FILE_BUF_SIZE;
	char* buffer = (char*)calloc(buffer_size, sizeof(char));
	if (buffer == NULL) {
		printf("Error allocating memory...");
		return FALSE;
	}

	int bytes_read = 0;
	do {
		bytes_read = fread_s(buffer, buffer_size, sizeof(char), buffer_size, file);
		if (bytes_read > 0) {
			iResult = sendto(ConnectSocket, buffer, bytes_read, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
			if (iResult == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
				free(buffer);
				return FALSE;
			}
		}
	} 
	while (bytes_read > 0);


	free(buffer);
	fclose(file);

	return TRUE;
}

void sendFiles(SOCKET ClientSocket, sockaddr_in* recvAddr, pMessageStruct* filesArr, int* fielsAmount)
{
	char userId = 0;

	int iResult;
	iResult = recv(ClientSocket, &userId, 1, 0);
	if (iResult != 1) {
		puts(ERROR_RECV);
		return;
	}

	for (int i = 0; i < *fielsAmount; i++)
	{
		if (filesArr[i]->destId == userId)
		{
			if (sendFile(ClientSocket, recvAddr, filesArr[i]))
			{
				printf("File sent: %s\n", filesArr[i]->message);
				free(filesArr[i]);
				filesArr[i] = NULL;
			}
		}
	}

	trimArray(filesArr, fielsAmount);

	char endSend = SEND_END;
	iResult = sendto(ClientSocket, &endSend, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
}


BOOL serverDuty(SOCKET ListenSocket)
{
	int iResult;
	char recvbuf[DEFAULT_BUFLEN] = {};
	int recvbuflen = DEFAULT_BUFLEN;

	pMessageStruct messageArr[MAX_MESSAGES] = {};
	pMessageStruct filesArr[MAX_MESSAGES] = {};
	int messagesAmount = 0;
	int filesAmount = 0;

	bool isDone = FALSE;

	while (!isDone)
	{

		sockaddr_in senderAddr;

		int requestType = recieveRequest(ListenSocket, &senderAddr);

		switch (requestType)
		{
		case SEND_MESSAGE:
		{
			recieveMessage(ListenSocket, messageArr, &messagesAmount);
			break;
		}
		case RECV_MESSAGE:
		{
			sendMessages(ListenSocket, &senderAddr, messageArr, &messagesAmount);
			break;
		}
		case SEND_FILE:
		{
			recieveFile(ListenSocket, filesArr, &filesAmount);
			break;
		}
		case RECV_FILE:
		{
			sendFiles(ListenSocket, &senderAddr, filesArr, &filesAmount);
			break;
		}
		case ERROR_REQUEST:
		{
			puts("Error recieving request");
			break;
		}
		case STOP_SERVER:
		{
			isDone = TRUE;
		}
		}
	}

	closesocket(ListenSocket);

	return TRUE;

}

int __cdecl main(void)
{

	SOCKET ListenSocket = INVALID_SOCKET;
	sockaddr_in server;

	if (!initialize(&ListenSocket))
		return 1;

	if (!serverDuty(ListenSocket))
		return 1;

	closesocket(ListenSocket);

	WSACleanup();

	return 0;
}