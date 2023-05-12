#include <stdio.h>
#include "sdcollision.h"
#include <nds.h>
#include "sdmath.h"
#define OCTREE_MAX_DEPTH 10
#define OCTREE_MAX_TRIS 40
// note: barely noticeable slowdown from this, and fixes a bug with large polygons
#define BARY64

float dotf(float x1, float y1, float z1, float x2, float y2, float z2) {
	return x1 * x2 + y1 * y2 + z1 * z2;
}

#ifndef _NOTDS
long long dot64(Vec3* left, Vec3* right) {
	long long work = ((long long)left->x * (long long)right->x) >> 12;
	long long work2 = ((long long)left->y * (long long)right->y) >> 12;
	long long work3 = ((long long)left->z * (long long)right->z) >> 12;
	return work + work2 + work3;
}

long long mulf64(long long left, long long right) {
	return (left * right) >> 12;
}

long long divf64(long long left, long long right) {
	REG_DIVCNT = DIV_64_64;

	while (REG_DIVCNT & DIV_BUSY);

	REG_DIV_NUMER = left << 12;
	REG_DIV_DENOM = right;

	while (REG_DIVCNT & DIV_BUSY);

	return (REG_DIV_RESULT);
}
#endif

Vec3 BarycentricCoords(CollisionTriangle *tri, Vec3 *point) {
	Vec3 v0, v1, v2;
	Vec3Subtraction(&tri->verts[1], &tri->verts[0], &v0);
	Vec3Subtraction(&tri->verts[2], &tri->verts[0], &v1);
	Vec3Subtraction(point, &tri->verts[0], &v2);
	#ifdef FLOATBARY
	float v0x = f32tofloat(v0.x);
	float v0y = f32tofloat(v0.y);
	float v0z = f32tofloat(v0.z);
	float v1x = f32tofloat(v1.x);
	float v1y = f32tofloat(v1.y);
	float v1z = f32tofloat(v1.z);
	float v2x = f32tofloat(v2.x);
	float v2y = f32tofloat(v2.y);
	float v2z = f32tofloat(v2.z);
	float d00 = dotf(v0x, v0y, v0z, v0x, v0y, v0z);
	float d01 = dotf(v0x, v0y, v0z, v1x, v1y, v1z);
	float d11 = dotf(v1x, v1y, v1z, v1x, v1y, v1z);
	float d20 = dotf(v2x, v2y, v2z, v0x, v0y, v0z);
	float d21 = dotf(v2x, v2y, v2z, v1x, v1y, v1z);
	float denom = d00 * d11 - d01 * d01;
	Vec3 retValue;
	retValue.x = floattof32((d11 * d20 - d01 * d21) / denom);
	retValue.y = floattof32((d00 * d21 - d01 * d20) / denom);
	retValue.z = Fixed32ToNative(4096) - retValue.x - retValue.y;
	return retValue;
	#else
#if defined(BARY64) && !defined(_NOTDS)
	long long d00 = dot64(&v0, &v0);
	long long d01 = dot64(&v0, &v1);
	long long d11 = dot64(&v1, &v1);
	long long d20 = dot64(&v2, &v0);
	long long d21 = dot64(&v2, &v1);
	long long denom = mulf64(d00, d11) - mulf64(d01, d01);
	Vec3 retValue;
	retValue.x = divf64(mulf64(d11, d20) - mulf64(d01, d21), denom);
	retValue.y = divf64(mulf64(d00, d21) - mulf64(d01, d20), denom);
	retValue.z = Fixed32ToNative(4096) - retValue.x - retValue.y;
	return retValue;
#else
	f32 d00 = DotProduct(&v0, &v0);
	f32 d01 = DotProduct(&v0, &v1);
	f32 d11 = DotProduct(&v1, &v1);
	f32 d20 = DotProduct(&v2, &v0);
	f32 d21 = DotProduct(&v2, &v1);
	f32 denom = mulf32(d00, d11) - mulf32(d01, d01);
	Vec3 retValue;
	retValue.x = divf32(mulf32(d11, d20) - mulf32(d01, d21), denom);
	retValue.y = divf32(mulf32(d00, d21) - mulf32(d01, d20), denom);
	retValue.z = Fixed32ToNative(4096) - retValue.x - retValue.y;
	
	return retValue;
#endif
	#endif
}

bool SphereOnPoint(CollisionSphere *sphere, Vec3 *point, f32 *penetration) {
	Vec3 distance;
	Vec3Subtraction(sphere->position, point, &distance);
	f32 mag = SqrMagnitude(&distance);
	if (mag < mulf32(sphere->radius, sphere->radius)) {
		*penetration = sphere->radius - sqrtf32(mag);
		return true;
	}
	return false;
}

