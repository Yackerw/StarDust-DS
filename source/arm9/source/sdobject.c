#include "sdobject.h"
#include "sdcollision.h"
#include "sddelta.h"
#include <stdio.h>
#include <nds.h>
#include <stdbool.h>
#include <stdlib.h>

Object firstObject;

void(*updateFuncs[256])(Object*);
void(*lateUpdateFuncs[256])(Object*);
void(*startFuncs[256])(Object*);
bool(*collisionFuncs[256])(Object*, CollisionHit*);
void(*destroyFuncs[256])(Object*);
void(*networkCreateFuncs[256])(Object*, void**, int*);
void(*networkCreateReceiveFuncs[256])(Object*, void*, int);
void(*networkPacketReceiveFuncs[256])(Object*, void*, int, int, int);
bool networkedObjectTypes[256];
int currFunc;

int objPacketId;
int objCreateId;
int objStopSyncId;

ObjectPtr* networkedObjects;
int networkedObjectsCount;

bool layerCollision[1024];

void DestroyObjectInternal(Object *object) {
	if (destroyFuncs[object->objectType] != NULL) {
		destroyFuncs[object->objectType](object);
	}

	// if it's synced, stop syncing it
	if (object->netId != -1) {
		StopSyncingObject(object);
	}

	if (object->next != NULL) {
		object->next->previous = object->previous;
	}
	object->previous->next = object->next;

	// free the pointers to it
	ObjectPtr* currPtr = object->references.next;
	while (currPtr != NULL) {
		currPtr->object = NULL;
		currPtr = currPtr->next;
	}

	free(object);
}

bool RaycastWorld(Vec3* point, Vec3* direction, f32 length, unsigned int layerMask, f32* t, CollisionHit* hitInfo) {
	Object* colObject = firstObject.next;

	bool everHit = false;
	f32 closestHit = length;
	Object* closestObject = NULL;
	int closestHitType = 0;
	int closestTriHit;
	Vec3 closestNormal;

	Vec3 rayPlusDir = {
		point->x + mulf32(direction->x, length),
		point->y + mulf32(direction->y, length),
		point->z + mulf32(direction->z, length)
	};

	Vec3 rayAABBMin = {
		Min(point->x, rayPlusDir.x),
		Min(point->y, rayPlusDir.y),
		Min(point->z, rayPlusDir.z)
	};
	Vec3 rayAABBMax = {
		Max(point->x, rayPlusDir.x),
		Max(point->y, rayPlusDir.y),
		Max(point->z, rayPlusDir.z)
	};

	while (colObject != NULL) {
		// ensure object is solid and on the layer mask
		if (colObject->solid && ((1 << (colObject->layer - 1)) & layerMask) != 0) {
			f32 tempT;
			Vec3 tempNorm;
			int tempTri;
			if (colObject->sphereCol != NULL) {
				// oh boy, raycast against S P H E R E
				if (RayOnSphere(point, direction, colObject->sphereCol, &tempT, NULL)); {
					if (tempT <= closestHit) {
						everHit = true;
						closestHit = tempT;
						closestTriHit = -1;
						closestHitType = COLLIDER_SPHERE;
						closestObject = colObject;
					}
				}
			}
			if (colObject->meshCol != NULL) {
				if (RayOnMesh(point, direction, length, &rayAABBMin, &rayAABBMax, colObject->meshCol, &colObject->position, &colObject->scale, &colObject->rotation, &tempT, NULL, &tempNorm, &tempTri)) {
					if (tempT <= closestHit) {
						everHit = true;
						closestTriHit = tempTri;
						closestNormal = tempNorm;
						closestHit = tempT;
						closestHitType = COLLIDER_MESH;
						closestObject = colObject;
					}
				}
			}
			if (colObject->boxCol != NULL) {
				Vec3 boxMin;
				boxMin.x = -colObject->boxCol->extents.x;
				boxMin.y = -colObject->boxCol->extents.y;
				boxMin.z = -colObject->boxCol->extents.z;
				// rotate point and direction
				Vec3 rotatedPoint, workVec;
				Vec3Subtraction(point, colObject->boxCol->position, &workVec);
				Quaternion inverseRot;
				QuaternionInverse(colObject->boxCol->rotation, &inverseRot);
				QuatTimesVec3(&inverseRot, &workVec, &rotatedPoint);
				Vec3 rotatedDir;
				QuatTimesVec3(&inverseRot, direction, &rotatedDir);
				// ray on AABB
				if (RayOnAABB(&rotatedPoint, &rotatedDir, &boxMin, &colObject->boxCol->extents, &tempNorm, &tempT)) {
					if (tempT <= closestHit && tempT <= length) {
						everHit = true;
						QuatTimesVec3(colObject->boxCol->rotation, &tempNorm, &closestNormal);
						closestHit = tempT;
						closestHitType = COLLIDER_BOX;
						closestObject = colObject;
						closestTriHit = -1;
					}
				}
			}
		}
		colObject = colObject->next;
	}

	if (everHit) {
		if (t != NULL) {
			*t = closestHit;
		}
		if (hitInfo != NULL) {
			hitInfo->hitPos.x = point->x + mulf32(closestHit, direction->x);
			hitInfo->hitPos.y = point->y + mulf32(closestHit, direction->y);
			hitInfo->hitPos.z = point->z + mulf32(closestHit, direction->z);
			// generate normal for sphere
			if (closestHitType == COLLIDER_SPHERE) {
				Vec3Subtraction(&hitInfo->hitPos, &closestObject->position, &hitInfo->normal);
				Normalize(&hitInfo->normal, &hitInfo->normal);
			}
			else {
				hitInfo->normal = closestNormal;
			}
			hitInfo->hitObject = closestObject;
			hitInfo->hitTri = closestTriHit;
			hitInfo->colliderType = closestHitType;
		}
		return true;
	}
	return false;
}

