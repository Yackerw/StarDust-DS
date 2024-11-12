#include <stdio.h>
#include "sdcollision.h"
#include <nds.h>
#include "sdmath.h"
#include <stdint.h>
#define OCTREE_MAX_DEPTH 10
#define OCTREE_MAX_TRIS 40
// note: barely noticeable slowdown from this, and fixes a bug with large polygons
#define FLOATBARY
//#define BARY64

// these are the same, but they're different defines for clarity anyways
#define ShortToVec3(sh, ve) (ve).x = (sh).x; (ve).y = (sh).y; (ve).z = (sh).z
#define Vec3ToShort(ve, sh) (sh).x = (ve).x; (sh).y = (ve).y; (sh).z = (ve).z

float dotf(float x1, float y1, float z1, float x2, float y2, float z2) {
	return x1 * x2 + y1 * y2 + z1 * z2;
}

#if defined(BARY64_DEBUG)
// debug functions
long long dot64(Vec3* left, Vec3* right) {
	// note: i've decided to extend the precision for these by 12 bits for the barycentric calculations. it's still a net gain of 20 bits before overflow
	long long work = ((long long)(left->x * 4096.0f) * (long long)(right->x * 4096.0f));
	long long work2 = ((long long)(left->y * 4096.0f) * (long long)(right->y * 4096.0f));
	long long work3 = ((long long)(left->z * 4096.0f) * (long long)(right->z * 4096.0f));
	return work + work2 + work3;
}

long long mulf64(long long left, long long right) {
	return (left * right) >> 24;
}

long long divf64(long long left, long long right) {
	return (left << 12) / right;
}
#else
long long dot64(Vec3* left, Vec3* right) {
	// note: i've decided to extend the precision for these by 12 bits for the barycentric calculations. it's still a net gain of 20 bits before overflow
	long long work = ((long long)left->x * (long long)right->x);
	long long work2 = ((long long)left->y * (long long)right->y);
	long long work3 = ((long long)left->z * (long long)right->z);
	return work + work2 + work3;
}

long long mulf64(long long left, long long right) {
	return (left * right) >> 24;
}

long long divf64(long long left, long long right) {
#ifndef _NOTDS
	REG_DIVCNT = DIV_64_64;

	while (REG_DIVCNT & DIV_BUSY);

	// really hope this still works
	REG_DIV_NUMER = left << 12;
	REG_DIV_DENOM = right;

	while (REG_DIVCNT & DIV_BUSY);

	return (REG_DIV_RESULT);
#else
	return (left << 12) / right;
#endif
}
#endif

void Vec3Subtractions(Vec3s* left, Vec3s* right, Vec3* out) {
	out->x = left->x - right->x;
	out->y = left->y - right->y;
	out->z = left->z - right->z;
}