bool SphereOnLine(CollisionSphere *sphere, Vec3 *p1, Vec3 *p2, Vec3 *closestPoint) {
	Vec3 working;
	Vec3 working2;
	Vec3 working3;
	f32 t;
	f32 t2;
	Vec3Subtraction(p2, p1, &working3);
	Vec3Subtraction(p1, sphere->position, &working2);
	t = DotProduct(&working3, &working2);
	t2 = DotProduct(&working3, &working3);
	t = divf32(t, t2);
	
	working.x = mulf32(t, working3.x);
	working.y = mulf32(t, working3.y);
	working.z = mulf32(t, working3.z);
	closestPoint->x = p1->x - working.x;
	closestPoint->y = p1->y - working.y;
	closestPoint->z = p1->z - working.z;
	
	// if distance to closest point is greater than the spheres radius, no collision
	Vec3Subtraction(sphere->position, closestPoint, &working);
	if (SqrMagnitude(&working) >= mulf32(sphere->radius, sphere->radius)) {
		return false;
	}
	// okay, get magnitude between the points. if
	// the distance between the two points of the line and the closest point is equal to the distance between
	// the two points of the line, then it's on the line and we return true
	f32 magLine = Magnitude(&working3);
	Vec3Subtraction(p1, closestPoint, &working);
	f32 magPoint1 = Magnitude(&working);
	Vec3Subtraction(p2, closestPoint, &working);
	f32 magPoint2 = Magnitude(&working);
	magPoint1 += magPoint2;
	// add a little leniency for, uh...lack of precision
	if (magPoint1 <= magLine + Fixed32ToNative(32) && magPoint1 >= magLine - Fixed32ToNative(32)) {
		return true;
	}
	
	return false;
}

bool SphereOnTriangleLine(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration) {
	// and finally, line collision
	for (int i = 0; i < 3; ++i) {
		Vec3 closestPoint;
		if (SphereOnLine(sphere, &tri->verts[i], &tri->verts[(i + 1) % 3], &closestPoint)) {
			// get penetration
			Vec3 lineDiff;
			Vec3Subtraction(sphere->position, &closestPoint, &lineDiff);
			*penetration = sphere->radius - Magnitude(&lineDiff);
			// and finally the normal
			Normalize(&lineDiff, normal);
			
			return true;
		}
	}
	return false;
}

bool SphereOnTriangleVertex(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration) {
	for (int i = 0; i < 3; ++i) {
		f32 pointMag;
		if (SphereOnPoint(sphere, &tri->verts[i], &pointMag)) {
			// get penetration and normal
			*penetration = pointMag;
			// normal; subtract the two vectors, then normalize
			Vec3Subtraction(sphere->position, &tri->verts[i], normal);
			Normalize(normal, normal);
			return true;
		}
	}
	return false;
}

bool SphereOnTrianglePlane(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration, bool *onPlane) {
	// adjust sphere so triangle is origin
	Vec3 newSpherePosition;
	newSpherePosition.x = sphere->position->x - tri->verts[0].x;
	newSpherePosition.y = sphere->position->y - tri->verts[0].y;
	newSpherePosition.z = sphere->position->z - tri->verts[0].z;
	// finally, just apply a dot product
	f32 dot = DotProduct(&newSpherePosition, &(tri->normal));
	// don't collide if we're beneath the polygon!
	if (dot >= sphere->radius || dot < 0) {
		*onPlane = false;
		return false;
	}
	*penetration = sphere->radius - dot;
	// get closest position on the plane
	Vec3 spotOnPlane;
	spotOnPlane.x = mulf32(dot, -tri->normal.x);
	spotOnPlane.y = mulf32(dot, -tri->normal.y);
	spotOnPlane.z = mulf32(dot, -tri->normal.z);
	spotOnPlane.x += sphere->position->x;
	spotOnPlane.y += sphere->position->y;
	spotOnPlane.z += sphere->position->z;
	// now calculate the barycentric coordinates on the triangle
	Vec3 barycentric = BarycentricCoords(tri, &spotOnPlane);
	*onPlane = true;
	// if any are < 0 or > 1, no collision here. if all arent, then yes collision. also add some leniency
	if (barycentric.x >= -Fixed32ToNative(0) && barycentric.x <= Fixed32ToNative(4096)
	&& barycentric.y >= -Fixed32ToNative(0) && barycentric.y <= Fixed32ToNative(4096)
	&& barycentric.z >= -Fixed32ToNative(0) && barycentric.z <= Fixed32ToNative(4096)) {
		normal->x = tri->normal.x;
		normal->y = tri->normal.y;
		normal->z = tri->normal.z;
		return true;
	}
	return false;
}

