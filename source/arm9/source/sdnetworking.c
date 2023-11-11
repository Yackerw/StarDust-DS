#include "sdnetworking.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdobject.h"

#ifdef STARDUST_NETWORKING

NetworkInstance* defaultNetInstance;

// allocate this on heap instead of stack since it's rather large
char* dataToReceive;

void InitializeNetworking(int maxPlayers, int maxPackets) {
	InitializeSockets();
	defaultNetInstance = CreateNetworkingInstance(maxPlayers, maxPackets);
	objCreateId = RegisterPacketType(SyncObjectCreateReceive, defaultNetInstance);
	objStopSyncId = RegisterPacketType(SyncObjectStopSyncReceive, defaultNetInstance);
	objPacketId = RegisterPacketType(SyncObjectPacketReceive, defaultNetInstance);
}

void DisconnectRecv(char* packet, int packetSize, int node, NetworkInstance* instance) {
	if (!instance->host) {
		int* iPacket = (int*)packet;
		int n = iPacket[0];
		int reas = iPacket[1];
		if (n == node) {
			CloseConn(node, reas, instance);
		}
		else {
			if (instance->gameLayerDisconnect != NULL) {
				instance->gameLayerDisconnect(n, reas, instance);
			}
		}
	}
	else {
		int* iPacket = (int*)packet;
		int reas = iPacket[1];
		CloseConn(node, reas, instance);
	}
}

void SyncJoinSend(int node, NetworkInstance* instance) {
	char* ou = (char*)malloc(instance->maxClients);
	memcpy(ou, instance->clientsUsed, instance->maxClients);
	PacketSendTo(ou, instance->maxClients, instance->joinEvent, true, node, instance);
	free(ou);
}

void SyncJoinRecv(char* data, int dataLen, int node, NetworkInstance* instance) {
	for (int i = 0; i < instance->maxClients && i < dataLen; ++i) {
		instance->clientsUsed[i] = data[i] != 0;
	}
	if (instance->gameLayerJoinReceive != NULL) {
		instance->gameLayerJoinReceive(instance);
	}
}

void SyncJoinOthers(char* data, int dataLen, int node, NetworkInstance* instance) {
	int* iData = (int*)data;
	if (instance->host) {
		return;
	}
	int nNode = iData[0];
	if (nNode != instance->clientNode) {
		instance->clientsUsed[nNode] = true;
		if (instance->gameLayerJoinOthers != NULL) {
			instance->gameLayerJoinOthers(nNode, instance);
		}
	}
}

NetworkInstance* CreateNetworkingInstance(int maxClients, int maxPackets) {
	NetworkInstance* retValue = (NetworkInstance*)calloc(sizeof(NetworkInstance), 1);
	retValue->clientIDs = (int*)malloc(sizeof(int) * maxClients);
	retValue->clients = (IPEndPointAbstract**)calloc(sizeof(IPEndPointAbstract*) * maxClients, 1);
	retValue->clientsUsed = (bool*)calloc(sizeof(bool) * maxClients, 1);
	retValue->gameLayerPacketProcessing = (void(**))malloc(sizeof(void(*)));
	retValue->importantStackPosition = (int*)calloc(sizeof(int) * maxClients, 1);
	retValue->importantStack = (unsigned char***)malloc(sizeof(unsigned char**) * maxClients);
	retValue->importantStackSizes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->importantStackUsed = (bool**)malloc(sizeof(bool*) * maxClients);
	retValue->packetTimeout = (f32**)malloc(sizeof(f32*) * maxClients);
	retValue->receiveStack = (unsigned char***)malloc(sizeof(unsigned char**) * maxClients);
	retValue->receiveStackPosition = (int*)calloc(sizeof(int) * maxClients, 1);
	retValue->receiveStackSizes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->receiveStackTypes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->receiveStackUsed = (bool**)malloc(sizeof(bool*) * maxClients);
	for (int i = 0; i < maxClients; ++i) {
		retValue->importantStackSizes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->importantStackUsed[i] = (bool*)calloc(sizeof(bool) * maxPackets, 1);
		retValue->importantStack[i] = (unsigned char**)calloc(sizeof(unsigned char*) * maxPackets, 1);
		retValue->packetTimeout[i] = (f32*)calloc(sizeof(f32) * maxPackets, 1);
		retValue->receiveStack[i] = (unsigned char**)calloc(sizeof(unsigned char*) * maxPackets, 1);
		retValue->receiveStackSizes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->receiveStackTypes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->receiveStackUsed[i] = (bool*)calloc(sizeof(bool) * maxPackets, 1);
	}
	retValue->socketTimeout = (f32*)malloc(sizeof(f32) * maxClients);

	retValue->maxClients = maxClients;
	retValue->maxPackets = maxPackets;
	retValue->active = false;
	retValue->localEndPoint = NULL;
	retValue->gameLayerPacketProcessingCount = 0;

	// DUMMY packet for 0
	RegisterPacketType(NULL, retValue);
	retValue->joinEvent = RegisterPacketType(SyncJoinRecv, retValue);
	retValue->joinOtherEvent = RegisterPacketType(SyncJoinOthers, retValue);
	retValue->disconnectEvent = RegisterPacketType(DisconnectRecv, retValue);
	return retValue;
}

