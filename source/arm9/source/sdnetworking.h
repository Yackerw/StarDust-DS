#ifndef __SDNETWORKING
#define __SDNETWORKING
#include <nds.h>
#include "sdsocket.h"
#include "sdmath.h"

#ifndef NETVER
#define NETVER 0
#endif

#ifndef NETAUTH
#define NETAUTH "Stardust Vanilla. Change this to match your game!"
#endif

typedef struct NetworkInstance NetworkInstance;

struct NetworkInstance {
#if STARDUST_NETWORKING
	SocketAbstract* socket;
	IPEndPointAbstract* localEndPoint;
	IPEndPointAbstract** clients;
#endif
	int maxClients;
	int maxPackets;
	bool host;
	bool* clientsUsed;
	unsigned char*** importantStack;
	bool** importantStackUsed;
	int* importantStackPosition;
	int** importantStackSizes;
	unsigned char*** receiveStack;
	bool** receiveStackUsed;
	int* receiveStackPosition;
	int** receiveStackTypes;
	int** receiveStackSizes;
	f32* socketTimeout;
	f32** packetTimeout;
	int* clientIDs;
	int clientNode;
	// authentication ID
	int clientID;
	// host
	void (*gameLayerJoinSend)(int, NetworkInstance*);
	// client
	void (*gameLayerJoinReceive)(NetworkInstance*);
	void (*gameLayerJoinOthers)(int, NetworkInstance*);
	void (*gameLayerDisconnect)(int, int, NetworkInstance*);
	void (**gameLayerPacketProcessing)(unsigned char*, int, int, NetworkInstance*);
	int gameLayerPacketProcessingCount;
	int disconnectEvent;
	int joinEvent;
	int joinOtherEvent;
	bool active;
};

// enforce tight packing here
#pragma pack(push, 1)
typedef struct {
	int unode;
	int uid;
	unsigned short pid;
	unsigned short ptype;
	unsigned char important;
	unsigned char padding;
	unsigned short padding2;
} PacketHeader;
#pragma pack(pop)

extern NetworkInstance* defaultNetInstance;

void InitializeNetworking(int maxPlayers, int maxPackets);
NetworkInstance* CreateNetworkingInstance(int maxClients, int maxPackets);
int RegisterPacketType(void (*receiveCallback)(char*, int, int, NetworkInstance*), NetworkInstance* instance);
void DestroyNetworkingInstance(NetworkInstance* instance);
void StopNetworking(NetworkInstance* instance);
void StartNetworking(bool host, char* ip, int port, NetworkInstance* instance);
void UpdateNetworking(NetworkInstance* instance, f32 deltaTime);
void PacketSendAll(char* data, int len, unsigned short packetType, bool important, NetworkInstance* instance);
void PacketSendTo(char* data, int len, unsigned short packetType, bool important, int id, NetworkInstance* instance);
void CloseConn(int node, int reason, NetworkInstance* instance);

#endif