MeshCollider *LoadCollisionMesh(char *input) {
	FILE *f = fopen(input, "rb");
	fseek(f, 0, SEEK_END);
	int fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	MeshCollider *mesh = malloc(fileSize);
	fread(mesh, fileSize, 1, f);
	fclose(f);
	mesh->triangles = (CollisionTriangle*)((uint)mesh + (uint)mesh->triangles);
	return mesh;
}

bool SphereOnSphere(CollisionSphere *sphere1, CollisionSphere *sphere2, f32 *penetration, Vec3 *normal) {
	// simple, just get distance
	Vec3 sub;
	Vec3Subtraction(sphere1->position, sphere2->position, &sub);
	int mag = Magnitude(&sub);
	if (mag < sphere1->radius + sphere2->radius) {
		Normalize(&sub, normal);
		*penetration = (sphere1->radius + sphere2->radius) - mag;
		return true;
	}
	return false;
}

void GenerateBoundsForBlocks(Vec3 *min, Vec3 *max, CollisionBlock* blocks) {
	Vec3 middle;
	middle.x = (min->x + max->x) / 2;
	middle.y = (min->y + max->y) / 2;
	middle.z = (min->z + max->z) / 2;
	// generate each block w/ a loop
	for (int i = 0; i < 8; ++i) {
		if (i % 2 == 1) {
			// right
			blocks[i].boundsMin.x = middle.x;
			blocks[i].boundsMax.x = max->x;
		}
		else {
			// left
			blocks[i].boundsMin.x = min->x;
			blocks[i].boundsMax.x = middle.x;
		}
		if (i % 4 >= 2) {
			// front
			blocks[i].boundsMin.z = middle.z;
			blocks[i].boundsMax.z = max->z;
		}
		else {
			// back
			blocks[i].boundsMin.z = min->z;
			blocks[i].boundsMax.z = middle.z;
		}
		if (i >= 4) {
			// top
			blocks[i].boundsMin.y = middle.y;
			blocks[i].boundsMax.y = max->y;
		}
		else {
			// bottom
			blocks[i].boundsMin.y = min->y;
			blocks[i].boundsMax.y = middle.y;
		}
	}
}

bool AABBCheck(Vec3 *minA, Vec3 *maxA, Vec3 *minB, Vec3 *maxB) {
	return (minA->x <= maxB->x &&
		maxA->x >= minB->x &&
		minA->y <= maxB->y &&
		maxA->y >= minB->y &&
		minA->z <= maxB->z &&
		maxA->z >= minB->z);
}

void GenerateOctree(CollisionBlock *currBlock, MeshCollider *currMesh, int currDepth) {
	++currDepth;
	currBlock->subdivided = false;
	currBlock->triCount = 0;
	int maxTris = OCTREE_MAX_TRIS;
	currBlock->triangleList = (unsigned int*)malloc(sizeof(unsigned int) * maxTris);
	// iterate over all the collision triangles in the mesh and see if they fall within the block
	for (int i = 0; i < currMesh->triCount; ++i) {
		if (AABBCheck(&currBlock->boundsMin, &currBlock->boundsMax, &currMesh->triangles[i].boundsMin, &currMesh->triangles[i].boundsMax)) {
			currBlock->triangleList[currBlock->triCount] = i;
			++currBlock->triCount;
			// max tris, either start a new subdivision or increase tri count
			if (currBlock->triCount >= maxTris && currDepth < OCTREE_MAX_DEPTH) {
				free(currBlock->triangleList);
				currBlock->subdivided = true;
				currBlock->triCount = 0;
				break;
			}
			else if (currBlock->triCount >= maxTris) {
				maxTris += OCTREE_MAX_TRIS;
				currBlock->triangleList = (unsigned int*)realloc(currBlock->triangleList, sizeof(unsigned int) * maxTris);
			}
		}
	}

	if (currBlock->subdivided) {
		currBlock->blocks = (CollisionBlock*)malloc(sizeof(CollisionBlock) * 8);
		GenerateBoundsForBlocks(&currBlock->boundsMin, &currBlock->boundsMax, currBlock->blocks);
		for (int i = 0; i < 8; ++i) {
			GenerateOctree(&currBlock->blocks[i], currMesh, currDepth);
		}
	}

}