// data, packet size, node, instance
int RegisterPacketType(void (*receiveCallback)(char*, int, int, NetworkInstance*), NetworkInstance* instance) {
	instance->gameLayerPacketProcessing = (void(**))realloc(instance->gameLayerPacketProcessing, sizeof(void(*)) * (instance->gameLayerPacketProcessingCount + 1));
	instance->gameLayerPacketProcessing[instance->gameLayerPacketProcessingCount] = receiveCallback;
	++instance->gameLayerPacketProcessingCount;
	return instance->gameLayerPacketProcessingCount - 1;
}

void DestroyNetworkingInstance(NetworkInstance* instance) {
	free(instance->clientIDs);
	free(instance->clientsUsed);
	free(instance->gameLayerPacketProcessing);
	free(instance->importantStackPosition);
	free(instance->receiveStackPosition);
	for (int i = 0; i < instance->maxClients; ++i) {
		if (instance->clients[i] != NULL) {
			free(instance->clients[i]);
		}
		for (int j = 0; j < instance->maxPackets; ++j) {
			if (instance->importantStack[i][j] != NULL) {
				free(instance->importantStack[i][j]);
			}
			if (instance->receiveStack[i][j] != NULL) {
				free(instance->receiveStack[i][j]);
			}
		}
		free(instance->importantStackSizes[i]);
		free(instance->importantStackUsed[i]);
		free(instance->importantStack[i]);
		free(instance->packetTimeout[i]);
		free(instance->receiveStack[i]);
		free(instance->receiveStackSizes[i]);
		free(instance->receiveStackTypes[i]);
		free(instance->receiveStackUsed[i]);
	}
	free(instance->importantStackSizes);
	free(instance->importantStackUsed);
	free(instance->importantStack);
	free(instance->packetTimeout);
	free(instance->receiveStack);
	free(instance->receiveStackSizes);
	free(instance->receiveStackTypes);
	free(instance->receiveStackUsed);
	free(instance->socketTimeout);
	free(instance->clients);
	if (instance->active) {
		CloseSocketAbstract(instance->socket);
	}
	if (instance->localEndPoint != NULL) {
		free(instance->localEndPoint);
	}
	free(instance);
}

void StopNetworking(NetworkInstance* instance) {
	if (instance->active) {
		CloseSocketAbstract(instance->socket);
	}
	if (instance->localEndPoint != NULL) {
		free(instance->localEndPoint);
		instance->localEndPoint = NULL;
	}
	instance->active = false;
	for (int i = 0; i < instance->maxClients; ++i) {
		if (instance->clients[i] != NULL) {
			free(instance->clients[i]);
			instance->clients[i] = NULL;
		}
		for (int j = 0; j < instance->maxPackets; ++j) {
			if (instance->importantStack[i][j] != NULL) {
				free(instance->importantStack[i][j]);
				instance->importantStack[i][j] = NULL;
			}
			if (instance->receiveStack[i][j] != NULL) {
				free(instance->receiveStack[i][j]);
				instance->receiveStack[i][j] = NULL;
			}
		}
	}
}