int SphereCollisionCheck(CollisionSphere *sphere, unsigned int layerMask, CollisionHit* hitInfos, int maxHit) {
	// iterate over all objects and get mesh colliders
	Object *meshObject = firstObject.next;
	// you MUST pass some collision storage to use function, sowwy
	if (maxHit <= 0) {
		return 0;
	}
	int objsFound = 0;
	while (meshObject != NULL) {
		if (meshObject->meshCol != NULL && meshObject->solid && ((1 << (meshObject->layer - 1)) & layerMask) != 0) {
			// apply blockmap
			Vec3 rotatedPosition;
			Quaternion inverseObjectRotation;
			QuaternionInverse(&meshObject->rotation, &inverseObjectRotation);
			QuatTimesVec3(&inverseObjectRotation, sphere->position, &rotatedPosition);
			// adjust the position to inverse the transformation of the mesh object
			rotatedPosition.x -= meshObject->position.x;
			rotatedPosition.y -= meshObject->position.y;
			rotatedPosition.z -= meshObject->position.z;
			// and scale
			rotatedPosition.x = divf32(rotatedPosition.x, meshObject->scale.x);
			rotatedPosition.y = divf32(rotatedPosition.y, meshObject->scale.y);
			rotatedPosition.z = divf32(rotatedPosition.z, meshObject->scale.z);
			// radius...
			int newRadius = divf32(sphere->radius, meshObject->scale.x);
			// are we INSIDE the AABB?
			if (f32abs(rotatedPosition.x - meshObject->meshCol->AABBPosition.x) > meshObject->meshCol->AABBBounds.x + newRadius ||
			f32abs(rotatedPosition.y - meshObject->meshCol->AABBPosition.y) > meshObject->meshCol->AABBBounds.y + newRadius ||
			f32abs(rotatedPosition.z - meshObject->meshCol->AABBPosition.z) > meshObject->meshCol->AABBBounds.z + newRadius) {
				meshObject = meshObject->next;
				continue;
			}
			CollisionSphere newSphere;
			newSphere.position = &rotatedPosition;
			newSphere.radius = newRadius;
			int totalTris = 0;
			Vec3 min;
			Vec3 max;
			min.x = newSphere.position->x - newSphere.radius;
			min.y = newSphere.position->y - newSphere.radius;
			min.z = newSphere.position->z - newSphere.radius;
			max.x = newSphere.position->x + newSphere.radius;
			max.y = newSphere.position->y + newSphere.radius;
			max.z = newSphere.position->z + newSphere.radius;
			unsigned short* trisToCollideWith = FindTrianglesFromOctree(&min, &max, meshObject->meshCol, &totalTris);
			// now check them once more for whether they're on the plane
			for (int i = 0; i < totalTris; ++i) {
				bool onPlane;
				if (SphereOnTrianglePlane(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfos[objsFound].normal, &hitInfos[objsFound].penetration, &onPlane)) {
					hitInfos[objsFound].hitTri = trisToCollideWith[i];
					hitInfos[objsFound].hitObject = meshObject;
					// scale pen by object scale
					hitInfos[objsFound].penetration = mulf32(hitInfos[objsFound].penetration, meshObject->scale.x);
					hitInfos[objsFound].colliderType = COLLIDER_MESH;
					++objsFound;
					if (objsFound >= maxHit) return objsFound;
					trisToCollideWith[i] = -1;
				}
				if (!onPlane) {
					trisToCollideWith[i] = -1;
				}
			}
			// lines...
			for (int i = 0; i < totalTris; ++i) {
				if (trisToCollideWith[i] != -1 && SphereOnTriangleLine(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfos[objsFound].normal, &hitInfos[objsFound].penetration)) {
					hitInfos[objsFound].hitTri = trisToCollideWith[i];
					hitInfos[objsFound].hitObject = meshObject;
					// scale pen by object scale
					hitInfos[objsFound].penetration = mulf32(hitInfos[objsFound].penetration, meshObject->scale.x);
					hitInfos[objsFound].colliderType = COLLIDER_MESH;
					++objsFound;
					if (objsFound >= maxHit) return objsFound;
					trisToCollideWith[i] = -1;
				}
			}
			// verts
			for (int i = 0; i < totalTris; ++i) {
				if (trisToCollideWith[i] != -1 && SphereOnTriangleVertex(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfos[objsFound].normal, &hitInfos[objsFound].penetration)) {
					hitInfos[objsFound].hitTri = trisToCollideWith[i];
					hitInfos[objsFound].hitObject = meshObject;
					// scale pen by object scale
					hitInfos[objsFound].penetration = mulf32(hitInfos[objsFound].penetration, meshObject->scale.x);
					hitInfos[objsFound].colliderType = COLLIDER_MESH;
					++objsFound;
					if (objsFound >= maxHit) return objsFound;
				}
			}
			free(trisToCollideWith);
		}
		else if (meshObject->sphereCol != NULL) {
			if (SphereOnSphere(sphere, meshObject->sphereCol, &hitInfos[objsFound].penetration, &hitInfos[objsFound].normal)) {
				hitInfos[objsFound].hitTri = -1;
				hitInfos[objsFound].hitObject = meshObject;
				hitInfos[objsFound].colliderType = COLLIDER_SPHERE;
				f32 positionValue = sphere->radius - hitInfos[objsFound].penetration;
				hitInfos[objsFound].hitPos.x = sphere->position->x + mulf32(positionValue, hitInfos[objsFound].normal.x);
				hitInfos[objsFound].hitPos.y = sphere->position->y + mulf32(positionValue, hitInfos[objsFound].normal.y);
				hitInfos[objsFound].hitPos.z = sphere->position->z + mulf32(positionValue, hitInfos[objsFound].normal.z);
				++objsFound;
				if (objsFound >= maxHit) return objsFound;
			}
		}
		else if (meshObject->boxCol != NULL) {
			if (SphereOnOBB(sphere, meshObject->boxCol, &hitInfos[objsFound].hitPos, &hitInfos[objsFound].normal, &hitInfos[objsFound].penetration)) {
				hitInfos[objsFound].hitTri = -1;
				hitInfos[objsFound].hitObject = meshObject;
				hitInfos[objsFound].colliderType = COLLIDER_BOX;
				++objsFound;
				if (objsFound >= maxHit) return objsFound;
			}
		}
		meshObject = meshObject->next;
	}
	return objsFound;
}