MeshCollider *MeshColliderFromMesh(Model *input) {
	if (input == NULL) {
		return NULL;
	}
	MeshCollider *retValue = (MeshCollider*)malloc(sizeof(MeshCollider));
	// start with the AABB
	retValue->AABBPosition.x = (input->boundsMin.x + input->boundsMax.x) / 2;
	retValue->AABBPosition.y = (input->boundsMin.y + input->boundsMax.y) / 2;
	retValue->AABBPosition.z = (input->boundsMin.z + input->boundsMax.z) / 2;
	retValue->AABBBounds.x = input->boundsMax.x - retValue->AABBPosition.x;
	retValue->AABBBounds.y = input->boundsMax.y - retValue->AABBPosition.y;
	retValue->AABBBounds.z = input->boundsMax.z - retValue->AABBPosition.z;
	// and then move onto the triangles
	int triCount = 0;
	VertexHeader* currVertexGroup = input->vertexGroups;
	for (int i = 0; i < input->vertexGroupCount; ++i) {
		triCount += currVertexGroup->count / 3;
		currVertexGroup = (VertexHeader*)((uint)(&(currVertexGroup->vertices)) + (uint)(sizeof(Vertex) * (currVertexGroup->count)));
	}
	retValue->triangles = (CollisionTriangle*)malloc(sizeof(CollisionTriangle)*triCount);
	retValue->triCount = triCount;
	int currTri = 0;
	currVertexGroup = input->vertexGroups;
	for (int i = 0; i < input->vertexGroupCount; ++i) {
		Vertex *currVerts = &currVertexGroup->vertices;
		for (int j = 0; j < currVertexGroup->count; j += 3) {
			// get verts...
			for (int k = 0; k < 3; ++k) {
				retValue->triangles[currTri].verts[k].x = currVerts[j+k].x;
				retValue->triangles[currTri].verts[k].y = currVerts[j+k].y;
				retValue->triangles[currTri].verts[k].z = currVerts[j+k].z;
			}
			retValue->triangles[currTri].boundsMax.x = -4000000;
			retValue->triangles[currTri].boundsMax.y = -4000000;
			retValue->triangles[currTri].boundsMax.z = -4000000;
			retValue->triangles[currTri].boundsMin.x = 4000000;
			retValue->triangles[currTri].boundsMin.y = 4000000;
			retValue->triangles[currTri].boundsMin.z = 4000000;
			// calculate bounds extents now
			for (int k = 0; k < 3; ++k) {
				retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, currVerts[j+k].x);
				retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, currVerts[j+k].y);
				retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, currVerts[j+k].z);
				retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, currVerts[j+k].x);
				retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, currVerts[j+k].y);
				retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, currVerts[j+k].z);
			}
			// calculate normal
			NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
			currTri += 1;
		}
		currVertexGroup = (VertexHeader*)((uint)(&(currVertexGroup->vertices)) + (uint)(sizeof(Vertex) * (currVertexGroup->count)));
	}
#ifdef _NOTDS
	for (int i = 0; i < retValue->triCount; ++i) {
		for (int j = 0; j < 3; ++j) {
			retValue->triangles[i].verts[j].x = Fixed32ToNative(retValue->triangles[i].verts[j].x);
			retValue->triangles[i].verts[j].y = Fixed32ToNative(retValue->triangles[i].verts[j].y);
			retValue->triangles[i].verts[j].z = Fixed32ToNative(retValue->triangles[i].verts[j].z);
		}
		retValue->triangles[i].boundsMax.x = Fixed32ToNative(retValue->triangles[i].boundsMax.x);
		retValue->triangles[i].boundsMax.y = Fixed32ToNative(retValue->triangles[i].boundsMax.y);
		retValue->triangles[i].boundsMax.z = Fixed32ToNative(retValue->triangles[i].boundsMax.z);
		retValue->triangles[i].boundsMin.x = Fixed32ToNative(retValue->triangles[i].boundsMin.x);
		retValue->triangles[i].boundsMin.y = Fixed32ToNative(retValue->triangles[i].boundsMin.y);
		retValue->triangles[i].boundsMin.z = Fixed32ToNative(retValue->triangles[i].boundsMin.z);
	}
#endif

	Vec3 AABBMin;
	Vec3Subtraction(&retValue->AABBPosition, &retValue->AABBBounds, &AABBMin);
	Vec3 AABBMax;
	Vec3Addition(&retValue->AABBPosition, &retValue->AABBBounds, &AABBMax);
	GenerateBoundsForBlocks(&AABBMin, &AABBMax, retValue->blocks);
	for (int i = 0; i < 8; ++i) {
		GenerateOctree(&retValue->blocks[i], retValue, 0);
	}

	return retValue;
}

