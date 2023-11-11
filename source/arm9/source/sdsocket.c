#include "sdsocket.h"
#include <malloc.h>

#ifdef STARDUST_NETWORKING

#ifdef WIN32

void InitializeSockets() {
	WSADATA tmp;
	WSAStartup(MAKEWORD(2, 2), &tmp);
}

SocketAbstract* CreateSocketAbstract() {
	SocketAbstract* sa = (SocketAbstract*)malloc(sizeof(SocketAbstract));
	sa->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// Because we're gonna be receiving a lot of data, let's make sure we have the space
	int rcvbuffsize = 1000000;
	setsockopt(sa->socket, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuffsize, sizeof(int));
	// Disable killing the socket if port is deemed unreachable
	char tmp = 0;
	char out;
	DWORD out2;
	WSAIoctl(sa->socket, -1744830452, &tmp, 1, &out, 1, &out2, NULL, NULL);
	// make non-blocking
	long tmplong = 1;
	ioctlsocket(sa->socket, FIONBIO, &tmplong);
	return sa;
}

void BindSocketAbstract(SocketAbstract* socket, IPEndPointAbstract* ipe) {
	bind(socket->socket, &ipe->connInfo, sizeof(struct sockaddr_in));
}

void SetEndPointIPV4(char* ip, IPEndPointAbstract* ipe) {
	ipe->connInfo.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &ipe->connInfo.sin_addr.S_un.S_addr);
}

void SetEndPointPort(int port, IPEndPointAbstract* ipe) {
	ipe->connInfo.sin_port = port;
}

int RecvFromAbstract(SocketAbstract* socket, char* buffer, int maxSize, IPEndPointAbstract* connInfo) {
	const int sockaddrSize = sizeof(struct sockaddr);
	return recvfrom(socket->socket, buffer, maxSize, 0, &connInfo->connInfo, &sockaddrSize);
}

void SendToAbstract(SocketAbstract* socket, char* buffer, int size, IPEndPointAbstract* to) {
	sendto(socket->socket, buffer, size, 0, &to->connInfo, sizeof(struct sockaddr));
}

void CloseSocketAbstract(SocketAbstract* socket) {
	closesocket(socket->socket);
}

#endif

#ifndef _NOTDS

void LogHandler(const char* s) {
	printf(s);
}

void InitializeSockets() {
	DSiWifi_InitDefault(true);
	//DSiWifi_SetLogHandler(LogHandler);
}

SocketAbstract* CreateSocketAbstract() {
	SocketAbstract* sa = (SocketAbstract*)malloc(sizeof(SocketAbstract));
	sa->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// make non-blocking
	int flags = fcntl(sa->socket, F_GETFL, 0) | O_NONBLOCK;
	fcntl(sa->socket, F_SETFL, flags);
	return sa;
}

void BindSocketAbstract(SocketAbstract* socket, IPEndPointAbstract* ipe) {
	bind(socket->socket, (struct sockaddr*)&ipe->connInfo, sizeof(struct sockaddr_in));
}

void SetEndPointIPV4(char* ip, IPEndPointAbstract* ipe) {
	ipe->connInfo.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &ipe->connInfo.sin_addr.s_addr);
}

void SetEndPointPort(int port, IPEndPointAbstract* ipe) {
	ipe->connInfo.sin_port = port;
}

int RecvFromAbstract(SocketAbstract* socket, char* buffer, int maxSize, IPEndPointAbstract* connInfo) {
	socklen_t sockaddrSize = sizeof(struct sockaddr_in);
	return recvfrom(socket->socket, buffer, maxSize, 0, (struct sockaddr*)&connInfo->connInfo, &sockaddrSize);
}

void SendToAbstract(SocketAbstract* socket, char* buffer, int size, IPEndPointAbstract* to) {
	sendto(socket->socket, buffer, size, 0, (struct sockaddr*)&to->connInfo, sizeof(struct sockaddr_in));
}

void CloseSocketAbstract(SocketAbstract* socket) {
	close(socket->socket);
}
#endif
#endif