void MoveObjectOut(f32 penetration, Vec3 *normal, CollisionSphere *sphere, Object *meshObject, CollisionSphere *newSphere) {
	// adjust the normal once more
	Vec3 rotNormal;
	QuatTimesVec3(&meshObject->rotation, normal, &rotNormal);
	// adjust our local space
	normal->x = mulf32(normal->x, penetration);
	normal->y = mulf32(normal->y, penetration);
	normal->z = mulf32(normal->z, penetration);
	Vec3Addition(newSphere->position, normal, newSphere->position);
	penetration = mulf32(penetration, meshObject->scale.x);
	rotNormal.x = mulf32(penetration, rotNormal.x);
	rotNormal.y = mulf32(penetration, rotNormal.y);
	rotNormal.z = mulf32(penetration, rotNormal.z);
	Vec3Addition(sphere->position, &rotNormal, sphere->position);
}

void SphereObjOnMeshObj(CollisionSphere *sphere, Object *meshObject, Object *sphereObject) {
	// apply blockmap
	Vec3 rotatedPosition;
	Vec3 tmpPos;
	Quaternion inverseObjectRotation;
	// radius...
	f32 newRadius = divf32(sphere->radius, meshObject->scale.x);
	// the quaternion operations here are actually rather expensive, so let's early out extra early. just be crazy lenient.
	// potential solution: bounding sphere instead of bounding box?
	f32 AABBBounds = Max(meshObject->meshCol->AABBBounds.x, Max(meshObject->meshCol->AABBBounds.y, meshObject->meshCol->AABBBounds.z));
	f32 AABBScaling = Max(meshObject->scale.x, Max(meshObject->scale.y, meshObject->scale.z));
	AABBBounds = mulf32(mulf32(AABBBounds, AABBScaling), 6144) + newRadius;
	if (f32abs(sphere->position->x - (meshObject->meshCol->AABBPosition.x + meshObject->position.x)) > AABBBounds ||
		f32abs(sphere->position->y - (meshObject->meshCol->AABBPosition.y + meshObject->position.y)) > AABBBounds ||
		f32abs(sphere->position->z - (meshObject->meshCol->AABBPosition.z + meshObject->position.z)) > AABBBounds) {
		return;
	}
	QuaternionInverse(&meshObject->rotation, &inverseObjectRotation);
	tmpPos.x = sphere->position->x - meshObject->position.x;
	tmpPos.y = sphere->position->y - meshObject->position.y;
	tmpPos.z = sphere->position->z - meshObject->position.z;
	QuatTimesVec3(&inverseObjectRotation, &tmpPos, &rotatedPosition);
	// adjust the position to inverse the transformation of the mesh object
	// and scale
	rotatedPosition.x = divf32(rotatedPosition.x, meshObject->scale.x);
	rotatedPosition.y = divf32(rotatedPosition.y, meshObject->scale.y);
	rotatedPosition.z = divf32(rotatedPosition.z, meshObject->scale.z);
	// are we INSIDE the AABB?
	if (f32abs(rotatedPosition.x - meshObject->meshCol->AABBPosition.x) > meshObject->meshCol->AABBBounds.x + newRadius ||
	f32abs(rotatedPosition.y - meshObject->meshCol->AABBPosition.y) > meshObject->meshCol->AABBBounds.y + newRadius ||
	f32abs(rotatedPosition.z - meshObject->meshCol->AABBPosition.z) > meshObject->meshCol->AABBBounds.z + newRadius) {
		return;
	}
	CollisionSphere newSphere;
	newSphere.position = &rotatedPosition;
	newSphere.radius = newRadius;
	int totalTris = 0;
	Vec3 min;
	Vec3 max;
	min.x = newSphere.position->x - newSphere.radius;
	min.y = newSphere.position->y - newSphere.radius;
	min.z = newSphere.position->z - newSphere.radius;
	max.x = newSphere.position->x + newSphere.radius;
	max.y = newSphere.position->y + newSphere.radius;
	max.z = newSphere.position->z + newSphere.radius;
	unsigned short* trisToCollideWith = FindTrianglesFromOctree(&min, &max, meshObject->meshCol, &totalTris);
	f32 penetration;
	Vec3 normal;
	CollisionHit hitInfo;
	hitInfo.colliderType = COLLIDER_MESH;
	hitInfo.hitObject = meshObject;
	// now check them once more for whether they're on the plane
	// we check them one at a time like this because otherwise you'll get caught up on lines and verts when you should
	// be on a flat surface
	for (int i = 0; i < totalTris; ++i) {
		bool onPlane;
		if (SphereOnTrianglePlane(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfo.normal, &hitInfo.penetration, &onPlane)) {
			hitInfo.hitTri = trisToCollideWith[i];
			if (collisionFuncs[sphereObject->objectType] == NULL || collisionFuncs[sphereObject->objectType](sphereObject, &hitInfo))
				MoveObjectOut(hitInfo.penetration, &hitInfo.normal, sphere, meshObject, &newSphere);
			trisToCollideWith[i] = 0xFFFF;
		}
		if (!onPlane) {
			trisToCollideWith[i] = 0xFFFF;
		}
	}
	// lines...
	for (int i = 0; i < totalTris; ++i) {
		if (trisToCollideWith[i] != 0xFFFF && SphereOnTriangleLine(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfo.normal, &hitInfo.penetration)) {
			hitInfo.hitTri = trisToCollideWith[i];
			if (collisionFuncs[sphereObject->objectType] == NULL || collisionFuncs[sphereObject->objectType](sphereObject, &hitInfo))
				MoveObjectOut(hitInfo.penetration, &hitInfo.normal, sphere, meshObject, &newSphere);
			trisToCollideWith[i] = 0xFFFF;
		}
	}
	// verts
	for (int i = 0; i < totalTris; ++i) {
		if (trisToCollideWith[i] != 0xFFFF && SphereOnTriangleVertex(&newSphere, &meshObject->meshCol->triangles[trisToCollideWith[i]], &hitInfo.normal, &hitInfo.penetration)) {
			hitInfo.hitTri = trisToCollideWith[i];
			if (collisionFuncs[sphereObject->objectType] == NULL || collisionFuncs[sphereObject->objectType](sphereObject, &hitInfo))
				MoveObjectOut(hitInfo.penetration, &hitInfo.normal, sphere, meshObject, &newSphere);
		}
	}
	free(trisToCollideWith);
}

