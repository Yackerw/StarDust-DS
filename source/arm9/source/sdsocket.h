#ifndef __SDSOCKET
#define __SDSOCKET
//#define STARDUST_NETWORKING
#ifdef STARDUST_NETWORKING
#include <nds.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>

typedef struct {
	SOCKET socket;
} SocketAbstract;

typedef struct {
	struct sockaddr_in connInfo;
} IPEndPointAbstract;
#endif
#ifndef _NOTDS
#include <dsiwifi9.h>
#include <lwip/sockets.h>
typedef struct {
	int socket;
} SocketAbstract;

typedef struct {
	struct sockaddr_in connInfo;
} IPEndPointAbstract;
#endif

void InitializeSockets();

SocketAbstract* CreateSocketAbstract();

void BindSocketAbstract(SocketAbstract* socket, IPEndPointAbstract* ipe);

void SetEndPointIPV4(char* ip, IPEndPointAbstract* ipe);

void SetEndPointPort(int port, IPEndPointAbstract* ipe);

int RecvFromAbstract(SocketAbstract* socket, char* buffer, int maxSize, IPEndPointAbstract* connInfo);

void SendToAbstract(SocketAbstract* socket, char* buffer, int size, IPEndPointAbstract* to);

void CloseSocketAbstract(SocketAbstract* socket);

#endif
#endif