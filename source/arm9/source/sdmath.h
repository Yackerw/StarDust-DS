#ifndef SDMATH
#define SDMATH
#include <nds.h>

#ifndef _NOTDS
typedef int f32;
#else
typedef float f32;
#endif

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

#ifndef _NOTDS
#define FixedDegreesToRotation 91
#define FixedRadiansToRotation 5215
#define RotationToFixedRadians 3217
#define Fixed32ToNative(x) (x)
#define Angle16ToNative(x) (x)
#define NativeToFixed32(x) (x)
#else
#define PI 3.14159265359f
#define FixedDegreesToRotation 0.0174533f
#define FixedRadiansToRotation 1.0f
#define RotationToFixedRadians 1.0f
#define Fixed32ToNative(x) (f32)((x) / 4096.0f)
#define Angle16ToNative(x) (f32)((x) / 32767.0f * PI * 2.0f)
#define NativeToFixed32(x) (int)(4096.0f * (x))
#endif

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

f32 DotProduct(Vec3 *left, Vec3 *right);

void CrossProduct(Vec3 *left, Vec3 *right, Vec3 *out);

void QuatTimesVec3(Quaternion *quat, Vec3 *vec, Vec3 *out);

void QuaternionInverse(Quaternion *quat, Quaternion *out);

void QuaternionFromAngleAxis(f32 angle, Vec3 *axis, Quaternion *out);

void QuaternionToEuler(Quaternion *quat, Vec3 *out);

void VectorFromToRotation(Vec3 *v1, Vec3 *v2, Quaternion *out);

f32 Magnitude(Vec3 *vec);

f32 SqrMagnitude(Vec3 *vec);

void Normalize(Vec3 *vec, Vec3 *out);

void Vec3Addition(Vec3 *left, Vec3 *right, Vec3 *out);

void Vec3Subtraction(Vec3 *left, Vec3 *right, Vec3 *out);

void Vec3Multiplication(Vec3 *left, Vec3 *right, Vec3 *out);

void Vec3Division(Vec3 *left, Vec3 *right, Vec3 *out);

f32 Lerp(f32 left, f32 right, f32 t);

f32 Atan2(f32 y, f32 x);

void Reflect(Vec3 *a, Vec3 *b, Vec3 *out);

f32 Clamp(f32 value, f32 min, f32 max);

int iClamp(int value, int min, int max);

f32 Max(f32 value, f32 max);

f32 Min(f32 value, f32 min);

f32 DeltaAngle(f32 dir1, f32 dir2);

f32 Pow(f32 value, f32 toPow);

void NormalFromVerts(Vec3 *vert1, Vec3 *vert2, Vec3 *vert3, Vec3 *out);

void NormalFromVertsFloat(Vec3* vert1, Vec3* vert2, Vec3* vert3, Vec3* out);

void MakePerspectiveMatrix(f32 fov, f32 aspect, f32 near, f32 far, m4x4* ret);

f32 f32Mod(f32 left, f32 right);

void MatrixTimesVec3(m4x4* left, Vec3* right, Vec3* ret);

f32 f32abs(f32 input);

void GenerateViewFrustum(m4x4* matrix, ViewFrustum* frustumOut);

#ifndef _NOTDS
long long Int64Div(int left, int right);
#endif

#endif