Vec3 BarycentricCoords(CollisionTriangle *tri, Vec3 *point) {
	Vec3 v0, v1, v2;
	Vec3Subtractions(&tri->verts[1], &tri->verts[0], &v0);
	Vec3Subtractions(&tri->verts[2], &tri->verts[0], &v1);
	ShortToVec3(tri->verts[0], v2);
	Vec3Subtraction(point, &v2, &v2);
	// FLOATBARY is...probably a big bottleneck. maybe.
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
	retValue.z = 4096 - retValue.x - retValue.y;
	return retValue;
	#else
#if defined(BARY64)
	long long d00 = dot64(&v0, &v0);
	long long d01 = dot64(&v0, &v1);
	long long d11 = dot64(&v1, &v1);
	long long d20 = dot64(&v2, &v0);
	long long d21 = dot64(&v2, &v1);
	long long denom = mulf64(d00, d11) - mulf64(d01, d01);
	Vec3 retValue;
	retValue.x = divf64(mulf64(d11, d20) - mulf64(d01, d21), denom);
	retValue.y = divf64(mulf64(d00, d21) - mulf64(d01, d20), denom);
	retValue.z = 4096 - retValue.x - retValue.y;
	return retValue;
#elif defined (BARY64_DEBUG)
	long long d00 = dot64(&v0, &v0);
	long long d01 = dot64(&v0, &v1);
	long long d11 = dot64(&v1, &v1);
	long long d20 = dot64(&v2, &v0);
	long long d21 = dot64(&v2, &v1);
	long long denom = mulf64(d00, d11) - mulf64(d01, d01);
	Vec3 retValue;
	retValue.x = divf64(mulf64(d11, d20) - mulf64(d01, d21), denom) / 4096.0f;
	retValue.y = divf64(mulf64(d00, d21) - mulf64(d01, d20), denom) / 4096.0f;;
	retValue.z = 4096 - retValue.x - retValue.y;
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
	retValue.z = 4096 - retValue.x - retValue.y;
	
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
	f32 magLine = SqrMagnitude(&working3);
	Vec3Subtraction(p1, closestPoint, &working);
	f32 magPoint1 = Magnitude(&working);
	Vec3Subtraction(p2, closestPoint, &working);
	f32 magPoint2 = Magnitude(&working);
	magPoint1 += magPoint2;
	// this saves us 1 (one) sqrt call for magLine. worth, I think.
	magPoint1 = mulf32(magPoint1, magPoint1);
	// add a little leniency for, uh...lack of precision
	if (magPoint1 <= magLine + 32 && magPoint1 >= magLine - 32) {
		return true;
	}
	
	return false;
}