void StartNetworking(bool host, char* ip, int port, NetworkInstance* instance) {
	if (instance->active) {
		printf("Networking instance already active!\n");
		return;
	}
	// really crappy waiting loop
#ifndef _NOTDS
	bool connectedToAP = false;
	for (int i = 0; i < 200000000; ++i) {
		if (DSiWifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED) {
			connectedToAP = true;
			break;
		}
	}
	if (!connectedToAP) {
		printf("Not connected to AP...\n");
		//return;
	}
#endif
	instance->host = host;
	instance->active = true;

	// create socket and local end point (or, server end point for client)
	instance->socket = CreateSocketAbstract();
	instance->localEndPoint = calloc(sizeof(IPEndPointAbstract), 1);
	SetEndPointIPV4(ip, instance->localEndPoint);
	SetEndPointPort(port, instance->localEndPoint);
	if (host) {
		// bind the socket so we can receive connections
		BindSocketAbstract(instance->socket, instance->localEndPoint);
		instance->clientNode = -1;
		instance->clientID = rand();
		instance->clientsUsed[0] = true;
	}
	else {
		// send a connection packet
		char ou[sizeof(PacketHeader) + sizeof(int) + sizeof(NETAUTH)];
		PacketHeader* ph = (PacketHeader*)ou;
		ph->unode = -1;
		ph->ptype = 0;
		int tmp = NETVER;
		memcpy(&ou[sizeof(PacketHeader)], &tmp, sizeof(int));
		strcpy(&ou[sizeof(PacketHeader) + sizeof(int)], NETAUTH);
		SendToAbstract(instance->socket, ou, sizeof(PacketHeader) + sizeof(int) + sizeof(NETAUTH), instance->localEndPoint);
	}
}

void ProcessPacket(char* data, int packetType, int node, int packetSize, NetworkInstance* instance) {
	// bad packet!!
	if (packetType < 0 || packetType > instance->gameLayerPacketProcessingCount) {
		return;
	}
	if (instance->gameLayerPacketProcessing[packetType] != NULL) {
		instance->gameLayerPacketProcessing[packetType](data, packetSize, node, instance);
	}
}

void CloseConn(int node, int reason, NetworkInstance* instance) {
	if (!instance->clientsUsed[node]) {
		return;
	}
	instance->clientsUsed[node] = false;
	// if we're the host, inform everyone else of the disconnect
	if (instance->host || node == instance->clientNode) {
		int ou[2];
		ou[0] = node;
		ou[1] = reason;
		PacketSendAll(ou, sizeof(int) * 2, instance->disconnectEvent, true, instance);
	}
	// iterate over our stacks and free them
	for (int i = 0; i < instance->maxPackets; ++i) {
		if (instance->receiveStack[node][i] != NULL) {
			free(instance->receiveStack[node][i]);
			instance->receiveStack[node][i] = NULL;
		}
		if (instance->importantStack[node][i] != NULL) {
			free(instance->importantStack[node][i]);
			instance->importantStack[node][i] = NULL;
		}
		instance->receiveStackUsed[node][i] = false;
		instance->importantStackUsed[node][i] = false;
	}
	instance->importantStackPosition[node] = 0;
	instance->receiveStackPosition[node] = 0;
	if (instance->gameLayerDisconnect != NULL) {
		instance->gameLayerDisconnect(node, reason, instance);
	}
}