void SphereObjOnSphereObj(Object *collider, Object* collidee) {
	f32 pen;
	Vec3 normal;
	if (SphereOnSphere(collider->sphereCol, collidee->sphereCol, &pen, &normal)) {
		CollisionHit hitInfo;
		hitInfo.colliderType = COLLIDER_SPHERE;
		hitInfo.normal = normal;
		hitInfo.penetration = pen;
		hitInfo.hitObject = collidee;
		hitInfo.hitTri = -1;
		if (collisionFuncs[collider->objectType] == NULL || collisionFuncs[collider->objectType](collider, &hitInfo)) {
			normal.x = mulf32(pen, normal.x);
			normal.y = mulf32(pen, normal.y);
			normal.z = mulf32(pen, normal.z);
			Vec3Addition(&collider->position, &normal, &collider->position);
		}
	}
}

void SphereObjOnBoxObj(Object* collider, Object* collidee) {
	// first, do AABB check
	f32 maxExtents = mulf32(Max(collidee->boxCol->extents.x, Max(collidee->boxCol->extents.y, collidee->boxCol->extents.z)), Fixed32ToNative(4096 + 2048));
	f32 extentsPlusRadius = maxExtents + collider->sphereCol->radius;

	if (f32abs(collider->sphereCol->position->x - collidee->boxCol->position->x) > extentsPlusRadius ||
		f32abs(collider->sphereCol->position->y - collidee->boxCol->position->y) > extentsPlusRadius ||
		f32abs(collider->sphereCol->position->z - collidee->boxCol->position->z) > extentsPlusRadius) {
		return;
	}

	// okay great, perform actual collision check
	CollisionHit hitInfo;
	if (SphereOnOBB(collider->sphereCol, collidee->boxCol, &hitInfo.hitPos, &hitInfo.normal, &hitInfo.penetration)) {
		hitInfo.colliderType = COLLIDER_BOX;
		hitInfo.hitObject = collidee;
		if (collisionFuncs[collider->objectType] == NULL || collisionFuncs[collider->objectType](collider, &hitInfo)) {
			Vec3 tmpNormal;
			tmpNormal.x = mulf32(hitInfo.normal.x, hitInfo.penetration);
			tmpNormal.y = mulf32(hitInfo.normal.y, hitInfo.penetration);
			tmpNormal.z = mulf32(hitInfo.normal.z, hitInfo.penetration);
			// move out
			Vec3Addition(collider->sphereCol->position, &tmpNormal, collider->sphereCol->position);
		}
	}
}

