#ifndef SDOBJECT
#define SDOBJECT
#include <nds.h>
#include "sdmath.h"
#include "sdrender.h"
#include "sdcollision.h"
#include "sdnetworking.h"

#define COLLIDER_SPHERE 1
#define COLLIDER_MESH 2
#define COLLIDER_BOX 3

typedef struct ObjectPtr ObjectPtr;

typedef struct Object Object;

struct ObjectPtr {
	Object* object;
	ObjectPtr* prev;
	ObjectPtr* next;
};

struct Object {
	Object *previous;
	Object *next;
	Vec3 position;
	Quaternion rotation;
	Vec3 scale;
	Vec3 velocity;
	void *data;
	int objectType;
	Model *mesh;
	CollisionSphere *sphereCol;
	MeshCollider *meshCol;
	CollisionBox* boxCol;
	unsigned int flags;
	bool solid;
	bool moves;
	bool culled;
	bool trigger;
	bool destroy;
	bool active;
	Animator *animator;
	unsigned int layer;
	int netId;
	ObjectPtr references;
};

typedef struct {
	Vec3 hitPos;
	Vec3 normal;
	Object* hitObject;
	int hitTri;
	int colliderType;
	f32 penetration;
} CollisionHit;

extern int objPacketId;
extern int objCreateId;
extern int objStopSyncId;

extern Object firstObject;

void ProcessObjects();

int AddObjectType(void(*update)(Object*), void(*start)(Object*), bool(*collision)(Object*, CollisionHit*), void(*lateUpdate)(Object*), void(*destroy)(Object*), void(*networkCreateSend)(Object*, void**, int*),
	void (*networkCreateReceive)(Object*, void*, int), void(*networkPacketReceive)(Object*, void**, int, int, int), bool networked);

Object *CreateObject(int type, Vec3 *position, bool forced);

bool RaycastWorld(Vec3* point, Vec3* direction, f32 length, unsigned int layerMask, f32* t, CollisionHit* hitInfo);

int SphereCollisionCheck(CollisionSphere *sphere, unsigned int layerMask, CollisionHit* hitInfos, int maxHit);

void AddCollisionBetweenLayers(int layer1, int layer2);

void DestroyObject(Object *object);

void DestroyObjectImmediate(Object* object);

int GetObjectsOfType(int type, Object **out, int maxObjects);

void GetObjectPtr(Object* object, ObjectPtr* out);

void FreeObjectPtr(ObjectPtr* ptr);

void SyncObjectPacketSend(Object* object, void* data, int dataLen, unsigned short type, bool important);

void SyncObjectPacketReceive(void* data, int dataLen, int node, NetworkInstance* instance);

void SyncObjectCreateReceive(void* data, int dataLen, int node, NetworkInstance* instance);

void SyncObjectStopSyncReceive(void* data, int dataLen, int node, NetworkInstance* instance);

void StopSyncingObject(Object* object);

void SyncAllObjects(int node);

#endif