void UpdateNetworking(NetworkInstance* instance, f32 deltaTime) {
	if (instance == NULL) {
		printf("Attempted to update NULL network instance!\n");
		return;
	}
	if (dataToReceive == NULL) {
		dataToReceive = (char*)malloc(10000);
	}
	IPEndPointAbstract receiveEndPoint;
	if (!instance->active) {
		// throw data into the aether
		if (instance->socket != NULL) {
			int recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
			while (recvBytes >= 0) {
				recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
			}
		}
		return;
	}
	int recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
	while (recvBytes > 0) {
		// invalid packet...
		if (recvBytes < sizeof(PacketHeader)) {
			recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
			continue;
		}
		PacketHeader* header = (PacketHeader*)dataToReceive;
		int packSize = recvBytes - sizeof(PacketHeader);
		// if we're the host, then check for a connection packet
		if (header->unode == -1) {
			if (instance->host) {
				// TODO: banning
				// ensure they're on the right version
				int clientNetVer = -1;
				if (packSize >= 4) {
					memcpy(&clientNetVer, &dataToReceive[sizeof(PacketHeader)], sizeof(int));
				}
				char clientNetAuth[sizeof(NETAUTH)] = { 0 };
				if (packSize - 4 >= sizeof(NETAUTH)) {
					memcpy(clientNetAuth, &dataToReceive[sizeof(PacketHeader) + sizeof(int)], sizeof(NETAUTH));
					clientNetAuth[sizeof(NETAUTH) - 1] = 0;
				}
				if (clientNetVer == NETVER && strcmp(NETAUTH, clientNetAuth) == 0) {
					for (int i = 0; i < instance->maxClients; ++i) {
						if (!instance->clientsUsed[i]) {
							instance->clients[i] = malloc(sizeof(IPEndPointAbstract));
							*instance->clients[i] = receiveEndPoint;
							instance->clientIDs[i] = rand();
							// send packet marking player as connected (done explicitly before marking as used)
							PacketSendAll(&i, 4, instance->joinOtherEvent, true, instance);

							instance->clientsUsed[i] = true;

							char* ou = malloc(sizeof(PacketHeader) + sizeof(int));
							PacketHeader* header = (PacketHeader*)ou;
							// they get our ID from this, and their node from the node slot
							header->uid = instance->clientID;
							header->unode = i;
							header->important = 1;
							header->ptype = 0;
							header->pid = 0;
							// put their ID inside the message
							memcpy(&ou[sizeof(PacketHeader)], &instance->clientIDs[i], sizeof(int));
							SendToAbstract(instance->socket, ou, sizeof(PacketHeader) + sizeof(int), instance->clients[i]);
							// i'm marking it as important i'm tired of dropping the handshake because the dsi's wifi card is bad
							++instance->importantStackPosition[i];
							instance->importantStackPosition[i] %= instance->maxPackets;
							instance->importantStackUsed[i][header->pid] = true;
							instance->packetTimeout[i][header->pid] = 0;
							// store packet to important stack
							instance->importantStack[i][header->pid] = ou;
							instance->importantStackSizes[i][header->pid] = sizeof(PacketHeader) + sizeof(int);

							SyncJoinSend(i, instance);

							// sync game layer values now
							if (instance->gameLayerJoinSend != NULL) {
								instance->gameLayerJoinSend(i, instance);
							}

							instance->socketTimeout[i] = 0;

							break;
						}
					}
					// TODO: send a proper packet explaining servers full if it is
				}
			}
		}
		else {
			// not a connection packet

			// first check for a validated connection packet from host
			if (!instance->host && header->ptype == 0 && (header->important == 0 || header->important == 1)) {
				instance->clientNode = header->unode;
				instance->clientIDs[0] = header->uid;
				memcpy(&instance->clientID, &dataToReceive[sizeof(PacketHeader)], sizeof(int));
				instance->clientsUsed[0] = true;
				instance->clientsUsed[instance->clientNode] = true;
				instance->clients[0] = (IPEndPointAbstract*)malloc(sizeof(IPEndPointAbstract));
				memcpy(instance->clients[0], instance->localEndPoint, sizeof(IPEndPointAbstract));

				if (header->important == 1) {
					// tell the sender we got it
					PacketHeader confirmHeader;
					confirmHeader.unode = instance->clientNode;
					confirmHeader.uid = instance->clientID;
					confirmHeader.important = 2;
					// ensure a valid packet type so it's not thrown out...
					confirmHeader.ptype = 0;
					confirmHeader.pid = header->pid;
					SendToAbstract(instance->socket, &confirmHeader, sizeof(PacketHeader), instance->localEndPoint);
					instance->receiveStackPosition[header->unode] += 1;
				}

				// don't process packet normally, just continue
				recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
				continue;
			}

			// validate node to id
			if (header->unode >= 0 && header->unode < instance->maxClients) {
				if (header->uid != instance->clientIDs[header->unode]) {
					recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
					continue;
				}
			}
			else {
				// bad user node received, continue receiving packets
				recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
				continue;
			}
			instance->socketTimeout[header->unode] = 0;
			// if it's not an important packet, parse it, otherwise store it for later
			if (header->important == 0) {
				// parse packet
				ProcessPacket(&dataToReceive[sizeof(PacketHeader)], header->ptype, header->unode, packSize, instance);
			}
			else {
				if (header->pid > instance->maxPackets) {
					// wtf you doin' bro
					CloseConn(header->unode, 0, instance);
					recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
					continue;
				}
				// important packet being received
				if (header->important == 1) {
					// store a new instance of this packet if we need to
					if (!instance->receiveStackUsed[header->unode][header->pid]) {
						instance->receiveStack[header->unode][header->pid] = malloc(packSize);
						memcpy(instance->receiveStack[header->unode][header->pid], &dataToReceive[sizeof(PacketHeader)], packSize);
						instance->receiveStackUsed[header->unode][header->pid] = true;
						instance->receiveStackTypes[header->unode][header->pid] = header->ptype;
						instance->receiveStackSizes[header->unode][header->pid] = packSize;
					}
					// tell the sender we got it
					PacketHeader confirmHeader;
					confirmHeader.unode = instance->clientNode;
					confirmHeader.uid = instance->clientID;
					confirmHeader.important = 2;
					// ensure a valid packet type so it's not thrown out...
					confirmHeader.ptype = 0;
					confirmHeader.pid = header->pid;
					if (instance->host) {
						SendToAbstract(instance->socket, &confirmHeader, sizeof(PacketHeader), instance->clients[header->unode]);
					}
					else {
						SendToAbstract(instance->socket, &confirmHeader, sizeof(PacketHeader), instance->localEndPoint);
					}

					// read all packets that we can
					while (instance->receiveStackUsed[header->unode][instance->receiveStackPosition[header->unode]]) {
						int impPosition = instance->receiveStackPosition[header->unode];
						ProcessPacket(instance->receiveStack[header->unode][impPosition], instance->receiveStackTypes[header->unode][impPosition], header->unode, instance->receiveStackSizes[header->unode][impPosition], instance);
						// don't mark this one as unused so we can continue to tell the client it's full. instead, go back and free up old slots the size of the clients failure to send size to use again
						int oldPos = impPosition - (instance->maxPackets / 2);
						if (oldPos < 0) {
							oldPos += instance->maxPackets;
						}
						instance->receiveStackUsed[header->unode][oldPos] = false;

						// free our packet data and increment the position
						free(instance->receiveStack[header->unode][impPosition]);
						instance->receiveStack[header->unode][impPosition] = NULL;
						instance->receiveStackPosition[header->unode] += 1;
						instance->receiveStackPosition[header->unode] %= instance->maxPackets;
					}
				}
				else if (header->important == 2) {
					// acknowledgement packet of an important packet, mark it as free to use and free it
					instance->importantStackUsed[header->unode][header->pid] = false;
					free(instance->importantStack[header->unode][header->pid]);
					instance->importantStack[header->unode][header->pid] = NULL;
				}
			}
		}
		recvBytes = RecvFromAbstract(instance->socket, dataToReceive, 10000, &receiveEndPoint);
	}

	// update everyones timeout
	for (int i = 0; i < instance->maxClients; ++i) {
		if (instance->clientsUsed[i]) {
			// re-send packets deemed lost
			int resendStartPos = instance->importantStackPosition[i] - (instance->maxPackets / 2);
			while (resendStartPos != instance->importantStackPosition[i]) {
				if (resendStartPos < 0) {
					resendStartPos += instance->maxPackets;
				}
				if (instance->importantStackUsed[i][resendStartPos]) {
					// todo: change this to incorporate ping
					instance->packetTimeout[i][resendStartPos] += deltaTime;
					if (instance->packetTimeout[i][resendStartPos] > Fixed32ToNative(1228)) {
						instance->packetTimeout[i][resendStartPos] = 0;
						SendToAbstract(instance->socket, instance->importantStack[i][resendStartPos], instance->importantStackSizes[i][resendStartPos], instance->clients[i]);
					}
				}
				++resendStartPos;
				resendStartPos %= instance->maxPackets;
			}
			instance->socketTimeout[i] += deltaTime;
			if (instance->socketTimeout[i] >= Fixed32ToNative(4096 * 20)) {
				CloseConn(i, 0, instance);
			}
		}
	}

}