int GetObjectsOfType(int type, Object **out, int maxObjects) {
	Object *currObject = firstObject.next;
	int currIdx = 0;
	while (currObject != NULL) {
		if (currObject->objectType == type) {
			out[currIdx] = currObject;
			++currIdx;
			if (currIdx >= maxObjects) {
				return currIdx;
			}
		}
		currObject = currObject->next;
	}
	return currIdx;
}

void ProcessObjects() {
	// update networking before everything else
	if (defaultNetInstance != NULL) {
		UpdateNetworking(defaultNetInstance, deltaTime);
	}

	Object *currObject = firstObject.next;
	while (currObject != NULL) {
		// TODO; functions
		if (updateFuncs[currObject->objectType] != NULL && currObject->active) {
			updateFuncs[currObject->objectType](currObject);
		}
		// handle physics
		// two step movement
		if (currObject->moves && currObject->active) {
			currObject->dirtyTransform = true;
			Vec3 stepVelocity;
			if (deltaTimeEngine) {
				stepVelocity.x = mulf32(currObject->velocity.x / 2, deltaTime);
				stepVelocity.y = mulf32(currObject->velocity.y / 2, deltaTime);
				stepVelocity.z = mulf32(currObject->velocity.z / 2, deltaTime);

			} else {
				stepVelocity.x = currObject->velocity.x / 2;
				stepVelocity.y = currObject->velocity.y / 2;
				stepVelocity.z = currObject->velocity.z / 2;
			}
			for (int i = 0; i < 2; ++i) {
				Vec3Addition(&stepVelocity, &currObject->position, &currObject->position);
				if (currObject->sphereCol != NULL) {
					// iterate over all objects and get mesh colliders
					Object *colObject = firstObject.next;
					while (colObject != NULL) {
						if (colObject != currObject) {
							if (colObject->solid && layerCollision[currObject->layer + (colObject->layer*32)]) {
								if (colObject->meshCol != NULL) {
									SphereObjOnMeshObj(currObject->sphereCol, colObject, currObject);
								}
								if (colObject->sphereCol != NULL) {
									SphereObjOnSphereObj(currObject, colObject);
								}
								if (colObject->boxCol != NULL) {
									SphereObjOnBoxObj(currObject, colObject);
								}
							}
						}
						colObject = colObject->next;
					}
					
				}
			}
		}
		currObject = currObject->next;
	}
	//late update
	currObject = firstObject.next;
	while (currObject != NULL) {
		if (lateUpdateFuncs[currObject->objectType] != NULL && currObject->active) {
			lateUpdateFuncs[currObject->objectType](currObject);
		}
		currObject = currObject->next;
	}
	// set up the camera
	SetupCameraMatrix();
	
	
	// do rendering, now
	currObject = firstObject.next;
	while (currObject != NULL) {
		if (currObject->mesh != NULL && !currObject->culled) {
			if (currObject->dirtyTransform) {
				m4x4 workMatrix2, workMatrix3, matrixToRender;
				//MakeTranslationMatrix(currObject->position.x, currObject->position.y, currObject->position.z, &workMatrix1);
				MakeScaleMatrix(currObject->scale.x, currObject->scale.y, currObject->scale.z, &workMatrix2);
				MakeRotationMatrix(&currObject->rotation, &workMatrix3);
				//CombineMatrices(&workMatrix1, &workMatrix2, &workMatrix4);
				Combine3x3Matrices(&workMatrix2, &workMatrix3, &workMatrix3);
				// optimize generation of x y z
				workMatrix3.m[3] = currObject->position.x;
				workMatrix3.m[7] = currObject->position.y;
				workMatrix3.m[11] = currObject->position.z;
				MatrixToDSMatrix(&workMatrix3, &currObject->transform);
				currObject->dirtyTransform = false;
			}
			if (currObject->mesh->skeletonCount != 0 && currObject->animator != NULL) {
				if (deltaTimeEngine) {
					f32 tmp = currObject->animator->speed;
					currObject->animator->speed = mulf32(tmp * 60, deltaTime);
					UpdateAnimator(currObject->animator, currObject->mesh);
					currObject->animator->speed = tmp;
				} else {
					UpdateAnimator(currObject->animator, currObject->mesh);
				}
				if (AABBInCamera(&currObject->mesh->boundsMin, &currObject->mesh->boundsMax, &currObject->transform)) {
					RenderModelRigged(currObject->mesh, &currObject->transform, NULL, currObject->animator);
				}
			} else {
				if (AABBInCamera(&currObject->mesh->boundsMin, &currObject->mesh->boundsMax, &currObject->transform)) {
					RenderModel(currObject->mesh, &currObject->transform, NULL);
				}
			}
		}
		Object *tmpObj = currObject;
		currObject = currObject->next;
		// also destroy object if relevant
		if (tmpObj->destroy) {
			DestroyObjectInternal(tmpObj);
		}
	}

	// finally, render transparent models
#ifdef _NOTDS
	RenderTransparentModels();
#endif
}

