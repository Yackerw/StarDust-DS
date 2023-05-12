#ifndef SDOBJECT
#define SDOBJECT
#include <nds.h>
#include "sdmath.h"
#include "sdrender.h"
#include "sdcollision.h"

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
	unsigned int flags;
	bool solid;
	bool moves;
	bool culled;
	bool trigger;
	bool destroy;
	bool dirtyTransform;
	bool active;
	m4x4 transform;
	Animator *animator;
	unsigned int layer;
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

extern Object firstObject;

void ProcessObjects();

int AddObjectType(void(*update)(Object*), void(*start)(Object*), bool(*collision)(Object*, CollisionHit*), void(*lateUpdate)(Object*), void(*destroy)(Object*));

Object *CreateObject(int type, Vec3 *position);

void RaycastWorld(Vec3* point, Vec3* direction, f32 length, unsigned int layerMask, f32* t, CollisionHit* hitInfo);

int SphereCollisionCheck(CollisionSphere *sphere, unsigned int layerMask, CollisionHit* hitInfos, int maxHit);

void AddCollisionBetweenLayers(int layer1, int layer2);

void DestroyObject(Object *object);

int GetObjectsOfType(int type, Object **out, int maxObjects);

void GetObjectPtr(Object* object, ObjectPtr* out);

void FreeObjectPtr(ObjectPtr* ptr);

#endif