bool SphereOnTriangleLine(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration) {
	// and finally, line collision
	for (int i = 0; i < 3; ++i) {
		Vec3 closestPoint;
		Vec3 v1;
		Vec3 v2;
		ShortToVec3(tri->verts[i], v1);
		ShortToVec3(tri->verts[(i + 1) % 3], v2);
		if (SphereOnLine(sphere, &v1, &v2, &closestPoint)) {
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
		Vec3 v;
		ShortToVec3(tri->verts[i], v);
		if (SphereOnPoint(sphere, &v, &pointMag)) {
			// get penetration and normal
			*penetration = pointMag;
			// normal; subtract the two vectors, then normalize
			Vec3Subtraction(sphere->position, &v, normal);
			Normalize(normal, normal);
			return true;
		}
	}
	return false;
}

long long dot64_2(long long x1, long long y1, long long z1, long long x2, long long y2, long long z2) {
	return mulf64(x1, x2) + mulf64(y1, y2) + mulf64(z1, z2);
}

bool SphereOnTrianglePlane(CollisionSphere *sphere, CollisionTriangle *tri, Vec3 *normal, f32 *penetration, bool *onPlane) {
	// adjust sphere so triangle is origin
	Vec3 newSpherePosition;
	newSpherePosition.x = sphere->position->x - tri->verts[0].x;
	newSpherePosition.y = sphere->position->y - tri->verts[0].y;
	newSpherePosition.z = sphere->position->z - tri->verts[0].z;
	// finally, just apply a dot product
	Vec3 n;
	ShortToVec3(tri->normal, n);
	f32 dot = DotProduct(&newSpherePosition, &n);
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
	if (barycentric.x >= 0 && barycentric.x <= 4096
	&& barycentric.y >= 0 && barycentric.y <= 4096
	&& barycentric.z >= 0 && barycentric.z <= 4096) {
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
	mesh->triangles = (CollisionTriangle*)((uint32_t)mesh + (uint32_t)mesh->triangles);
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

void GenerateBoundsForBlocks(Vec3s *min, Vec3s *max, CollisionBlock* blocks) {
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

bool AABBCheckLeniency(Vec3* minA, Vec3* maxA, Vec3* minB, Vec3* maxB, f32 leniency) {
	return (minA->x <= maxB->x + leniency &&
		maxA->x >= minB->x - leniency &&
		minA->y <= maxB->y + leniency &&
		maxA->y >= minB->y - leniency &&
		minA->z <= maxB->z + leniency &&
		maxA->z >= minB->z - leniency);
}

bool AABBCheckLeniencyShort(Vec3s* minA, Vec3s* maxA, Vec3s* minB, Vec3s* maxB, f32 leniency) {
	return (minA->x <= maxB->x + leniency &&
		maxA->x >= minB->x - leniency &&
		minA->y <= maxB->y + leniency &&
		maxA->y >= minB->y - leniency &&
		minA->z <= maxB->z + leniency &&
		maxA->z >= minB->z - leniency);
}

void GenerateOctree(CollisionBlock *currBlock, MeshCollider *currMesh, int currDepth) {
	++currDepth;
	currBlock->subdivided = false;
	currBlock->triCount = 0;
	int maxTris = OCTREE_MAX_TRIS;
	currBlock->triangleList = (unsigned short*)malloc(sizeof(unsigned short) * maxTris);
	// iterate over all the collision triangles in the mesh and see if they fall within the block
	for (int i = 0; i < currMesh->triCount; ++i) {
		// fixes occasionally broken faces on DS
		if (AABBCheckLeniencyShort(&currBlock->boundsMin, &currBlock->boundsMax, &currMesh->triangles[i].boundsMin, &currMesh->triangles[i].boundsMax, 1)) {
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
				currBlock->triangleList = (unsigned short*)realloc(currBlock->triangleList, sizeof(unsigned short) * maxTris);
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
		if (!(currVertexGroup->bitFlags & VTX_QUAD)) {
			if (currVertexGroup->bitFlags & VTX_STRIPS) {
				triCount += 1 + (currVertexGroup->count - 3);
			}
			else {
				triCount += currVertexGroup->count / 3;
			}
		}
		else {
			if (currVertexGroup->bitFlags & VTX_STRIPS) {
				// two verts to make one quad, or two triangles.
				triCount += 2 + (currVertexGroup->count - 4);
			}
			else {
				triCount += (currVertexGroup->count / 4) * 2;
			}
		}
		currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
	}
	retValue->triangles = (CollisionTriangle*)malloc(sizeof(CollisionTriangle)*triCount);
	retValue->triCount = triCount;
	int currTri = 0;
	currVertexGroup = input->vertexGroups;
	for (int i = 0; i < input->vertexGroupCount; ++i) {
		Vertex *currVerts = &currVertexGroup->vertices;
		for (int j = 0; j < currVertexGroup->count; j += 3) {
			// get verts...
			if (!(currVertexGroup->bitFlags & VTX_QUAD)) {
				if (!(currVertexGroup->bitFlags & VTX_STRIPS) || j < 3) {
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].verts[k].x = currVerts[j + k].x;
						retValue->triangles[currTri].verts[k].y = currVerts[j + k].y;
						retValue->triangles[currTri].verts[k].z = currVerts[j + k].z;
					}
				}
				else {
					// not sure why i have to reverse the winding order for these, but i do
					if ((j & 1) == 0) {
						retValue->triangles[currTri].verts[1].x = currVerts[j - 2].x;
						retValue->triangles[currTri].verts[1].y = currVerts[j - 2].y;
						retValue->triangles[currTri].verts[1].z = currVerts[j - 2].z;
						retValue->triangles[currTri].verts[2].x = currVerts[j - 1].x;
						retValue->triangles[currTri].verts[2].y = currVerts[j - 1].y;
						retValue->triangles[currTri].verts[2].z = currVerts[j - 1].z;
						retValue->triangles[currTri].verts[0].x = currVerts[j].x;
						retValue->triangles[currTri].verts[0].y = currVerts[j].y;
						retValue->triangles[currTri].verts[0].z = currVerts[j].z;
					}
					else {
						retValue->triangles[currTri].verts[2].x = currVerts[j - 2].x;
						retValue->triangles[currTri].verts[2].y = currVerts[j - 2].y;
						retValue->triangles[currTri].verts[2].z = currVerts[j - 2].z;
						retValue->triangles[currTri].verts[1].x = currVerts[j - 1].x;
						retValue->triangles[currTri].verts[1].y = currVerts[j - 1].y;
						retValue->triangles[currTri].verts[1].z = currVerts[j - 1].z;
						retValue->triangles[currTri].verts[0].x = currVerts[j].x;
						retValue->triangles[currTri].verts[0].y = currVerts[j].y;
						retValue->triangles[currTri].verts[0].z = currVerts[j].z;
					}
					// fix j to increment for 1 vert instead of 1 tri
					j -= 2;
				}
				retValue->triangles[currTri].boundsMax.x = -32768;
				retValue->triangles[currTri].boundsMax.y = -32768;
				retValue->triangles[currTri].boundsMax.z = -32768;
				retValue->triangles[currTri].boundsMin.x = 32767;
				retValue->triangles[currTri].boundsMin.y = 32767;
				retValue->triangles[currTri].boundsMin.z = 32767;
				// calculate bounds extents now
				for (int k = 0; k < 3; ++k) {
					retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, currVerts[j + k].x);
					retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, currVerts[j + k].y);
					retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, currVerts[j + k].z);
					retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, currVerts[j + k].x);
					retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, currVerts[j + k].y);
					retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, currVerts[j + k].z);
				}
				// calculate normal
				NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
				currTri += 1;
			}
			else {
				if (!(currVertexGroup->bitFlags & VTX_STRIPS) || j < 4) {
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].verts[k].x = currVerts[j + k].x;
						retValue->triangles[currTri].verts[k].y = currVerts[j + k].y;
						retValue->triangles[currTri].verts[k].z = currVerts[j + k].z;
					}
					retValue->triangles[currTri].boundsMax.x = -32768;
					retValue->triangles[currTri].boundsMax.y = -32768;
					retValue->triangles[currTri].boundsMax.z = -32768;
					retValue->triangles[currTri].boundsMin.x = 32767;
					retValue->triangles[currTri].boundsMin.y = 32767;
					retValue->triangles[currTri].boundsMin.z = 32767;
					// calculate bounds extents now
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, currVerts[j + k].x);
						retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, currVerts[j + k].y);
						retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, currVerts[j + k].z);
						retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, currVerts[j + k].x);
						retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, currVerts[j + k].y);
						retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, currVerts[j + k].z);
					}
					// calculate normal
					NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
					currTri += 1;
					++j;
					retValue->triangles[currTri].verts[0].x = currVerts[j + 2].x;
					retValue->triangles[currTri].verts[0].y = currVerts[j + 2].y;
					retValue->triangles[currTri].verts[0].z = currVerts[j + 2].z;
					retValue->triangles[currTri].verts[1] = retValue->triangles[currTri - 1].verts[0];
					retValue->triangles[currTri].verts[2] = retValue->triangles[currTri - 1].verts[2];
					// reverse winding if it's a strip
					if (currVertexGroup->bitFlags & VTX_STRIPS) {
						Vec3s tmp = retValue->triangles[currTri].verts[0];
						retValue->triangles[currTri].verts[0] = retValue->triangles[currTri - 1].verts[1];
						retValue->triangles[currTri].verts[1] = tmp;
						retValue->triangles[currTri].verts[2] = retValue->triangles[currTri - 1].verts[2];
					}
					retValue->triangles[currTri].boundsMax.x = -32768;
					retValue->triangles[currTri].boundsMax.y = -32768;
					retValue->triangles[currTri].boundsMax.z = -32768;
					retValue->triangles[currTri].boundsMin.x = 32767;
					retValue->triangles[currTri].boundsMin.y = 32767;
					retValue->triangles[currTri].boundsMin.z = 32767;
					// calculate bounds extents now
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, retValue->triangles[currTri].verts[k].z);
						retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, retValue->triangles[currTri].verts[k].z);
					}
					// calculate normal
					NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
				}
				else {
					// create virtual quad
					Vec3s quad[4];
					quad[0].x = currVerts[j - 1].x;
					quad[0].y = currVerts[j - 1].y;
					quad[0].z = currVerts[j - 1].z;
					quad[1].x = currVerts[j - 2].x;
					quad[1].y = currVerts[j - 2].y;
					quad[1].z = currVerts[j - 2].z;
					quad[2].x = currVerts[j + 1].x;
					quad[2].y = currVerts[j + 1].y;
					quad[2].z = currVerts[j + 1].z;
					quad[3].x = currVerts[j].x;
					quad[3].y = currVerts[j].y;
					quad[3].z = currVerts[j].z;
					// adjust j position
					j -= 1;

					retValue->triangles[currTri].verts[0] = quad[2];
					retValue->triangles[currTri].verts[1] = quad[1];
					retValue->triangles[currTri].verts[2] = quad[0];
					retValue->triangles[currTri].boundsMax.x = -32768;
					retValue->triangles[currTri].boundsMax.y = -32768;
					retValue->triangles[currTri].boundsMax.z = -32768;
					retValue->triangles[currTri].boundsMin.x = 32767;
					retValue->triangles[currTri].boundsMin.y = 32767;
					retValue->triangles[currTri].boundsMin.z = 32767;
					// calculate bounds extents now
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, retValue->triangles[currTri].verts[k].z);
						retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, retValue->triangles[currTri].verts[k].z);
					}
					// calculate normal
					NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
					currTri += 1;
					retValue->triangles[currTri].verts[0] = quad[1];
					retValue->triangles[currTri].verts[1] = quad[2];
					retValue->triangles[currTri].verts[2] = quad[3];
					retValue->triangles[currTri].boundsMax.x = -32768;
					retValue->triangles[currTri].boundsMax.y = -32768;
					retValue->triangles[currTri].boundsMax.z = -32768;
					retValue->triangles[currTri].boundsMin.x = 32767;
					retValue->triangles[currTri].boundsMin.y = 32767;
					retValue->triangles[currTri].boundsMin.z = 32767;
					// calculate bounds extents now
					for (int k = 0; k < 3; ++k) {
						retValue->triangles[currTri].boundsMax.x = Max(retValue->triangles[currTri].boundsMax.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMax.y = Max(retValue->triangles[currTri].boundsMax.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMax.z = Max(retValue->triangles[currTri].boundsMax.z, retValue->triangles[currTri].verts[k].z);
						retValue->triangles[currTri].boundsMin.x = Min(retValue->triangles[currTri].boundsMin.x, retValue->triangles[currTri].verts[k].x);
						retValue->triangles[currTri].boundsMin.y = Min(retValue->triangles[currTri].boundsMin.y, retValue->triangles[currTri].verts[k].y);
						retValue->triangles[currTri].boundsMin.z = Min(retValue->triangles[currTri].boundsMin.z, retValue->triangles[currTri].verts[k].z);
					}
					// calculate normal
					NormalFromVertsFloat(&retValue->triangles[currTri].verts[0], &retValue->triangles[currTri].verts[1], &retValue->triangles[currTri].verts[2], &retValue->triangles[currTri].normal);
				}
				currTri += 1;
			}
		}
		currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
	}

	Vec3 AABBMin;
	Vec3Subtraction(&retValue->AABBPosition, &retValue->AABBBounds, &AABBMin);
	Vec3 AABBMax;
	Vec3Addition(&retValue->AABBPosition, &retValue->AABBBounds, &AABBMax);
	Vec3s AABBMins, AABBMaxs;
	Vec3ToShort(AABBMin, AABBMins);
	Vec3ToShort(AABBMax, AABBMaxs);
	GenerateBoundsForBlocks(&AABBMins, &AABBMaxs, retValue->blocks);
	for (int i = 0; i < 8; ++i) {
		GenerateOctree(&retValue->blocks[i], retValue, 0);
	}

	return retValue;
}