void FindTrianglesFromOctreeInternal(Vec3* min, Vec3* max, MeshCollider *mesh, CollisionBlock* block, unsigned int** retValue, int* maxSize, int* currSize) {
	if (block->subdivided) {
		for (int i = 0; i < 8; ++i) {
			if (AABBCheck(min, max, &block->blocks[i].boundsMin, &block->blocks[i].boundsMax)) {
				FindTrianglesFromOctreeInternal(min, max, mesh, &block->blocks[i], retValue, maxSize, currSize);
			}
		}
	}
	else {
		// ensure we overlap the triangles
		for (int i = 0; i < block->triCount; ++i) {
			const CollisionTriangle* currTri = &mesh->triangles[block->triangleList[i]];
			if (AABBCheck(min, max, &currTri->boundsMin, &currTri->boundsMax)) {
				// omit duplicates
				bool toContinue = false;
				for (int j = 0; j < *currSize; ++j) {
					if (retValue[0][j] == block->triangleList[i]) {
						toContinue = true;
						break;
					}
				}
				if (toContinue) {
					continue;
				}
				retValue[0][*currSize] = block->triangleList[i];
				++*currSize;
				if (*currSize >= *maxSize) {
					*maxSize += 512;
					retValue[0] = realloc(retValue[0], sizeof(unsigned int) * *maxSize);
				}
			}
		}
	}
}

unsigned int* FindTrianglesFromOctree(Vec3* min, Vec3* max, MeshCollider* meshCollider, int *totalTris) {
	unsigned int* retValue = (unsigned int*)malloc(sizeof(unsigned int) * 512);
	int maxSize = 512;
	int currSize = 0;
	for (int i = 0; i < 8; ++i) {
		if (AABBCheck(min, max, &meshCollider->blocks[i].boundsMin, &meshCollider->blocks[i].boundsMax)) {
			FindTrianglesFromOctreeInternal(min, max, meshCollider, &meshCollider->blocks[i], &retValue, &maxSize, &currSize);
		}
	}
	*totalTris = currSize;
	// dummy
	/*free(retValue);
	*totalTris = meshCollider->triCount;
	retValue = (unsigned int*)malloc(sizeof(unsigned int) * meshCollider->triCount);
	for (int i = 0; i < meshCollider->triCount; ++i) {
		retValue[i] = i;
	}*/
	return retValue;
}

void DestroyOctree(CollisionBlock* block) {
	if (block->subdivided) {
		for (int i = 0; i < 8; ++i) {
			DestroyOctree(&block->blocks[i]);
		}
	}
	// tomato tomato, both are ptrs here
	free(block->blocks);
}

void DestroyCollisionMesh(MeshCollider* meshCollider) {
	// destroy all the octrees...
	for (int i = 0; i < 8; ++i) {
		DestroyOctree(&meshCollider->blocks[i]);
	}
	free(meshCollider->triangles);
	free(meshCollider);
}