int AddObjectType(void(*update)(Object*), void(*start)(Object*), bool(*collision)(Object*, CollisionHit*), void(*lateUpdate)(Object*), void(*destroy)(Object*), void(*networkCreateSend)(Object*, void**, int*),
	void (*networkCreateReceive)(Object*, void*, int), void(*networkPacketReceive)(Object*, void**, int, int, int), bool networked) {
	updateFuncs[currFunc] = update;
	startFuncs[currFunc] = start;
	collisionFuncs[currFunc] = collision;
	lateUpdateFuncs[currFunc] = lateUpdate;
	destroyFuncs[currFunc] = destroy;
	networkCreateFuncs[currFunc] = networkCreateSend;
	networkCreateReceiveFuncs[currFunc] = networkCreateReceive;
	networkPacketReceiveFuncs[currFunc] = (void (*)(Object *, void *, int,  int,  int))networkPacketReceive;
	networkedObjectTypes[currFunc] = networked;

	int retValue = currFunc;
	++currFunc;
	return retValue;
}

Object *CreateObject(int type, Vec3 *position, bool forced) {
	if (!(forced || defaultNetInstance == NULL || (!defaultNetInstance->active || defaultNetInstance->host)) && networkedObjectTypes[type]) {
		return NULL;
	}
	Object *newObj = calloc(sizeof(Object), 1);
	newObj->position.x = position->x;
	newObj->position.y = position->y;
	newObj->position.z = position->z;
	newObj->scale.x = Fixed32ToNative(4096);
	newObj->scale.y = Fixed32ToNative(4096);
	newObj->scale.z = Fixed32ToNative(4096);
	newObj->objectType = type;
	newObj->rotation.w = Fixed32ToNative(4096);
	if (firstObject.next != NULL) {
		firstObject.next->previous = newObj;
	}
	newObj->next = firstObject.next;
	firstObject.next = newObj;
	newObj->previous = &firstObject;
	newObj->layer = 1;
	if (startFuncs[type] != NULL) {
		startFuncs[type](newObj);
	}
	newObj->dirtyTransform = true;
	newObj->references.object = newObj;
	newObj->active = true;
	if (!defaultNetInstance == NULL && networkedObjectTypes[type] && defaultNetInstance->host && defaultNetInstance->active) {
		// find first unused networked object slot
		int objSlot = 0;
		while (objSlot < networkedObjectsCount) {
			if (networkedObjects[objSlot].object == NULL) {
				break;
			}
			++objSlot;
		}
		if (objSlot == networkedObjectsCount) {
			if (networkedObjectsCount == 0) {
				// 64 initial networked objects available
				networkedObjectsCount = 32;
			}
			networkedObjects = realloc(networkedObjects, sizeof(ObjectPtr) * networkedObjectsCount * 2);
			networkedObjectsCount *= 2;
			for (int i = objSlot; i < networkedObjectsCount; ++i) {
				networkedObjects[i].object = NULL;
			}
		}
		newObj->netId = objSlot;
		GetObjectPtr(newObj, &networkedObjects[objSlot]);
		void* syncData = NULL;
		int syncDataLen = 0;
		if (networkCreateFuncs[type] != NULL) {
			networkCreateFuncs[type](newObj, &syncData, &syncDataLen);
		}
		char* objectCreateData;
		// 0x0: object id
		// 0x4: object type
		objectCreateData = (char*)malloc(syncDataLen + sizeof(int) + sizeof(int));
		if (syncDataLen != 0) {
			memcpy(&objectCreateData[8], syncData, syncDataLen);
			free(syncData);
		}
		((int*)objectCreateData)[0] = objSlot;
		((int*)objectCreateData)[1] = type;
		PacketSendAll(objectCreateData, syncDataLen + sizeof(int) + sizeof(int), objCreateId, true, defaultNetInstance);
		free(objectCreateData);
	}
	else {
		newObj->netId = -1;
	}
	return newObj;
}