void FindTrianglesFromOctreeInternal(Vec3* min, Vec3* max, MeshCollider *mesh, CollisionBlock* block, unsigned short** retValue, int* maxSize, int* currSize) {
	if (block->subdivided) {
		for (int i = 0; i < 8; ++i) {
			Vec3 blockMin, blockMax;
			ShortToVec3(block->blocks[i].boundsMin, blockMin);
			ShortToVec3(block->blocks[i].boundsMax, blockMax);
			if (AABBCheck(min, max, &blockMin, &blockMax)) {
				FindTrianglesFromOctreeInternal(min, max, mesh, &block->blocks[i], retValue, maxSize, currSize);
			}
		}
	}
	else {
		// ensure we overlap the triangles
		for (int i = 0; i < block->triCount; ++i) {
			const CollisionTriangle* currTri = &mesh->triangles[block->triangleList[i]];
			Vec3 vMin, vMax;
			ShortToVec3(currTri->boundsMin, vMin);
			ShortToVec3(currTri->boundsMax, vMax);
			if (AABBCheck(min, max, &vMin, &vMax)) {
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

unsigned short* FindTrianglesFromOctree(Vec3* min, Vec3* max, MeshCollider* meshCollider, int *totalTris) {
	unsigned short* retValue = (unsigned short*)malloc(sizeof(unsigned short) * 512);
	int maxSize = 512;
	int currSize = 0;
	for (int i = 0; i < 8; ++i) {
		Vec3 blockMin, blockMax;
		ShortToVec3(meshCollider->blocks[i].boundsMin, blockMin);
		ShortToVec3(meshCollider->blocks[i].boundsMax, blockMax);
		if (AABBCheck(min, max, &blockMin, &blockMax)) {
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
bool RayOnAABB(Vec3* point, Vec3* direction, Vec3* boxMin, Vec3* boxMax, Vec3* normal, f32* t) {
	Vec3 tMin, tMax;
	Vec3 workVec;
	Vec3 newDir = *direction;
	// division by 0 fix
	if (newDir.x == 0) {
		newDir.x = 1;
	}
	if (newDir.y == 0) {
		newDir.y = 1;
	}
	if (newDir.z == 0) {
		newDir.z = 1;
	}
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
	long long dist;
	if (tNear < 0) {
		dist = tFar;
	}
	else {
		dist = tNear;
	}
	if (t != NULL) {
		*t = dist;
	}

	// return normal as well
	if (normal != NULL) {
		f32 tSeries[] = { tMin1, tMax1, tMin2, tMax2, tMin3, tMax3 };
		Vec3 normals[] = { {-4096, 0, 0 }, {4096, 0, 0},
			{0, -4096, 0}, {0, 4096, 0},
			{0, 0, -4096}, {0, 0, 4096} };
		for (int i = 0; i < 6; ++i) {
			if (dist == tSeries[i]) {
				*normal = normals[i];
				break;
			}
		}
	}

	return tNear <= tFar && tFar >= 0;
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
	Vec3 n, v;
	ShortToVec3(triangle->normal, n);
	ShortToVec3(triangle->verts[0], v);
	if (RayOnPlane(point, direction, &n, DotProduct(&n, &v), t, hitPos)) {
		Vec3 barycentric = BarycentricCoords(triangle, hitPos);
		if (barycentric.x >= 0 && barycentric.x <= 4096
			&& barycentric.y >= 0 && barycentric.y <= 4096
			&& barycentric.z >= 0 && barycentric.z <= 4096) {
			return true;
		}
	}

	return false;
}

int GetQuadTreeCountSub(CollisionBlock* block) {
	// potentially recode this to do the subdivided check before calling on it, to avoid extra function calls as a minor optimization
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
			Vec3 bMin, bMax;
			ShortToVec3(block->boundsMin, bMin);
			ShortToVec3(block->boundsMax, bMax);
			if (AABBCheck(AABBMin, AABBMax, &bMin, &bMax)) {
				if (RayOnAABB(point, direction, &bMin, &bMax, NULL, &t)) {
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
	if (RayOnAABB(&newPoint, &newDirection, &AABBMin, &AABBMax, NULL, &tempt)) {
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
	f32 closestHit = 2147483647;
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
			Vec3 newBoundsMin;
			Vec3 newBoundsMax;
			ShortToVec3(mesh->triangles[hitBlocks[i]->triangleList[j]].boundsMin, newBoundsMin);
			ShortToVec3(mesh->triangles[hitBlocks[i]->triangleList[j]].boundsMax, newBoundsMax);
			if (AABBCheck(&rayAABBMin, &rayAABBMax, &newBoundsMin, &newBoundsMax)) {
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
			//*normal = mesh->triangles[closestTri].normal;
			ShortToVec3(mesh->triangles[closestTri].normal, *normal);
		}
	}
	free(trisToCheck);
	free(hitBlocks);
	return everHit;
}

void ClosestPointAABB(Vec3* position, Vec3* boxMin, Vec3* boxMax, Vec3* out) {
	*out = *position;

	out->x = (out->x < boxMin->x) ? boxMin->x : out->x;
	out->y = (out->y < boxMin->y) ? boxMin->y : out->y;
	out->z = (out->z < boxMin->z) ? boxMin->z : out->z;

	out->x = (out->x > boxMax->x) ? boxMax->x : out->x;
	out->y = (out->y > boxMax->y) ? boxMax->y : out->y;
	out->z = (out->z > boxMax->z) ? boxMax->z : out->z;
}

// essentially just sphere on AABB but applying inverse rotation to the sphere
bool SphereOnOBB(CollisionSphere* sphere, CollisionBox* box, Vec3* hitPos, Vec3* normal, f32* t) {
	Vec3 rotatedSpherePoint, workVec;
	Vec3Subtraction(sphere->position, box->position, &workVec);
	Quaternion invQuat;
	QuaternionInverse(box->rotation, &invQuat);
	QuatTimesVec3(&invQuat, &workVec, &rotatedSpherePoint);

	// center around 0
	Vec3 boxMin;
	Vec3 zeroVec = { 0, 0, 0 };
	Vec3Subtraction(&zeroVec, &box->extents, &boxMin);

	Vec3 closestPoint;
	ClosestPointAABB(&rotatedSpherePoint, &boxMin, &box->extents, &closestPoint);
	Vec3 closestRelativeToSphere;
	Vec3Subtraction(&rotatedSpherePoint, &closestPoint, &closestRelativeToSphere);
	f32 sqrDist = SqrMagnitude(&closestRelativeToSphere);

	if (sqrDist <= mulf32(sphere->radius, sphere->radius)) {
		*hitPos = closestPoint;
		if (sqrDist <= 1) {
			// we're INSIDE the cube, fix!
			Normalize(&rotatedSpherePoint, normal);

			// gross i know but nothing better came to me
			// 2867 = 0.7f
			const f32 normalCheck = 2867;
			if (normal->y >= normalCheck) {
				normal->x = 0;
				normal->y = 4096;
				normal->z = 0;
			} else if (normal->y <= -normalCheck) {
				normal->x = 0;
				normal->y = -4096;
				normal->z = 0;
			} else if (normal->x >= normalCheck) {
				normal->x = 4096;
				normal->y = 0;
				normal->z = 0;
			} else if (normal->x <= -normalCheck) {
				normal->y = 0;
				normal->x = -4096;
				normal->z = 0;
			} else if (normal->z >= normalCheck) {
				normal->x = 0;
				normal->z = 4096;
				normal->y = 0;
			}
			else {
				normal->x = 0;
				normal->y = 0;
				normal->z = -4096;
			}
			// this also sucks
			*t = sphere->radius + f32abs(DotProduct(normal, &box->extents)) - DotProduct(normal, &rotatedSpherePoint);
		}
		else {
			// we're outside of cube, return values
			Normalize(&closestRelativeToSphere, normal);
			*t = sphere->radius - sqrtf32(sqrDist);
		}
		Vec3 tmpNormal;
		QuatTimesVec3(box->rotation, normal, &tmpNormal);
		*normal = tmpNormal;
		return true;
	}
	return false;
}