// doesn't return position by default since this should rarely be used by actual game code
bool RayOnAABB(Vec3* point, Vec3* direction, Vec3* boxMin, Vec3* boxMax, f32* t) {
	Vec3 tMin, tMax;
	Vec3 workVec;
	Vec3 newDir = *direction;
	// division by 0 fix
	if (newDir.x == 0) {
		newDir.x = Fixed32ToNative(1);
	}
	if (newDir.y == 0) {
		newDir.y = Fixed32ToNative(1);
	}
	if (newDir.z == 0) {
		newDir.z = Fixed32ToNative(1);
	}
#ifdef _NOTDS
	Vec3Subtraction(boxMin, point, &workVec);
	Vec3Division(&workVec, &newDir, &tMin);
	Vec3Subtraction(boxMax, point, &workVec);
	Vec3Division(&workVec, &newDir, &tMax);
	Vec3 t1 = { Min(tMin.x, tMax.x), Min(tMin.y, tMax.y), Min(tMin.z, tMax.z) };
	Vec3 t2 = { Max(tMin.x, tMax.x), Max(tMin.y, tMax.y), Max(tMin.z, tMax.z) };
	f32 tNear = Max(t1.x, Max(t1.y, t1.z));
	f32 tFar = Min(t2.x, Min(t2.y, t2.z));
	if (t != NULL) {
		*t = tNear;
	}
	return tNear <= tFar && tFar >= 0;
#else
	long long tMin1, tMin2, tMin3;
	long long tMax1, tMax2, tMax3;
	Vec3Subtraction(boxMin, point, &workVec);
	tMin1 = Int64Div(workVec.x, newDir.x);
	tMin2 = Int64Div(workVec.y, newDir.y);
	tMin3 = Int64Div(workVec.z, newDir.z);
	Vec3Subtraction(boxMax, point, &workVec);
	tMax1 = Int64Div(workVec.x, newDir.x);
	tMax2 = Int64Div(workVec.y, newDir.y);
	tMax3 = Int64Div(workVec.z, newDir.z);
	long long t11, t12, t13, t21, t22, t23;
	// min...
	t11 = tMin1 > tMax1 ? tMax1 : tMin1;
	t12 = tMin2 > tMax2 ? tMax2 : tMin2;
	t13 = tMin3 > tMax3 ? tMax3 : tMin3;
	// max...
	t21 = tMin1 > tMax1 ? tMin1 : tMax1;
	t22 = tMin2 > tMax2 ? tMin2 : tMax2;
	t23 = tMin3 > tMax3 ? tMin3 : tMax3;

	long long tNear, tFar;

	tNear = t11;
	if (t12 > tNear) {
		tNear = t12;
	}
	if (t13 > tNear) {
		tNear = t13;
	}

	tFar = t21;
	if (t22 < tFar) {
		tFar = t22;
	}
	if (t23 < tFar) {
		tFar = t23;
	}
	// who cares if it gets truncated
	if (t != NULL) {
		*t = tNear;
	}
	return tNear <= tFar && tFar >= 0;
#endif
}

bool RayOnSphere(Vec3* point, Vec3* direction, CollisionSphere* sphere, f32* t, Vec3* hitPos) {
	// TODO: int64 version for DS?
	Vec3 dist;
	Vec3Subtraction(point, sphere->position, &dist);
	f32 b = DotProduct(&dist, direction);
	f32 c = DotProduct(&dist, &dist) - mulf32(sphere->radius, sphere->radius);
	// return if it's outside of the sphere and pointing the wrong way
	if (c > 0 && b > 0) return false;
	f32 discr = mulf32(b, b) - c;
	// negative discriminant means a miss
	if (discr < 0) return false;
	// ray found to intersect sphere, compute intersection
	f32 hitDist = -b - sqrtf32(discr);
	// if t is negative, started in sphere
	if (hitDist < 0) {
		hitDist = 0;
	}
	if (t != NULL) {
		*t = hitDist;
	}
	if (hitPos != NULL) {
		hitPos->x = point->x + mulf32(hitDist, direction->x);
		hitPos->y = point->y + mulf32(hitDist, direction->y);
		hitPos->z = point->z + mulf32(hitDist, direction->z);
	}
	return true;
}

bool RayOnPlane(Vec3* point, Vec3* direction, Vec3* normal, f32 planeDistance, f32* t, Vec3* hitPos) {
	f32 nd = DotProduct(direction, normal);
	f32 pn = DotProduct(point, normal);
	// if nd is positive, they're facing the same way. no collision
	if (nd >= 0) {
		return false;
	}
	f32 hitDist = divf32(planeDistance - pn, nd);
	if (hitDist >= 0) {
		if (t != NULL) {
			*t = hitDist;
		}
		if (hitPos != NULL) {
			hitPos->x = point->x + mulf32(direction->x, hitDist);
			hitPos->y = point->y + mulf32(direction->y, hitDist);
			hitPos->z = point->z + mulf32(direction->z, hitDist);
			return true;
		}
	}
	return false;
}

bool RayOnTriangle(Vec3* point, Vec3* direction, CollisionTriangle* triangle, f32* t, Vec3* hitPos) {
	Vec3 tmpHitPos;
	if (hitPos == NULL) {
		hitPos = &tmpHitPos;
	}
	if (RayOnPlane(point, direction, &triangle->normal, DotProduct(&triangle->normal, &triangle->verts[0]), t, hitPos)) {
		Vec3 barycentric = BarycentricCoords(triangle, hitPos);
		if (barycentric.x >= -Fixed32ToNative(0) && barycentric.x <= Fixed32ToNative(4096)
			&& barycentric.y >= -Fixed32ToNative(0) && barycentric.y <= Fixed32ToNative(4096)
			&& barycentric.z >= -Fixed32ToNative(0) && barycentric.z <= Fixed32ToNative(4096)) {
			return true;
		}
	}

	return false;
}