void DestroyObject(Object *object) {
	object->destroy = true;
}

void AddCollisionBetweenLayers(int layer1, int layer2) {
	layerCollision[(layer1*32)+layer2] = true;
	layerCollision[layer1+(layer2*32)] = true;
}

void GetObjectPtr(Object* object, ObjectPtr* out) {
	out->next = object->references.next;
	out->prev = &object->references;
	if (out->next != NULL) {
		out->next->prev = out;
	}
	object->references.next = out;
	out->object = object;
}

void FreeObjectPtr(ObjectPtr* ptr) {
	// it's already been freed...
	if (ptr->object == NULL) {
		return;
	}
	ptr->prev->next = ptr->next;
	if (ptr->next != NULL) {
		ptr->next->prev = ptr->prev;
	}
	ptr->object = NULL;
}

void SyncObjectPacketSend(Object* object, void* data, int dataLen, unsigned short type, bool important) {
	if (object->netId == -1 || !defaultNetInstance->active) return;
	char* ou = (char*)malloc(dataLen + sizeof(int) * 3);
	((int*)ou)[0] = object->netId;
	((int*)ou)[1] = defaultNetInstance->clientNode;
	((unsigned short*)ou)[4] = type;
	((bool*)ou)[10] = important;
	if (data != NULL && dataLen != 0) {
		memcpy(&((char*)ou)[12], data, dataLen);
	}
	PacketSendAll(ou, dataLen + sizeof(int) * 3, objPacketId, important, defaultNetInstance);
}

