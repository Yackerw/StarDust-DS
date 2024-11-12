#ifndef SDMATH
#define SDMATH
#include <nds.h>

typedef int f32;

typedef struct {
	f32 x;
	f32 y;
	f32 z;
	f32 w;
} Quaternion;

typedef struct {
	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
			f32 w;
		};
		f32 coords[4];
	};
} Vec4;

typedef struct {
	union {
		struct {
			float x;
			float y;
			float z;
			float w;
		};
		float coords[4];
	};
} Vec4f;

typedef struct {
	union {
		struct {
			short x;
			short y;
			short z;
			short w;
		};
		short coords[4];
	};
} Vec4s;

typedef struct {
	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
		};
		f32 coords[3];
	};
} Vec3;

typedef struct {
	union {
		struct {
			short x;
			short y;
			short z;
		};
		short coords[3];
	};
} Vec3s;

typedef struct {
	union {
		struct {
			float x;
			float y;
			float z;
		};
		float coords[3];
	};
} Vec3f;

typedef struct {
	union {
		struct {
			float x;
			float y;
		};
		float coords[2];
	};
} Vec2f;

typedef struct {
	union {
		struct {
			int x;
			int y;
			int z;
		};
		int coords[3];
	};
} Vec3i;

typedef struct {
	f32 x;
	f32 y;
} Vec2;

typedef struct {
	Vec3 points[8];
	Vec4 planes[6];
} ViewFrustum;

#define PI 12868
#define FixedDegreesToRotation 91
#define FixedRadiansToRotation 5215
#define RotationToFixedRadians 3217

void MakeTranslationMatrix(f32 x, f32 y, f32 z, m4x4 *retValue);

void MakeScaleMatrix(f32 x, f32 y, f32 z, m4x4 *retValue);

void Combine3x3Matrices(m4x4 *left, m4x4 *right, m4x4 *retValue);

void CombineMatrices(m4x4 *left, m4x4 *right, m4x4 *retValue);

void CombineMatricesFull(m4x4* left, m4x4* right, m4x4* retValue);

void MakeRotationMatrix(Quaternion *input, m4x4 *retValue);

void TransposeMatrix(m4x4* input, m4x4* output);

void MatrixToDSMatrix(m4x4 *input, m4x4 *output);

void EulerToQuat(f32 x, f32 y, f32 z, Quaternion *q);

void QuatTimesQuat(Quaternion *left, Quaternion *right, Quaternion *out);

void QuatNormalize(Quaternion *input);

void QuatSlerp(Quaternion *left, Quaternion *right, Quaternion *out, f32 t);

inline f32 DotProduct(Vec3 *left, Vec3 *right);

inline void CrossProduct(Vec3 *left, Vec3 *right, Vec3 *out);

void QuatTimesVec3(Quaternion *quat, Vec3 *vec, Vec3 *out);

void QuaternionInverse(Quaternion *quat, Quaternion *out);

void QuaternionFromAngleAxis(f32 angle, Vec3 *axis, Quaternion *out);

void VectorFromToRotation(Vec3 *v1, Vec3 *v2, Quaternion *out);

inline f32 Magnitude(Vec3 *vec);

inline f32 SqrMagnitude(Vec3 *vec);

inline void Normalize(Vec3 *vec, Vec3 *out);

inline void Vec3Addition(Vec3 *left, Vec3 *right, Vec3 *out);

inline void Vec3Subtraction(Vec3 *left, Vec3 *right, Vec3 *out);

inline void Vec3Multiplication(Vec3 *left, Vec3 *right, Vec3 *out);

inline void Vec3Division(Vec3 *left, Vec3 *right, Vec3 *out);

inline f32 Lerp(f32 left, f32 right, f32 t);

f32 Atan2(f32 y, f32 x);

void Reflect(Vec3 *a, Vec3 *b, Vec3 *out);

inline f32 Clamp(f32 value, f32 min, f32 max);

inline int iClamp(int value, int min, int max);

inline f32 Max(f32 value, f32 max);

inline f32 Min(f32 value, f32 min);

inline f32 DeltaAngle(f32 dir1, f32 dir2);

f32 Pow(f32 value, f32 toPow);

void NormalFromVerts(Vec3s *vert1, Vec3s *vert2, Vec3s *vert3, Vec3s *out);

void NormalFromVertsFloat(Vec3s* vert1, Vec3s* vert2, Vec3s* vert3, Vec3s* out);

void MakePerspectiveMatrix(f32 fov, f32 aspect, f32 near, f32 far, m4x4* ret);

inline f32 f32Mod(f32 left, f32 right);

inline void MatrixTimesVec3(m4x4* left, Vec3* right, Vec3* ret);

inline f32 f32abs(f32 input);

void GenerateViewFrustum(m4x4* matrix, ViewFrustum* frustumOut);

void QuaternionToEuler(Quaternion* quat, Vec3* euler);

f32 f32rand(f32 min, f32 max);

long long Int64Div(int left, int right);

#endif