int GetQuadTreeCountSub(CollisionBlock* block) {
	int count = 0;
	if (block->subdivided) {
		for (int i = 0; i < 8; ++i) {
			count += GetQuadTreeCountSub(&block->blocks[i]);
		}
	}
	else {
		if (block->triCount > 0) {
			count = 1;
		}
	}
	return count;
}

int GetQuadTreeCount(MeshCollider* mesh) {
	int count = 0;
	for (int i = 0; i < 8; ++i) {
		count += GetQuadTreeCountSub(&mesh->blocks[i]);
	}
	return count;
}

void RaycastQuadTreeSub(Vec3* point, Vec3* direction, f32 length, Vec3* AABBMin, Vec3* AABBMax, CollisionBlock* block, CollisionBlock** hitBlocks, int* hitBlockPosition, int* triCount) {
	f32 t;
	if (!block->subdivided) {
		if (block->triCount > 0) {
			if (AABBCheck(AABBMin, AABBMax, &block->boundsMin, &block->boundsMax)) {
				if (RayOnAABB(point, direction, &block->boundsMin, &block->boundsMax, &t)) {
					if (t <= length) {
						hitBlocks[*hitBlockPosition] = block;
						*hitBlockPosition += 1;
						*triCount += block->triCount;
					}
				}
			}
		}
	}
	else {
		for (int i = 0; i < 8; ++i) {
			RaycastQuadTreeSub(point, direction, length, AABBMin, AABBMax, &block->blocks[i], hitBlocks, hitBlockPosition, triCount);
		}
	}
}

void RaycastQuadTree(Vec3* point, Vec3* direction, f32 length, Vec3* AABBMin, Vec3* AABBMax, MeshCollider* mesh, CollisionBlock** hitBlocks, int* hitBlockPosition, int* triCount) {
	for (int i = 0; i < 8; ++i) {
		RaycastQuadTreeSub(point, direction, length, AABBMin, AABBMax, &mesh->blocks[i], hitBlocks, hitBlockPosition, triCount);
	}
}

