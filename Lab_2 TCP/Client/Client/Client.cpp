#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "../../Server/Server/commands.h"

#define SERVER_ADDRES "localhost"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define USER_SEND_MESSAGE 1
#define USER_RECV_MESSAGE 2
#define USER_SEND_FILE 3
#define USER_RECV_FILE 4
#define USER_STOP_SERVER 9
#define USER_STOP_CLIENT 10


void printUserGuide()
{
	printf("Input %d to send message\n", USER_SEND_MESSAGE);
	printf("Input %d to recieve messages\n", USER_RECV_MESSAGE);
	printf("Input %d to send file\n", USER_SEND_FILE);
	printf("Input %d to recieve files\n", USER_RECV_FILE);
	printf("Input %d to stop server\n", USER_STOP_SERVER);
	printf("Input %d to exit\n", USER_STOP_CLIENT);
}

int getUserId()
{
	printf("Input user id: ");
	int userId;
	scanf_s("%d", &userId);
	return userId;
}

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

BOOL getAddrInfo(addrinfo* hints, addrinfo** result)
{
	int iResult;

	// Resolve the server address and port
	iResult = getaddrinfo(SERVER_ADDRES, DEFAULT_PORT, hints, result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return FALSE;
	}
	return TRUE;
}

BOOL initialize(addrinfo** result)
{
	if (!initializeWinsock())
		return FALSE;

	*result = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (!getAddrInfo(&hints, result))
		return FALSE;

	return TRUE;
}

