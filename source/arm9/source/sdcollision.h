#ifndef SDCOLLISION
#define SDCOLLISION
#include <nds.h>
#include "sdmath.h"
#include "sdrender.h"


typedef struct {
	Vec3 verts[3];
	Vec3 normal;
	Vec3 boundsMin;
	Vec3 boundsMax;
} CollisionTriangle;

typedef struct CollisionBlock CollisionBlock;

struct CollisionBlock {
	bool subdivided;
	int triCount;
	Vec3 boundsMin;
	Vec3 boundsMax;
	union {
		unsigned short *triangleList;
		CollisionBlock *blocks;
	};
};

typedef struct {
	int triCount;
	CollisionTriangle* triangles;
	Vec3 AABBPosition;
	Vec3 AABBBounds;
	// blockmap sorta
	CollisionBlock blocks[8];
} MeshCollider;

typedef struct {
	Vec3* position;
	f32 radius;
} CollisionSphere;

typedef struct {
	Vec3* position;
	Vec3 extents;
	Quaternion* rotation;
} CollisionBox;

bool SphereOnTriangleLine(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration);

bool SphereOnTriangleVertex(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration);

bool SphereOnTrianglePlane(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration, bool *onPlane);

MeshCollider *LoadCollisionMesh(char *input);

bool SphereOnSphere(CollisionSphere *sphere1, CollisionSphere *sphere2, f32 *penetration, Vec3 *normal);

MeshCollider *MeshColliderFromMesh(Model *input);

unsigned short* FindTrianglesFromOctree(Vec3* min, Vec3* max, MeshCollider* meshCollider, int *totalTris);

void DestroyCollisionMesh(MeshCollider* meshCollider);

bool RayOnAABB(Vec3* point, Vec3* direction, Vec3* boxMin, Vec3* boxMax, Vec3* normal, f32* t);

bool RayOnSphere(Vec3* point, Vec3* direction, CollisionSphere* sphere, f32* t, Vec3* hitPos);

bool RayOnTriangle(Vec3* point, Vec3* direction, CollisionTriangle* triangle, f32* t, Vec3* hitPos);

bool RayOnMesh(Vec3* point, Vec3* direction, f32 length, Vec3* rayMin, Vec3* rayMax, MeshCollider* mesh, Vec3* meshOffset, Vec3* meshScale, Quaternion* meshRotation, f32* t, Vec3* hitPos, Vec3* normal, int* triId);

bool SphereOnOBB(CollisionSphere* sphere, CollisionBox* box, Vec3* hitPos, Vec3* normal, f32* t);

#endif