bool RayOnMesh(Vec3* point, Vec3* direction, f32 length, Vec3* rayMin, Vec3* rayMax, MeshCollider* mesh, Vec3* meshOffset, Vec3* meshScale, Quaternion* meshRotation, f32* t, Vec3* hitPos, Vec3* normal, int* triId) {

	// attempt at optimization
	f32 maxBounds = Max(Max(mesh->AABBBounds.x, mesh->AABBBounds.y), mesh->AABBBounds.z);
	Vec3 oldMeshMax = { 
		mulf32(mesh->AABBPosition.x + maxBounds, meshScale->x) + meshOffset->x,
		mulf32(mesh->AABBPosition.y + maxBounds, meshScale->y) + meshOffset->y,
		mulf32(mesh->AABBPosition.z + maxBounds, meshScale->z) + meshOffset->z
	};

	Vec3 oldMeshMin = {
		mulf32(mesh->AABBPosition.x - maxBounds, meshScale->x) + meshOffset->x,
		mulf32(mesh->AABBPosition.y - maxBounds, meshScale->y) + meshOffset->y,
		mulf32(mesh->AABBPosition.z - maxBounds, meshScale->z) + meshOffset->z
	};

	if (!AABBCheck(rayMin, rayMax, &oldMeshMin, &oldMeshMax)) {
		return false;
	}

	// transform the point and direction
	Vec3 newPoint, newDirection;
	Vec3 workVec;
	Vec3Subtraction(point, meshOffset, &newPoint);
	Quaternion invQuat;
	QuaternionInverse(meshRotation, &invQuat);
	QuatTimesVec3(&invQuat, &newPoint, &workVec);
	Vec3Division(&workVec, meshScale, &newPoint);

	// direction now
	QuatTimesVec3(&invQuat, direction, &workVec);
	Vec3Division(&workVec, meshScale, &newDirection);
	Normalize(&newDirection, &newDirection);

	// generate new length value
	Vec3 absDir = {
		f32abs(newDirection.x),
		f32abs(newDirection.y),
		f32abs(newDirection.z)
	};
	Vec3 absScale = {
		f32abs(meshScale->x),
		f32abs(meshScale->y),
		f32abs(meshScale->z)
	};
	f32 lenDot = DotProduct(&absDir, &absScale);
	f32 newLength = divf32(length, lenDot);

	Vec3 rayPlusDir = {
	newPoint.x + mulf32(newDirection.x, newLength),
	newPoint.y + mulf32(newDirection.y, newLength),
	newPoint.z + mulf32(newDirection.z, newLength)
	};

	Vec3 rayAABBMin = {
		Min(newPoint.x, rayPlusDir.x),
		Min(newPoint.y, rayPlusDir.y),
		Min(newPoint.z, rayPlusDir.z)
	};
	Vec3 rayAABBMax = {
		Max(newPoint.x, rayPlusDir.x),
		Max(newPoint.y, rayPlusDir.y),
		Max(newPoint.z, rayPlusDir.z)
	};

	// check against the mesh AABB first
	f32 tempt;
	Vec3 AABBMin, AABBMax;
	Vec3Subtraction(&mesh->AABBPosition, &mesh->AABBBounds, &AABBMin);
	Vec3Addition(&mesh->AABBPosition, &mesh->AABBBounds, &AABBMax);
	if (RayOnAABB(&newPoint, &newDirection, &AABBMin, &AABBMax, &tempt)) {
		if (tempt > newLength) {
			return false;
		}
	}
	else {
		// simple AABB point check
		if (!(newPoint.x >= AABBMin.x && newPoint.x <= AABBMax.x &&
			newPoint.y >= AABBMin.y && newPoint.y <= AABBMax.y &&
			newPoint.z >= AABBMin.z && newPoint.z <= AABBMax.z)) {
			return false;
		}
	}

	// now raycast against the quadtrees
	int quadTreeCount = GetQuadTreeCount(mesh);

	CollisionBlock** hitBlocks = (CollisionBlock**)malloc(sizeof(CollisionBlock*) * quadTreeCount);
	int hitBlockPosition = 0;
	int triCount = 0;
	
	RaycastQuadTree(&newPoint, &newDirection, newLength, &rayAABBMin, &rayAABBMax, mesh, hitBlocks, &hitBlockPosition, &triCount);
	
	// TODO: potential optimization, sort by the quadtree positions so we have to sort fewer individual hits if applicable

	// all blocks hit, now create a list of triangles, omitting duplicates
	int* trisToCheck = (int*)malloc(sizeof(int) * triCount);
	int realTriCount = 0;
	f32 closestHit = Fixed32ToNative(2147483647);
	int closestTri;
	bool everHit = false;
	for (int i = 0; i < hitBlockPosition; ++i) {
		for (int j = 0; j < hitBlocks[i]->triCount; ++j) {
			// ensure no duplicates here
			bool duplicateTri = false;
			for (int k = 0; k < realTriCount; ++k) {
				if (trisToCheck[k] == hitBlocks[i]->triangleList[j]) {
					duplicateTri = true;
					break;
				}
			}
			if (duplicateTri) {
				continue;
			}
			trisToCheck[realTriCount] = hitBlocks[i]->triangleList[j];
			++realTriCount;

			// raycast the triangle now, starting with the AABB
			Vec3* newBoundsMin = &mesh->triangles[hitBlocks[i]->triangleList[j]].boundsMin;
			Vec3* newBoundsMax = &mesh->triangles[hitBlocks[i]->triangleList[j]].boundsMax;
			if (AABBCheck(&rayAABBMin, &rayAABBMax, newBoundsMin, newBoundsMax)) {
				//bool inAABB = (newPoint.x >= newBoundsMin->x && newPoint.x <= newBoundsMax->x &&
					//newPoint.y >= newBoundsMin->y && newPoint.y <= newBoundsMax->y &&
					//newPoint.z >= newBoundsMin->z && newPoint.z <= newBoundsMax->z);
				//if (inAABB || RayOnAABB(&newPoint, &newDirection, newBoundsMin, newBoundsMax, &tempt)) {
					//if (inAABB || tempt <= newLength) {
						if (RayOnTriangle(&newPoint, &newDirection, &mesh->triangles[hitBlocks[i]->triangleList[j]], &tempt, NULL)) {
							if (tempt < closestHit) {
								closestHit = tempt;
								closestTri = hitBlocks[i]->triangleList[j];
								everHit = true;
							}
						}
					//}
				//}
			}
		}
	}
	if (everHit) {
		if (t != NULL) {
			*t = mulf32(closestHit, lenDot);
		}
		if (hitPos != NULL) {
			f32 truLen;
			if (t != NULL) {
				truLen = *t;
			}
			else {
				truLen = divf32(closestHit, lenDot);
			}

			hitPos->x = point->x + mulf32(direction->x, truLen);
			hitPos->y = point->y + mulf32(direction->y, truLen);
			hitPos->z = point->z + mulf32(direction->z, truLen);
		}
		if (triId != NULL) {
			*triId = closestTri;
		}
		if (normal != NULL) {
			*normal = mesh->triangles[closestTri].normal;
		}
	}
	free(trisToCheck);
	free(hitBlocks);
	return everHit;
}