BOOL connectToServer(SOCKET* ConnectSocket, addrinfo* serverInfo)
{
	struct addrinfo* ptr = NULL;
	int iResult;

	// Attempt to connect to an address until one succeeds
	for (ptr = serverInfo; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		*ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (*ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return FALSE;
		}

		// Connect to server.
		iResult = connect(*ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(*ConnectSocket);
			*ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	if (*ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}

BOOL sendMessage(SOCKET ConnectSocket, char* message, char userId, char destId)
{
	char sendbuf[MAX_MESSAGE_LEN + 8] = {};

	sendbuf[0] = (char)SEND_MESSAGE;
	sendbuf[1] = userId;
	sendbuf[2] = destId;
	
	int msgLen = strlen(message);
	*(&sendbuf[3]) = msgLen;
	strcpy_s(&sendbuf[7], msgLen + 1, message);

	int iResult;

	 //Send an initial buffer
	iResult = send(ConnectSocket, sendbuf, 7 + msgLen, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}

void userSendMessage(SOCKET ConnectSocket, char userId)
{
	char message[MAX_MESSAGE_LEN] = {};
	int destId = 0;


	printf("Input message:\n");
	getchar();
	gets_s(message, MAX_MESSAGE_LEN - 1);

	printf("Input destination id:\n");
	scanf_s("%d", &destId);

	if (sendMessage(ConnectSocket, message, userId, destId))
		puts("Message sent.");
}

int recieveRequest(SOCKET ClientSocket)
{
	int iResult;
	char requestType;

	iResult = recv(ClientSocket, &requestType, 1, 0);
	if (iResult > 0) {
		return requestType;
	}
	else {
		printf("recv failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return ERROR_REQUEST;
	}
}

BOOL getSenderInfo(SOCKET ClientSocket, char* sendId, char* destId, int* size)
{
	int iResult;

	*sendId = 0;
	iResult = recv(ClientSocket, sendId, 1, 0);
	if (iResult != 1)
	{
		puts(ERROR_RECV);
		return FALSE;
	}

	*destId = 0;
	iResult = recv(ClientSocket, destId, 1, 0);
	if (iResult != 1)
	{
		puts(ERROR_RECV);
		return FALSE;
	}

	*size = 0;
	iResult = recv(ClientSocket, (char*)size, 4, 0);
	if (iResult != 4)
	{
		puts(ERROR_RECV);
		return FALSE;
	}

	return TRUE;
}

void recieveMessage(SOCKET ClientSocket)
{
	int iResult = 0;

	char sendId, destId;
	int size;

	if (!getSenderInfo(ClientSocket, &sendId, &destId, &size))
		return;

	char recvbuf[MAX_MESSAGE_LEN] = {};

	iResult = recv(ClientSocket, recvbuf, size, 0);
	if (iResult != size)
	{
		puts(ERROR_RECV);
		return;
	}

	printf("[%d] %s\n", sendId, recvbuf);
}

void userRecieveMessage(SOCKET ConnectSocket, int userId)
{
	char sendbuf[2];
	sendbuf[0] = RECV_MESSAGE;
	sendbuf[1] = userId;


	int iResult = 0;
	iResult = send(ConnectSocket, sendbuf, 2, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}

	int sendRequest;
	sendRequest = recieveRequest(ConnectSocket);

	while (sendRequest != SEND_END && sendRequest != ERROR_REQUEST)
	{
		recieveMessage(ConnectSocket);
		sendRequest = recieveRequest(ConnectSocket);
	}
	puts("End recieving messages.");
}

void sendFile(SOCKET ConnectSocket, char* filename, int userId, int destId)
{
	char sendbuf[100 + 8] = {};

	sendbuf[0] = (char)SEND_FILE;
	sendbuf[1] = userId;
	sendbuf[2] = destId;

	FILE* file;
	fopen_s(&file, filename, "rb");
	if (file == NULL)
		return;

	fseek(file, 0L, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	*((int *)(&sendbuf[3])) = fileSize;

	int fileNameLen = strlen(filename);
	sendbuf[7] = (char)fileNameLen;
	strcpy_s(&sendbuf[8], fileNameLen + 1, filename);

	int iResult = send(ConnectSocket, sendbuf, 8 + fileNameLen, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}


	int buffer_size = 65536;
	char* buffer = (char*)calloc(buffer_size, sizeof(char));
	if (buffer == NULL) {
		printf("Error allocating memory...");
		return;
	}

	int bytes_read = 0;
	do {
		bytes_read = fread_s(buffer, buffer_size, sizeof(char), buffer_size, file);
		if (bytes_read > 0) {
			iResult = send(ConnectSocket, buffer, bytes_read, 0);
			if (iResult == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
				free(buffer);
				closesocket(ConnectSocket);
				WSACleanup();
				return;
			}
		}
	} 
	while (bytes_read > 0);


	free(buffer);
	fclose(file);
}

void userSendFile(SOCKET ConnectSocket, int userId)
{
	char filename[100] = {};
	puts("Input file name:");
	getchar();
	gets_s(filename, 100);

	FILE* file;
	fopen_s(&file, filename, "r");
	if (file == NULL)
	{
		puts("File not found.");
		return;
	}
	fclose(file);

	int destId;
	printf("Input destination id:\n");
	scanf_s("%d", &destId);

	sendFile(ConnectSocket, filename, userId, destId);
}

void recieveFile(SOCKET ClientSocket)
{

	char sendId, destId;
	int size;

	if (!getSenderInfo(ClientSocket, &sendId, &destId, &size))
		return;

	char namebuf[100] = {};

	char fileNameLen;

	int iResult = 0;
	iResult = recv(ClientSocket, &fileNameLen, 1, 0);
	if (iResult != 1)
	{
		puts(ERROR_RECV);
		return;
	}

	iResult = recv(ClientSocket, namebuf, fileNameLen, 0);
	if (iResult != fileNameLen)
	{
		puts(ERROR_RECV);
		return;
	}

	FILE* file;
	int err = fopen_s(&file, namebuf, "w+b");
	if (err != 0) {
		printf("Error creating file...");
		return;
	}


	char buffer[DEFAULT_BUFLEN];
	do {
		if (size < DEFAULT_BUFLEN)
		{
			iResult = recv(ClientSocket, buffer, size, 0);
		}
		else
		{
			iResult = recv(ClientSocket, buffer, DEFAULT_BUFLEN, 0);
		}
		if (iResult > 0) {
			fwrite(buffer, sizeof(char), iResult, file);
			size -= iResult;
		}
	} while (iResult > 0 && size);


	fclose(file);

	printf("File recieved: %s. Sender: %d.\n", namebuf, sendId);

	return;
}

void userRecieveFile(SOCKET ConnectSocket, int userId)
{
	char sendbuf[2];
	sendbuf[0] = RECV_FILE;
	sendbuf[1] = userId;


	int iResult = 0;
	iResult = send(ConnectSocket, sendbuf, 2, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}

	int sendRequest;
	sendRequest = recieveRequest(ConnectSocket);

	while (sendRequest != SEND_END && sendRequest != ERROR_REQUEST)
	{
		recieveFile(ConnectSocket);
		sendRequest = recieveRequest(ConnectSocket);
	}
	puts("End recieving files.");
}



BOOL userStopServer(SOCKET ConnectSocket)
{
	char stopServer = STOP_SERVER;

	int iResult;
	iResult = send(ConnectSocket, &stopServer, 1, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}

BOOL clientDuty(addrinfo* serverInfo)
{
	int userChoice;

	char userId = getUserId();

	do
	{
		printUserGuide();
		scanf_s("%d", &userChoice);

		SOCKET ConnectSocket = INVALID_SOCKET;
		if (!connectToServer(&ConnectSocket, serverInfo))
			return FALSE;

		switch (userChoice)
		{
		case USER_SEND_MESSAGE:
		{
			userSendMessage(ConnectSocket, userId);
			break;
		}
		case USER_RECV_MESSAGE:
		{
			userRecieveMessage(ConnectSocket, userId);
			break;
		}
		case USER_SEND_FILE:
		{
			userSendFile(ConnectSocket, userId);
			break;
		}
		case USER_RECV_FILE:
		{
			userRecieveFile(ConnectSocket, userId);
			break;
		}
		case USER_STOP_SERVER:
		{
			userStopServer(ConnectSocket);
			userChoice = USER_STOP_CLIENT;
			break;
		}
		}


		int iResult = 0;
		iResult = shutdown(ConnectSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return FALSE;
		}

		closesocket(ConnectSocket);

	} while (userChoice != USER_STOP_CLIENT);

	return TRUE;
	
}

int __cdecl main(int argc, char** argv)
{
	addrinfo* serverInfo;
	if (!initialize(&serverInfo))
		return 1;

	if (!clientDuty(serverInfo))
		return 1;

	WSACleanup();

	return 0;
}