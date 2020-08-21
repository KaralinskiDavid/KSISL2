#define WIN32_LEAN_AND_MEAN

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "../../Server-UDP/Server-UDP/commands.h"

#define SERVER_ADDRES "127.0.0.1"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27015

#define USER_SEND_MESSAGE 1
#define USER_RECV_MESSAGE 2
#define USER_SEND_FILE 3
#define USER_RECV_FILE 4
#define USER_MEASURE_CONNECTION 5
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

BOOL initialize(SOCKET* ClientSocket)
{
	if (!initializeWinsock())
		return FALSE;


	//ZeroMemory(&hints, sizeof(hints));
	//hints.ai_family = AF_INET;
	//hints.ai_socktype = SOCK_DGRAM;
	//hints.ai_protocol = IPPROTO_UDP;

	*ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*ClientSocket == INVALID_SOCKET) {
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}

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

BOOL sendMessage(SOCKET ConnectSocket, sockaddr_in* recvAddr, char* message, char userId, char destId)
{
	int iResult;

	char sendbuf[MAX_MESSAGE_LEN] = {};


	if (!sendRequest(ConnectSocket, recvAddr, SEND_MESSAGE))
		return FALSE;

	int msgLen = strlen(message);
	if (!sendUserDestSize(ConnectSocket, recvAddr, userId, destId, msgLen))
		return FALSE;

	strcpy_s(sendbuf, msgLen + 1, message);

	//Send an initial buffer
	iResult = sendto(ConnectSocket, sendbuf, msgLen, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	return TRUE;
}

void userSendMessage(SOCKET ConnectSocket, sockaddr_in* recvAddr, char userId)
{
	char message[MAX_MESSAGE_LEN] = {};
	int destId = 0;


	printf("Input message:\n");
	getchar();
	gets_s(message, MAX_MESSAGE_LEN - 1);

	printf("Input destination id:\n");
	scanf_s("%d", &destId);

	if (sendMessage(ConnectSocket, recvAddr, message, userId, destId))
		puts("Message sent.");
}

int recieveRequest(SOCKET ClientSocket)
{
	int iResult;
	int fromLen = sizeof(sockaddr_in);
	char requestType;

	iResult = recvfrom(ClientSocket, &requestType, 1, 0, NULL, NULL);
	if (iResult > 0) {
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
	*size = *((int*)& recvbuf[2]);

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

	iResult = recvfrom(ClientSocket, recvbuf, size, 0, NULL, NULL);
	if (iResult != size)
	{
		puts(ERROR_RECV);
		return;
	}

	printf("[%d] %s\n", sendId, recvbuf);
}

void userRecieveMessage(SOCKET ConnectSocket, sockaddr_in* recvAddr, int userId)
{
	char sendbuf = RECV_MESSAGE;

	int iResult;

	iResult = sendto(ConnectSocket, &sendbuf, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return;
	}

	sendbuf = userId;
	iResult = sendto(ConnectSocket, &sendbuf, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
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

void sendFile(SOCKET ConnectSocket, sockaddr_in* recvAddr, char* filename, int userId, int destId)
{
	char sendbuf[100] = {};

	FILE* file;
	fopen_s(&file, filename, "rb");
	if (file == NULL)
		return;

	fseek(file, 0L, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	if (!sendRequest(ConnectSocket, recvAddr, SEND_FILE))
		return;

	if (!sendUserDestSize(ConnectSocket, recvAddr, userId, destId, fileSize))
		return;


	int fileNameLen = strlen(filename);

	strcpy_s(sendbuf, fileNameLen + 1, filename);

	int iResult;
	iResult = sendto(ConnectSocket, sendbuf, fileNameLen, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		return;
	}

	int buffer_size = UDP_FILE_BUF_SIZE;
	char* buffer = (char*)calloc(buffer_size, sizeof(char));
	if (buffer == NULL) {
		printf("Error allocating memory...");
		return;
	}

	int bytes_read = 0;
	do {
		bytes_read = fread_s(buffer, buffer_size, sizeof(char), buffer_size, file);
		if (bytes_read > 0) {
			iResult = sendto(ConnectSocket, buffer, bytes_read, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
			if (iResult == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
				free(buffer);
				return;
			}
		}
	} 
	while (bytes_read > 0);


	free(buffer);
	fclose(file);
}

void userSendFile(SOCKET ConnectSocket, sockaddr_in* recvAddr, int userId)
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

	sendFile(ConnectSocket, recvAddr, filename, userId, destId);
}

void recieveFile(SOCKET ClientSocket)
{
	char sendId, destId;
	int size;

	if (!getSenderInfo(ClientSocket, &sendId, &destId, &size))
		return;

	char recvbuf[100] = {};


	int iResult;
	iResult = recvfrom(ClientSocket, recvbuf, 100, 0, NULL, NULL);
	if (iResult < 0)
	{
		puts(ERROR_RECV);
		return;
	}

	FILE* file;
	int err = fopen_s(&file, recvbuf, "w+b");
	if (err != 0) {
		printf("Error creating file...");
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
	} 
	while (iResult > 0 && size);


	fclose(file);


	printf("File recieved: %s. Sender: %d.\n", recvbuf, sendId);

	return;
}

void userRecieveFile(SOCKET ConnectSocket, sockaddr_in* recvAddr, int userId)
{
	char sendbuf;
	sendbuf = RECV_FILE;
	int iResult;

	iResult = sendto(ConnectSocket, &sendbuf, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		return;
	}

	sendbuf = userId;
	iResult = sendto(ConnectSocket, &sendbuf, 1, 0, (SOCKADDR*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
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

BOOL userStopServer(SOCKET ConnectSocket, sockaddr_in* recvAddr)
{
	char stopServer = STOP_SERVER;

	int iResult;
	iResult = sendto(ConnectSocket, &stopServer, 1, 0, (sockaddr*)recvAddr, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}

BOOL clientDuty(SOCKET ClientSocket)
{
	int userChoice;

	char userId = getUserId();

	sockaddr_in RecvAddr;

	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(DEFAULT_PORT);
	RecvAddr.sin_addr.s_addr = inet_addr(SERVER_ADDRES);

	do
	{
		printUserGuide();
		scanf_s("%d", &userChoice);

		switch (userChoice)
		{
		case USER_SEND_MESSAGE:
		{
			userSendMessage(ClientSocket, &RecvAddr, userId);
			break;
		}
		case USER_RECV_MESSAGE:
		{
			userRecieveMessage(ClientSocket, &RecvAddr, userId);
			break;
		}
		case USER_SEND_FILE:
		{
			userSendFile(ClientSocket, &RecvAddr, userId);
			break;
		}
		case USER_RECV_FILE:
		{
			userRecieveFile(ClientSocket, &RecvAddr, userId);
			break;
		}
		case USER_STOP_SERVER:
		{
			userStopServer(ClientSocket, &RecvAddr);
			userChoice = USER_STOP_CLIENT;
			break;
		}
		}

		//closesocket(ClientSocket);

	} while (userChoice != USER_STOP_CLIENT);

	closesocket(ClientSocket);

	return TRUE;

}

int __cdecl main(int argc, char** argv)
{
	SOCKET ClientSocket = INVALID_SOCKET;
	if (!initialize(&ClientSocket))
		return 1;

	if (!clientDuty(ClientSocket))
		return 1; \

		WSACleanup();

	return 0;
}