void SyncObjectPacketReceive(void* data, int dataLen, int node, NetworkInstance* instance) {
	if (!instance->host) {
		node = ((int*)data)[1];
	}
	// don't process packets we send ourselves
	if (node == instance->clientNode) {
		return;
	}
	int netId = ((int*)data)[0];
	// sanity check
	if (netId < 0 || netId > networkedObjectsCount || networkedObjects[netId].object == NULL) {
		return;
	}
	Object* obj = networkedObjects[netId].object;
	if (obj == NULL) {
		return;
	}
	if (networkPacketReceiveFuncs[obj->objectType] != NULL) {
		networkPacketReceiveFuncs[obj->objectType](obj, (void*)&((char*)data)[12], dataLen - 12, node, ((unsigned short*)data)[4]);
	}
	// relay packet if we're host
	if (instance->host) {
		PacketSendAll(data, dataLen, objPacketId, ((bool*)data)[10], instance);
	}
}

void SyncObjectCreateReceive(void* data, int dataLen, int node, NetworkInstance* instance) {
	// host should authenticate all object creations!
	if (instance->host) {
		return;
	}
	int netId = ((int*)data)[0];
	int objId = ((int*)data)[1];
	Vec3 zeroVec = { 0, 0, 0 };
	Object* newObj = CreateObject(objId, &zeroVec, true);
	// overwrite old object if it exists, allocate memory, etc
	while (netId >= networkedObjectsCount) {
		int prevObjCount = networkedObjectsCount;
		if (networkedObjectsCount == 0) {
			networkedObjectsCount = 32;
		}
		networkedObjects = realloc(networkedObjects, sizeof(ObjectPtr) * networkedObjectsCount * 2);
		networkedObjectsCount *= 2;
		for (int i = prevObjCount; i < networkedObjectsCount; ++i) {
			networkedObjects[i].object = NULL;
		}
	}
	if (networkedObjects[netId].object != NULL) {
		networkedObjects[netId].object->netId = -1;
		FreeObjectPtr(&networkedObjects[netId]);
	}
	GetObjectPtr(newObj, &networkedObjects[netId]);
	if (networkCreateReceiveFuncs[objId] != NULL) {
		networkCreateReceiveFuncs[objId](newObj, (void*)&((char*)data)[8], dataLen - 8);
	}
}

void SyncObjectStopSyncReceive(void* data, int dataLen, int node, NetworkInstance* instance) {
	// host authentication!
	if (instance->host) {
		return;
	}
	int netId = ((int*)data)[0];
	// sanity check
	if (netId < 0 || netId >= networkedObjectsCount) {
		return;
	}
	FreeObjectPtr(&networkedObjects[netId]);
}

void StopSyncingObject(Object* obj) {
	if (obj->netId < 0 || obj->netId >= networkedObjectsCount) {
		return;
	}
	// sync that we should stop syncing this
	if (defaultNetInstance->host) {
		PacketSendAll((char*)&obj->netId, 4, objStopSyncId, true, defaultNetInstance);
	}
	FreeObjectPtr(&networkedObjects[obj->netId]);
	obj->netId = -1;
}

void SyncAllObjects(int node) {
	if (!defaultNetInstance->host) {
		return;
	}
	for (int i = 0; i < networkedObjectsCount; ++i) {
		if (networkedObjects[i].object != NULL) {
			void* data = NULL;
			int dataLen = 0;
			if (networkCreateFuncs[networkedObjects[i].object->objectType] != NULL) {
				networkCreateFuncs[networkedObjects[i].object->objectType](networkedObjects[i].object, &data, &dataLen);
			}
			char* ou = malloc(dataLen + sizeof(int) * 2);
			// 0x0: object id
			// 0x4: object type
			((int*)ou)[0] = i;
			((int*)ou)[1] = networkedObjects[i].object->objectType;
			if (dataLen != 0) {
				memcpy(&ou[8], data, dataLen);
			}
			PacketSendTo(ou, dataLen + sizeof(int) * 2, objCreateId, true, node, defaultNetInstance);
		}
	}
}

void DestroyObjectImmediate(Object* object) {
	DestroyObjectInternal(object);
}