void PacketSendAll(char* data, int len, unsigned short packetType, bool important, NetworkInstance* instance) {
	for (int i = 0; i < instance->maxClients; ++i) {
		if (instance->clientsUsed[i]) {
			PacketSendTo(data, len, packetType, important, i, instance);
		}
	}
}

void PacketSendTo(char* data, int len, unsigned short packetType, bool important, int node, NetworkInstance* instance) {
	if (instance->socket == NULL) {
		printf("Attempted to send packet on uninitialized socket!\n");
		return;
	}
	if (!instance->clientsUsed[node]) {
		printf("Attempted to send packet to unconnected client!\n");
		return;
	}
	if (instance->clients[node] == NULL) {
		return;
	}
	// set up the header first
	char* newPacket = (char*)malloc(sizeof(PacketHeader) + len);
	memcpy(&newPacket[sizeof(PacketHeader)], data, len);
	PacketHeader* header = (PacketHeader*)newPacket;
	header->uid = instance->clientID;
	header->unode = instance->clientNode;
	// hack: if host, force node to 0
	if (instance->clientNode == -1) {
		header->unode = 0;
	}
	header->important = important ? 1 : 0;
	header->ptype = packetType;
	if (important) {
		// close connection if we're too far into the stack
		int oldPos = instance->importantStackPosition[node] - instance->maxPackets / 2;
		if (oldPos < 0) {
			oldPos += instance->maxPackets;
		}
		if (instance->importantStackUsed[node][oldPos]) {
			CloseConn(node, 0, instance);
			return;
		}
		// get our position in the stack, and update packet timeout and position
		header->pid = instance->importantStackPosition[node];
		++instance->importantStackPosition[node];
		instance->importantStackPosition[node] %= instance->maxPackets;
		instance->importantStackUsed[node][header->pid] = true;
		instance->packetTimeout[node][header->pid] = 0;
		// store packet to important stack
		instance->importantStack[node][header->pid] = newPacket;
		instance->importantStackSizes[node][header->pid] = sizeof(PacketHeader) + len;
	}

	// finally, just send the packet and cleanup if necessary
	SendToAbstract(instance->socket, newPacket, len + sizeof(PacketHeader), instance->clients[node]);
	if (!header->important) {
		free(newPacket);
	}
}
#else
NetworkInstance* defaultNetInstance;
void InitializeNetworking(int maxPlayers, int maxPackets) {

}
// this function actually returns something so I GUESS we can implement it somewhat for "no networking"
NetworkInstance* CreateNetworkingInstance(int maxClients, int maxPackets) {
	NetworkInstance* retValue = (NetworkInstance*)calloc(sizeof(NetworkInstance), 1);
	retValue->clientIDs = (int*)malloc(sizeof(int) * maxClients);
	retValue->clientsUsed = (bool*)calloc(sizeof(bool) * maxClients, 1);
	retValue->gameLayerPacketProcessing = (void(**))malloc(sizeof(void(*)));
	retValue->importantStackPosition = (int*)calloc(sizeof(int) * maxClients, 1);
	retValue->importantStack = (unsigned char***)malloc(sizeof(unsigned char**) * maxClients);
	retValue->importantStackSizes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->importantStackUsed = (bool**)malloc(sizeof(bool*) * maxClients);
	retValue->packetTimeout = (f32**)malloc(sizeof(f32*) * maxClients);
	retValue->receiveStack = (unsigned char***)malloc(sizeof(unsigned char**) * maxClients);
	retValue->receiveStackPosition = (int*)calloc(sizeof(int) * maxClients, 1);
	retValue->receiveStackSizes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->receiveStackTypes = (int**)malloc(sizeof(int*) * maxClients);
	retValue->receiveStackUsed = (bool**)malloc(sizeof(bool*) * maxClients);
	for (int i = 0; i < maxClients; ++i) {
		retValue->importantStackSizes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->importantStackUsed[i] = (bool*)calloc(sizeof(bool) * maxPackets, 1);
		retValue->importantStack[i] = (unsigned char**)calloc(sizeof(unsigned char*) * maxPackets, 1);
		retValue->packetTimeout[i] = (f32*)calloc(sizeof(f32) * maxPackets, 1);
		retValue->receiveStack[i] = (unsigned char**)calloc(sizeof(unsigned char*) * maxPackets, 1);
		retValue->receiveStackSizes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->receiveStackTypes[i] = (int*)malloc(sizeof(int) * maxPackets);
		retValue->receiveStackUsed[i] = (bool*)calloc(sizeof(bool) * maxPackets, 1);
	}
	retValue->socketTimeout = (f32*)malloc(sizeof(f32) * maxClients);

	retValue->maxClients = maxClients;
	retValue->maxPackets = maxPackets;
	retValue->active = false;
	retValue->gameLayerPacketProcessingCount = 0;

	return retValue;
}
int RegisterPacketType(void (*receiveCallback)(char*, int, int, NetworkInstance*), NetworkInstance* instance) {
	return -1;
}
void DestroyNetworkingInstance(NetworkInstance* instance) {
	free(instance);
}
void StopNetworking(NetworkInstance* instance) {

}
void StartNetworking(bool host, char* ip, int port, NetworkInstance* instance) {

}
void UpdateNetworking(NetworkInstance* instance, f32 deltaTime) {

}
void PacketSendAll(char* data, int len, unsigned short packetType, bool important, NetworkInstance* instance) {

}
void PacketSendTo(char* data, int len, unsigned short packetType, bool important, int id, NetworkInstance* instance) {

}
void CloseConn(int node, int reason, NetworkInstance* instance) {

}
#endif