#include <math.h>
#include <nds.h>
#include <stdio.h>
#include "sdmath.h"
#include <string.h>

// used internally for one function lmao
typedef struct {
	float x;
	float y;
	float z;
} Vec3f;

#define r1x 0
#define r1y 1
#define r1z 2
#define r1w 3
#define r2x 4
#define r2y 5
#define r2z 6
#define r2w 7
#define r3x 8
#define r3y 9
#define r3z 10
#define r3w 11
#define r4x 12
#define r4y 13
#define r4z 14
#define r4w 15

//#define FixedToRadians 10430
#define FixedToRadians 5215

#define ZeroMatrix(matrix) for(int i = 0; i < 16; ++i) {matrix->m[i] = 0;}

void MakeTranslationMatrix(f32 x, f32 y, f32 z, m4x4 *retValue) {
	ZeroMatrix(retValue);
	retValue->m[r1x] = Fixed32ToNative(4096);
	retValue->m[r2y] = Fixed32ToNative(4096);
	retValue->m[r3z] = Fixed32ToNative(4096);
	retValue->m[r4w] = Fixed32ToNative(4096);
	retValue->m[r1w] = x;
	retValue->m[r2w] = y;
	retValue->m[r3w] = z;
}

void MakeScaleMatrix(f32 x, f32 y, f32 z, m4x4 *retValue) {
	ZeroMatrix(retValue);
	retValue->m[r1x] = x;
	retValue->m[r2y] = y;
	retValue->m[r3z] = z;
	retValue->m[r4w] = Fixed32ToNative(4096);
}

void Combine3x3Matrices(m4x4 *left, m4x4 *right, m4x4 *retValue) {
	retValue->m[0] = mulf32(left->m[0], right->m[0]) + mulf32(left->m[1], right->m[4]) + mulf32(left->m[2], right->m[8]);// + mulf32(left->m[3], right->m[12]);
    retValue->m[1] = mulf32(left->m[0], right->m[1]) + mulf32(left->m[1], right->m[5]) + mulf32(left->m[2], right->m[9]);// + mulf32(left->m[3], right->m[13]);
    retValue->m[2] = mulf32(left->m[0], right->m[2]) + mulf32(left->m[1], right->m[6]) + mulf32(left->m[2], right->m[10]);// + mulf32(left->m[3], right->m[14]);
    retValue->m[3] = 0;

    retValue->m[4] = mulf32(left->m[4], right->m[0]) + mulf32(left->m[5], right->m[4]) + mulf32(left->m[6], right->m[8]);// + left->m[7] * right->m[12];
    retValue->m[5] = mulf32(left->m[4], right->m[1]) + mulf32(left->m[5], right->m[5]) + mulf32(left->m[6], right->m[9]);// + left->m[7] * right->m[13];
    retValue->m[6] = mulf32(left->m[4], right->m[2]) + mulf32(left->m[5], right->m[6]) + mulf32(left->m[6], right->m[10]);// + left->m[7] * right->m[14];
    retValue->m[7] = 0;

    retValue->m[8] = mulf32(left->m[8], right->m[0]) + mulf32(left->m[9], right->m[4]) + mulf32(left->m[10], right->m[8]);// + left->m[11] * right->m[12];
    retValue->m[9] = mulf32(left->m[8], right->m[1]) + mulf32(left->m[9], right->m[5]) + mulf32(left->m[10], right->m[9]);// + left->m[11] * right->m[13];
    retValue->m[10] = mulf32(left->m[8], right->m[2]) + mulf32(left->m[9], right->m[6]) + mulf32(left->m[10], right->m[10]);// + left->m[11] * right->m[14];
    retValue->m[11] = 0;

	retValue->m[12] = 0;
	retValue->m[13] = 0;
	retValue->m[14] = 0;
	retValue->m[15] = Fixed32ToNative(4096);
}

// this function is HYPER optimized, and excludes row 4. consider it a 4x3 matrix instead
void CombineMatrices(m4x4 *left, m4x4 *right, m4x4 *retValue) {
	// i think this should work?
	/*for (f32 i = 0; i < 16; i += 4) {
		for (f32 i2 = 0; i2 < 4; ++i2) {
			retValue->m[i+i2] = 0;
			for (f32 i3 = 0; i3 < 4; ++i3) {
				retValue->m[i+i2] += mulf32(left->m[i+i3], right->m[i2 + (i3 * 4)]);
			}
		}
	}*/
    retValue->m[0] = mulf32(left->m[0], right->m[0]) + mulf32(left->m[1], right->m[4]) + mulf32(left->m[2], right->m[8]);// + mulf32(left->m[3], right->m[12]);
    retValue->m[1] = mulf32(left->m[0], right->m[1]) + mulf32(left->m[1], right->m[5]) + mulf32(left->m[2], right->m[9]);// + mulf32(left->m[3], right->m[13]);
    retValue->m[2] = mulf32(left->m[0], right->m[2]) + mulf32(left->m[1], right->m[6]) + mulf32(left->m[2], right->m[10]);// + mulf32(left->m[3], right->m[14]);
    retValue->m[3] = mulf32(left->m[0], right->m[3]) + mulf32(left->m[1], right->m[7]) + mulf32(left->m[2], right->m[11]) + left->m[3];// * right->m[15];

    retValue->m[4] = mulf32(left->m[4], right->m[0]) + mulf32(left->m[5], right->m[4]) + mulf32(left->m[6], right->m[8]);// + left->m[7] * right->m[12];
    retValue->m[5] = mulf32(left->m[4], right->m[1]) + mulf32(left->m[5], right->m[5]) + mulf32(left->m[6], right->m[9]);// + left->m[7] * right->m[13];
    retValue->m[6] = mulf32(left->m[4], right->m[2]) + mulf32(left->m[5], right->m[6]) + mulf32(left->m[6], right->m[10]);// + left->m[7] * right->m[14];
    retValue->m[7] = mulf32(left->m[4], right->m[3]) + mulf32(left->m[5], right->m[7]) + mulf32(left->m[6], right->m[11]) + left->m[7];// * right->m[15];

    retValue->m[8] = mulf32(left->m[8], right->m[0]) + mulf32(left->m[9], right->m[4]) + mulf32(left->m[10], right->m[8]);// + left->m[11] * right->m[12];
    retValue->m[9] = mulf32(left->m[8], right->m[1]) + mulf32(left->m[9], right->m[5]) + mulf32(left->m[10], right->m[9]);// + left->m[11] * right->m[13];
    retValue->m[10] = mulf32(left->m[8], right->m[2]) + mulf32(left->m[9], right->m[6]) + mulf32(left->m[10], right->m[10]);// + left->m[11] * right->m[14];
    retValue->m[11] = mulf32(left->m[8], right->m[3]) + mulf32(left->m[9], right->m[7]) + mulf32(left->m[10], right->m[11]) + left->m[11];// * right->m[15];

    //retValue->m[12] = left->m[12] * right->m[0] + left->m[13] * right->m[4] + left->m[14] * right->m[8] + left->m[15] * right->m[12];
    //retValue->m[13] = left->m[12] * right->m[1] + left->m[13] * right->m[5] + left->m[14] * right->m[9] + left->m[15] * right->m[13];
    //retValue->m[14] = left->m[12] * right->m[2] + left->m[13] * right->m[6] + left->m[14] * right->m[10] + left->m[15] * right->m[14];
    //retValue->m[15] = left->m[12] * right->m[3] + left->m[13] * right->m[7] + left->m[14] * right->m[11] + left->m[15] * right->m[15];
	retValue->m[12] = 0;
	retValue->m[13] = 0;
	retValue->m[14] = 0;
	retValue->m[15] = Fixed32ToNative(4096);
}

void CombineMatricesFull(m4x4* left, m4x4* right, m4x4* retValue) {
	retValue->m[0] = mulf32(left->m[0], right->m[0]) + mulf32(left->m[1], right->m[4]) + mulf32(left->m[2], right->m[8]) + mulf32(left->m[3], right->m[12]);
	retValue->m[1] = mulf32(left->m[0], right->m[1]) + mulf32(left->m[1], right->m[5]) + mulf32(left->m[2], right->m[9]) + mulf32(left->m[3], right->m[13]);
	retValue->m[2] = mulf32(left->m[0], right->m[2]) + mulf32(left->m[1], right->m[6]) + mulf32(left->m[2], right->m[10]) + mulf32(left->m[3], right->m[14]);
	retValue->m[3] = mulf32(left->m[0], right->m[3]) + mulf32(left->m[1], right->m[7]) + mulf32(left->m[2], right->m[11]) + mulf32(left->m[3], right->m[15]);

	retValue->m[4] = mulf32(left->m[4], right->m[0]) + mulf32(left->m[5], right->m[4]) + mulf32(left->m[6], right->m[8]) + mulf32(left->m[7], right->m[12]);
	retValue->m[5] = mulf32(left->m[4], right->m[1]) + mulf32(left->m[5], right->m[5]) + mulf32(left->m[6], right->m[9]) + mulf32(left->m[7], right->m[13]);
	retValue->m[6] = mulf32(left->m[4], right->m[2]) + mulf32(left->m[5], right->m[6]) + mulf32(left->m[6], right->m[10]) + mulf32(left->m[7], right->m[14]);
	retValue->m[7] = mulf32(left->m[4], right->m[3]) + mulf32(left->m[5], right->m[7]) + mulf32(left->m[6], right->m[11]) + mulf32(left->m[7], right->m[15]);

	retValue->m[8] = mulf32(left->m[8], right->m[0]) + mulf32(left->m[9], right->m[4]) + mulf32(left->m[10], right->m[8]) + mulf32(left->m[11], right->m[12]);
	retValue->m[9] = mulf32(left->m[8], right->m[1]) + mulf32(left->m[9], right->m[5]) + mulf32(left->m[10], right->m[9]) + mulf32(left->m[11], right->m[13]);
	retValue->m[10] = mulf32(left->m[8], right->m[2]) + mulf32(left->m[9], right->m[6]) + mulf32(left->m[10], right->m[10]) + mulf32(left->m[11], right->m[14]);
	retValue->m[11] = mulf32(left->m[8], right->m[3]) + mulf32(left->m[9], right->m[7]) + mulf32(left->m[10], right->m[11]) + mulf32(left->m[11], right->m[15]);

	retValue->m[12] = mulf32(left->m[12], right->m[0]) + mulf32(left->m[13], right->m[4]) + mulf32(left->m[14], right->m[8]) + mulf32(left->m[15], right->m[12]);
	retValue->m[13] = mulf32(left->m[12], right->m[1]) + mulf32(left->m[13], right->m[5]) + mulf32(left->m[14], right->m[9]) + mulf32(left->m[15], right->m[13]);
	retValue->m[14] = mulf32(left->m[12], right->m[2]) + mulf32(left->m[13], right->m[6]) + mulf32(left->m[14], right->m[10]) + mulf32(left->m[15], right->m[14]);
	retValue->m[15] = mulf32(left->m[12], right->m[3]) + mulf32(left->m[13], right->m[7]) + mulf32(left->m[14], right->m[11]) + mulf32(left->m[15], right->m[15]);
}

void TransposeMatrix(m4x4* input, m4x4* output) {
	output->m[r1x] = input->m[r1x];
	output->m[r2x] = input->m[r1y];
	output->m[r3x] = input->m[r1z];
	output->m[r4x] = input->m[r1w];
	output->m[r1y] = input->m[r2x];
	output->m[r2y] = input->m[r2y];
	output->m[r3y] = input->m[r2z];
	output->m[r4y] = input->m[r2w];
	output->m[r1z] = input->m[r3x];
	output->m[r2z] = input->m[r3y];
	output->m[r3z] = input->m[r3z];
	output->m[r4z] = input->m[r3w];
	output->m[r1w] = input->m[r4x];
	output->m[r2w] = input->m[r4y];
	output->m[r3w] = input->m[r4z];
	output->m[r4w] = input->m[r4w];
}


void MatrixToDSMatrix(m4x4 *input, m4x4 *output) {
#ifndef _NOTDS
	TransposeMatrix(input, output);
#else
	memcpy(output, input, sizeof(m4x4));
#endif
}

void MakeRotationMatrix(Quaternion *input, m4x4 *retValue) {
	f32 ww = mulf32(input->w, input->w);
	f32 xy = mulf32(input->x, input->y);
	f32 yz = mulf32(input->y, input->z);
	f32 xz = mulf32(input->x, input->z);
	f32 wx = mulf32(input->w, input->x);
	f32 wy = mulf32(input->w, input->y);
	f32 wz = mulf32(input->w, input->z);
	// oh god i have no idea what any of this means
	retValue->m[r1x] = 2 * (ww + mulf32(input->x, input->x)) - Fixed32ToNative(4096);
	retValue->m[r1y] = 2 * (xy - wz);
	retValue->m[r1z] = 2 * (xz + wy);
	
	retValue->m[r2x] = 2 * (xy + wz);
	retValue->m[r2y] = 2 * (ww + mulf32(input->y, input->y)) - Fixed32ToNative(4096);
	retValue->m[r2z] = 2 * (yz - wx);
	
	retValue->m[r3x] = 2 * (xz - wy);
	retValue->m[r3y] = 2 * (yz + wx);
	retValue->m[r3z] = 2 * (ww + mulf32(input->z, input->z)) - Fixed32ToNative(4096);
	
	retValue->m[r1w] = 0;
	retValue->m[r2w] = 0;
	retValue->m[r3w] = 0;
	retValue->m[r4x] = 0;
	retValue->m[r4y] = 0;
	retValue->m[r4z] = 0;
	retValue->m[r4w] = Fixed32ToNative(4096);
}

void EulerToQuat(f32 x, f32 y, f32 z, Quaternion *q) {
	// what in the god damn
	f32 cy = cosLerp(z / 2);
	f32 sy = sinLerp(z / 2);
	f32 cp = cosLerp(y / 2);
	f32 sp = sinLerp(y / 2);
	f32 cr = cosLerp(x / 2);
	f32 sr = sinLerp(x / 2);
	
	q->w = mulf32(mulf32(cr, cp), cy) + mulf32(mulf32(sr, sp), sy);
	q->x = mulf32(mulf32(sr, cp), cy) - mulf32(mulf32(cr, sp), sy);
	q->y = mulf32(mulf32(cr, sp), cy) + mulf32(mulf32(sr, cp), sy);
	q->z = mulf32(mulf32(cr, cp), sy) - mulf32(mulf32(sr, sp), cy);
}

void QuatTimesQuat(Quaternion *left, Quaternion *right, Quaternion *out) {
	out->w = mulf32(left->w, right->w) - mulf32(left->x, right->x) - mulf32(left->y, right->y) - mulf32(left->z, right->z);
	out->x = mulf32(left->w, right->x) + mulf32(left->x, right->w) + mulf32(left->y, right->z) - mulf32(left->z, right->y);
	out->y = mulf32(left->w, right->y) - mulf32(left->x, right->z) + mulf32(left->y, right->w) + mulf32(left->z, right->x);
	out->z = mulf32(left->w, right->z) + mulf32(left->x, right->y) - mulf32(left->y, right->x) + mulf32(left->z, right->w);
}

void QuatNormalize(Quaternion *input) {
	f32 magnitude = sqrtf32(mulf32(input->x, input->x) + mulf32(input->y, input->y) + mulf32(input->z, input->z) + mulf32(input->w, input->w));
	input->x = divf32(input->x, magnitude);
	input->y = divf32(input->y, magnitude);
	input->z = divf32(input->z, magnitude);
	input->w = divf32(input->w, magnitude);
}

void QuatNormalizeFast(Quaternion* input) {
#ifndef _NOTDS
	f32 magnitude = (input->x * input->x + input->y * input->y + input->z * input->z + input->w * input->w);
	REG_SQRTCNT = SQRT_32;
	while (REG_SQRTCNT & SQRT_BUSY);
	REG_SQRT_PARAM_L = magnitude;
	while (REG_SQRTCNT & SQRT_BUSY);
	magnitude = REG_SQRT_RESULT;
	/*input->x = divf32(input->x, magnitude);
	input->y = divf32(input->y, magnitude);
	input->z = divf32(input->z, magnitude);
	input->w = divf32(input->w, magnitude);*/
	input->x = (input->x << 12) / magnitude;
	input->y = (input->y << 12) / magnitude;
	input->z = (input->z << 12) / magnitude;
	input->w = (input->w << 12) / magnitude;
#else
	QuatNormalize(input);
#endif
}

#ifndef _NOTDS
#define mulf32fast(a, b) (((a) * (b)) >> 12)
#define divf32fast(a, b) (((a) << 12) / (b))
#else
#define mulf32fast mulf32
#define divf32fast divf32
#endif

void QuatSlerp(Quaternion *left, Quaternion *right, Quaternion *out, f32 t) {
	f32 cosOmega = mulf32(left->x, right->x) + mulf32(left->y, right->y) + mulf32(left->z, right->z) + mulf32(left->w, right->w);
	Quaternion usedRight;
	usedRight.x = right->x;
	usedRight.y = right->y;
	usedRight.z = right->z;
	usedRight.w = right->w;
	// flip signs if necessary to maintain shortest path
	if (cosOmega < 0) {
		cosOmega = -cosOmega;
		usedRight.x = -usedRight.x;
		usedRight.y = -usedRight.y;
		usedRight.z = -usedRight.z;
		usedRight.w = -usedRight.w;
	}
	// clamp cosomega to 1.0
	if (cosOmega > Fixed32ToNative(4096)) {
		cosOmega = Fixed32ToNative(4096);
	}
	f32 scaleFrom;
	f32 scaleTo;
	// algorithm doesn't work well for extreme values, employ regular lerp
	if (cosOmega > Fixed32ToNative(4095)) {
		scaleFrom = Fixed32ToNative(4096) - t;
		scaleTo = t;
	} else {
		// standard slerp
		f32 omega = acosLerp(cosOmega);
		f32 sinOmega = sqrtf32(Fixed32ToNative(4096) - mulf32fast(cosOmega, cosOmega));
		omega = mulf32(omega, RotationToFixedRadians);
		//mulf32(divf32(omega, 32767), 25735);
		scaleFrom = divf32fast(sinLerp(mulf32fast(Fixed32ToNative(4096) - t, omega)), sinOmega);
		scaleTo = divf32fast(sinLerp(mulf32fast(t, omega)), sinOmega);
	}
#ifdef _NOTDS
	out->x = mulf32(scaleFrom, left->x) + mulf32(scaleTo, usedRight.x);
	out->y = mulf32(scaleFrom, left->y) + mulf32(scaleTo, usedRight.y);
	out->z = mulf32(scaleFrom, left->z) + mulf32(scaleTo, usedRight.z);
	out->w = mulf32(scaleFrom, left->w) + mulf32(scaleTo, usedRight.w);
#else
	out->x = (scaleFrom * left->x + scaleTo * usedRight.x) >> 12;
	out->y = (scaleFrom * left->y + scaleTo * usedRight.y) >> 12;
	out->z = (scaleFrom * left->z + scaleTo * usedRight.z) >> 12;
	out->w = (scaleFrom * left->w + scaleTo * usedRight.w) >> 12;
#endif
	
	// maybe normalize?
	QuatNormalizeFast(out);
}

f32 DotProduct(Vec3 *left, Vec3 *right) {
	return mulf32(left->x, right->x) + mulf32(left->y, right->y) + mulf32(left->z, right->z);
}

void CrossProduct(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = mulf32(left->y, right->z) - mulf32(left->z, right->y);
	out->y = -(mulf32(left->x, right->z) - mulf32(left->z, right->x));
	out->z = mulf32(left->x, right->y) - mulf32(left->y, right->x);
}

void QuatTimesVec3(Quaternion *quat, Vec3 *vec, Vec3 *out) {
	// huh
	Vec3 *u = (Vec3*)quat;
	f32 dot = DotProduct(u, vec) * 2;
	Vec3 temp;
	temp.x = mulf32(u->x, dot);
	temp.y = mulf32(u->y, dot);
	temp.z = mulf32(u->z, dot);
	
	f32 s = mulf32(quat->w, quat->w);
	s -= DotProduct(u, u);
	temp.x += mulf32(s, vec->x);
	temp.y += mulf32(s, vec->y);
	temp.z += mulf32(s, vec->z);
	s = quat->w * 2;
	Vec3 cross;
	CrossProduct(u, vec, &cross);
	out->x = temp.x + mulf32(cross.x, s);
	out->y = temp.y + mulf32(cross.y, s);
	out->z = temp.z + mulf32(cross.z, s);
}

void QuaternionInverse(Quaternion *quat, Quaternion *out) {
	out->x = -quat->x;
	out->y = -quat->y;
	out->z = -quat->z;
	out->w = quat->w;
}

void QuaternionFromAngleAxis(f32 angle, Vec3 *axis, Quaternion *out) {
	angle /= 2;
	f32 s = sinLerp(angle);
	out->x = mulf32(s, axis->x);
	out->y = mulf32(s, axis->y);
	out->z = mulf32(s, axis->z);
	out->w = cosLerp(angle);
}

void VectorFromToRotation(Vec3 *v1, Vec3 *v2, Quaternion *out) {
	f32 dot = DotProduct(v1, v2);
	Vec3 a;
	if (dot <= -Fixed32ToNative(4095)) {
		Vec3 tmp;
		tmp.x = Fixed32ToNative(4096);
		tmp.y = 0;
		tmp.z = 0;
		CrossProduct(&tmp, v1, &a);
		if (SqrMagnitude(&a) == 0) {
			tmp.x = 0;
			tmp.y = Fixed32ToNative(4096);
			CrossProduct(&tmp, v1, &a);
		}
		Normalize(&a, &a);
		QuaternionFromAngleAxis(mulf32(180*Fixed32ToNative(4096), FixedDegreesToRotation), &a, out);
		return;
	}
	if (dot >= Fixed32ToNative(4095)) {
		// this is dumb
		out->x = 0;
		out->y = 0;
		out->z = 0;
		out->w = Fixed32ToNative(4096);
	}
	
	CrossProduct(v1, v2, &a);
	out->x = a.x;
	out->y = a.y;
	out->z = a.z;
	out->w = sqrtf32(mulf32(SqrMagnitude(v1), SqrMagnitude(v2))) + dot;
	QuatNormalize(out);
}

f32 Magnitude(Vec3 *vec) {
	return sqrtf32(mulf32(vec->x, vec->x) + mulf32(vec->y, vec->y) + mulf32(vec->z, vec->z));
}

f32 SqrMagnitude(Vec3 *vec) {
	return mulf32(vec->x, vec->x) + mulf32(vec->y, vec->y) + mulf32(vec->z, vec->z);
}

void Normalize(Vec3 *vec, Vec3 *out) {
	f32 magnitude = Magnitude(vec);
	out->x = divf32(vec->x, magnitude);
	out->y = divf32(vec->y, magnitude);
	out->z = divf32(vec->z, magnitude);
}

void Vec3Addition(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = left->x + right->x;
	out->y = left->y + right->y;
	out->z = left->z + right->z;
}

void Vec3Subtraction(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = left->x - right->x;
	out->y = left->y - right->y;
	out->z = left->z - right->z;
}

void Vec3Multiplication(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = mulf32(left->x, right->x);
	out->y = mulf32(left->y, right->y);
	out->z = mulf32(left->z, right->z);
}

void Vec3Division(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = divf32(left->x, right->x);
	out->y = divf32(left->y, right->y);
	out->z = divf32(left->z, right->z);
}

f32 Lerp(f32 left, f32 right, f32 t) {
	return left + mulf32(t, right - left);
}

f32 Atan2(f32 y, f32 x) {
	// this implementation has issues in floating point for some reason *shrug*
#ifndef _NOTDS
	const f32 b = Fixed32ToNative(2442);
	// arc tangent in first quadrant
	f32 bx_a = abs(mulf32(b, mulf32(x, y)));
	f32 num = bx_a + mulf32(y, y);
	f32 atan_1q = divf32(num, mulf32(x, x) + bx_a + num);
	// multiply by half pi
	atan_1q = mulf32(atan_1q,Fixed32ToNative(6434));
	// now set up the quadrant
	if (x < 0) {
		atan_1q = Fixed32ToNative(6434) + (Fixed32ToNative(6434) - atan_1q);
	}
	if (y < 0) {
		atan_1q = -atan_1q;
	}
	return mulf32(atan_1q, FixedRadiansToRotation);
#else
	return atan2f(y, x);
#endif
}

void Reflect(Vec3 *a, Vec3 *b, Vec3 *out) {
	f32 dot = DotProduct(a, b);
	dot *= 2;
	Vec3 tmp;
	tmp.x = mulf32(b->x, dot);
	tmp.y = mulf32(b->y, dot);
	tmp.z = mulf32(b->z, dot);
	out->x = a->x - tmp.x;
	out->y = a->y - tmp.y;
	out->z = a->z - tmp.z;
}

f32 Clamp(f32 value, f32 min, f32 max) {
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
	return value;
}

int iClamp(int value, int min, int max) {
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
	return value;
}

f32 Max(f32 value, f32 max) {
	if (max > value) {
		return max;
	}
	return value;
}

f32 Min(f32 value, f32 min) {
	if (min < value) {
		return min;
	}
	return value;
}

f32 DeltaAngle(f32 dir1, f32 dir2)
{
	f32 a = dir2 - dir1;

	a += Angle16ToNative(32767)/2;

	if (a < 0)
	{
		a += Angle16ToNative(32767);
	}

	if (a > Angle16ToNative(32767))
	{
		a -= Angle16ToNative(32767);
	}

	a -= Angle16ToNative(32767)/2;

	return a;
}

f32 Pow(f32 value, f32 toPow) {
	f32 retValue;
	if (toPow >= Fixed32ToNative(4096)) {
		retValue = value;
		toPow -= Fixed32ToNative(4096);
	} else {
		retValue = Lerp(Fixed32ToNative(4096), value, toPow);
	}
	while (toPow >= Fixed32ToNative(4096)) {
		retValue = mulf32(retValue, value);
		toPow -= Fixed32ToNative(4096);
	}
	if (toPow > 0) {
		retValue = mulf32(retValue, Lerp(Fixed32ToNative(4096), value, toPow));
	}
	return retValue;
}

void NormalFromVerts(Vec3 *vert1, Vec3 *vert2, Vec3 *vert3, Vec3 *out) {
	Vec3 U, V;
	U.x = vert2->x - vert1->x;
	U.y = vert2->y - vert1->y;
	U.z = vert2->z - vert1->z;
	V.x = vert3->x - vert1->x;
	V.y = vert3->y - vert1->y;
	V.z = vert3->z - vert1->z;
	out->x = mulf32(U.y, V.z) - mulf32(U.z, V.y);
	out->y = mulf32(U.z, V.x) - mulf32(U.x, V.z);
	out->z = mulf32(U.x, V.y) - mulf32(U.y, V.x);
	Normalize(out, out);
}

// above function is unreliable at low precision
void NormalFromVertsFloat(Vec3* vert1, Vec3* vert2, Vec3* vert3, Vec3* out) {
	Vec3f U, V;
	U.x = f32tofloat(vert2->x - vert1->x);
	U.y = f32tofloat(vert2->y - vert1->y);
	U.z = f32tofloat(vert2->z - vert1->z);
	V.x = f32tofloat(vert3->x - vert1->x);
	V.y = f32tofloat(vert3->y - vert1->y);
	V.z = f32tofloat(vert3->z - vert1->z);
	Vec3f computed;
	computed.x = (U.y * V.z) - (U.z * V.y);
	computed.y = (U.z * V.x) - (U.x * V.z);
	computed.z = (U.x * V.y) - (U.y * V.x);
	float magnitude = sqrtf(computed.x * computed.x + computed.y * computed.y + computed.z * computed.z);
	out->x = floattof32(computed.x / magnitude);
	out->y = floattof32(computed.y / magnitude);
	out->z = floattof32(computed.z / magnitude);
}

void FrustumToMatrix(f32 xmin, f32 xmax, f32 ymin, f32 ymax, f32 near, f32 far, m4x4 *ret) {
	ret->m[0] = divf32(2 * near, xmax - xmin);
	ret->m[1] = 0;
	ret->m[2] = divf32(xmax + xmin, xmax - xmin);
	ret->m[3] = 0;
	ret->m[4] = 0;
	ret->m[5] = divf32(2 * near, ymax - ymin);
	ret->m[6] = divf32(ymax + ymin, ymax - ymin);
	ret->m[7] = 0;
	ret->m[8] = 0;
	ret->m[9] = 0;
	ret->m[10] = -divf32(far + near, far - near);
	ret->m[11] = -divf32(2 * mulf32(far, near), far - near);
	ret->m[12] = 0;
	ret->m[13] = 0;
	ret->m[14] = -1.0f;
	ret->m[15] = 0;
}

void MakePerspectiveMatrix(f32 fov, f32 aspect, f32 near, f32 far, m4x4* ret) {
	f32 xmin, xmax, ymin, ymax;

	ymax = mulf32(near, tanLerp(fov / 2));

	ymin = -ymax;
	xmin = mulf32(ymin, aspect);
	xmax = mulf32(ymax, aspect);

	FrustumToMatrix(xmin, xmax, ymin, ymax, near, far, ret);
}

f32 f32Mod(f32 left, f32 right) {
#ifndef _NOTDS
	return left % right;
#else
	return fmodf(left, right);
#endif
}

void MatrixTimesVec3(m4x4 *left, Vec3 *right, Vec3 *ret) {
	ret->x = mulf32(left->m[0], right->x) + mulf32(left->m[1], right->y) + mulf32(left->m[2], right->z) + left->m[3];
	ret->y = mulf32(left->m[4], right->x) + mulf32(left->m[5], right->y) + mulf32(left->m[6], right->z) + left->m[7];
	ret->z = mulf32(left->m[8], right->x) + mulf32(left->m[9], right->y) + mulf32(left->m[10], right->z) + left->m[11];
}

f32 f32abs(f32 input) {
#ifndef _NOTDS
	return abs(input);
#else
	return fabsf(input);
#endif
}

void ExtractPlanesFromProj(
	const m4x4* mat,
	Vec4* left, Vec4* right,
	Vec4* bottom, Vec4* top,
	Vec4* near, Vec4* far)
{
	for (int i = 4; i--; ) left->coords[i] = mat->m[12 + i] + mat->m[i];
	for (int i = 4; i--; ) right->coords[i] = mat->m[12 + i] - mat->m[i];
	for (int i = 4; i--; ) bottom->coords[i] = mat->m[12 + i] + mat->m[4 + i];
	for (int i = 4; i--; ) top->coords[i] = mat->m[12 + i] - mat->m[4 + i];
	for (int i = 4; i--; ) near->coords[i] = mat->m[12 + i] + mat->m[8 + i];
	for (int i = 4; i--; ) far->coords[i] = mat->m[12 + i] - mat->m[8 + i];
	// i'm not really sure why, but things i see online seem to suggest you DONT normalize these
	/*Normalize((Vec3*)left, (Vec3*)left);
	Normalize((Vec3*)right, (Vec3*)right);
	Normalize((Vec3*)bottom, (Vec3*)bottom);
	Normalize((Vec3*)top, (Vec3*)top);
	Normalize((Vec3*)near, (Vec3*)near);
	Normalize((Vec3*)far, (Vec3*)far);*/
}

bool InvertMatrix(const m4x4* m, m4x4* invOut)
{
	m4x4 inv;
	float det;
	int i;

	inv.m[0] = m->m[5] * m->m[10] * m->m[15] -
		m->m[5] * m->m[11] * m->m[14] -
		m->m[9] * m->m[6] * m->m[15] +
		m->m[9] * m->m[7] * m->m[14] +
		m->m[13] * m->m[6] * m->m[11] -
		m->m[13] * m->m[7] * m->m[10];

	inv.m[4] = -m->m[4] * m->m[10] * m->m[15] +
		m->m[4] * m->m[11] * m->m[14] +
		m->m[8] * m->m[6] * m->m[15] -
		m->m[8] * m->m[7] * m->m[14] -
		m->m[12] * m->m[6] * m->m[11] +
		m->m[12] * m->m[7] * m->m[10];

	inv.m[8] = m->m[4] * m->m[9] * m->m[15] -
		m->m[4] * m->m[11] * m->m[13] -
		m->m[8] * m->m[5] * m->m[15] +
		m->m[8] * m->m[7] * m->m[13] +
		m->m[12] * m->m[5] * m->m[11] -
		m->m[12] * m->m[7] * m->m[9];

	inv.m[12] = -m->m[4] * m->m[9] * m->m[14] +
		m->m[4] * m->m[10] * m->m[13] +
		m->m[8] * m->m[5] * m->m[14] -
		m->m[8] * m->m[6] * m->m[13] -
		m->m[12] * m->m[5] * m->m[10] +
		m->m[12] * m->m[6] * m->m[9];

	inv.m[1] = -m->m[1] * m->m[10] * m->m[15] +
		m->m[1] * m->m[11] * m->m[14] +
		m->m[9] * m->m[2] * m->m[15] -
		m->m[9] * m->m[3] * m->m[14] -
		m->m[13] * m->m[2] * m->m[11] +
		m->m[13] * m->m[3] * m->m[10];

	inv.m[5] = m->m[0] * m->m[10] * m->m[15] -
		m->m[0] * m->m[11] * m->m[14] -
		m->m[8] * m->m[2] * m->m[15] +
		m->m[8] * m->m[3] * m->m[14] +
		m->m[12] * m->m[2] * m->m[11] -
		m->m[12] * m->m[3] * m->m[10];

	inv.m[9] = -m->m[0] * m->m[9] * m->m[15] +
		m->m[0] * m->m[11] * m->m[13] +
		m->m[8] * m->m[1] * m->m[15] -
		m->m[8] * m->m[3] * m->m[13] -
		m->m[12] * m->m[1] * m->m[11] +
		m->m[12] * m->m[3] * m->m[9];

	inv.m[13] = m->m[0] * m->m[9] * m->m[14] -
		m->m[0] * m->m[10] * m->m[13] -
		m->m[8] * m->m[1] * m->m[14] +
		m->m[8] * m->m[2] * m->m[13] +
		m->m[12] * m->m[1] * m->m[10] -
		m->m[12] * m->m[2] * m->m[9];

	inv.m[2] = m->m[1] * m->m[6] * m->m[15] -
		m->m[1] * m->m[7] * m->m[14] -
		m->m[5] * m->m[2] * m->m[15] +
		m->m[5] * m->m[3] * m->m[14] +
		m->m[13] * m->m[2] * m->m[7] -
		m->m[13] * m->m[3] * m->m[6];

	inv.m[6] = -m->m[0] * m->m[6] * m->m[15] +
		m->m[0] * m->m[7] * m->m[14] +
		m->m[4] * m->m[2] * m->m[15] -
		m->m[4] * m->m[3] * m->m[14] -
		m->m[12] * m->m[2] * m->m[7] +
		m->m[12] * m->m[3] * m->m[6];

	inv.m[10] = m->m[0] * m->m[5] * m->m[15] -
		m->m[0] * m->m[7] * m->m[13] -
		m->m[4] * m->m[1] * m->m[15] +
		m->m[4] * m->m[3] * m->m[13] +
		m->m[12] * m->m[1] * m->m[7] -
		m->m[12] * m->m[3] * m->m[5];

	inv.m[14] = -m->m[0] * m->m[5] * m->m[14] +
		m->m[0] * m->m[6] * m->m[13] +
		m->m[4] * m->m[1] * m->m[14] -
		m->m[4] * m->m[2] * m->m[13] -
		m->m[12] * m->m[1] * m->m[6] +
		m->m[12] * m->m[2] * m->m[5];

	inv.m[3] = -m->m[1] * m->m[6] * m->m[11] +
		m->m[1] * m->m[7] * m->m[10] +
		m->m[5] * m->m[2] * m->m[11] -
		m->m[5] * m->m[3] * m->m[10] -
		m->m[9] * m->m[2] * m->m[7] +
		m->m[9] * m->m[3] * m->m[6];

	inv.m[7] = m->m[0] * m->m[6] * m->m[11] -
		m->m[0] * m->m[7] * m->m[10] -
		m->m[4] * m->m[2] * m->m[11] +
		m->m[4] * m->m[3] * m->m[10] +
		m->m[8] * m->m[2] * m->m[7] -
		m->m[8] * m->m[3] * m->m[6];

	inv.m[11] = -m->m[0] * m->m[5] * m->m[11] +
		m->m[0] * m->m[7] * m->m[9] +
		m->m[4] * m->m[1] * m->m[11] -
		m->m[4] * m->m[3] * m->m[9] -
		m->m[8] * m->m[1] * m->m[7] +
		m->m[8] * m->m[3] * m->m[5];

	inv.m[15] = m->m[0] * m->m[5] * m->m[10] -
		m->m[0] * m->m[6] * m->m[9] -
		m->m[4] * m->m[1] * m->m[10] +
		m->m[4] * m->m[2] * m->m[9] +
		m->m[8] * m->m[1] * m->m[6] -
		m->m[8] * m->m[2] * m->m[5];

	det = m->m[0] * inv.m[0] + m->m[1] * inv.m[4] + m->m[2] * inv.m[8] + m->m[3] * inv.m[12];

	if (det == 0)
		return false;
	det = 1.0f / det;

	for (i = 0; i < 16; i++)
		invOut->m[i] = inv.m[i] * det;

	return true;
}

// this function does not work on ds
void GenerateViewFrustum(m4x4* matrix, ViewFrustum* frustumOut) {
	ExtractPlanesFromProj(matrix, &frustumOut->planes[0], &frustumOut->planes[1], &frustumOut->planes[2], &frustumOut->planes[3], &frustumOut->planes[4], &frustumOut->planes[5]);
	m4x4 inverted;
	InvertMatrix(matrix, &inverted);
	// we have to generate all the vertices of the matrix for our frustum AABB code...
	Vec3 workVec = { -Fixed32ToNative(4096), -Fixed32ToNative(4096), -Fixed32ToNative(4096) };
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[0]);
	workVec.z = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[1]);
	workVec.z = -Fixed32ToNative(4096);
	workVec.y = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[2]);
	workVec.z = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[3]);
	workVec.y = -Fixed32ToNative(4096);
	workVec.z = -Fixed32ToNative(4096);
	workVec.x = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[4]);
	workVec.z = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[5]);
	workVec.z = -Fixed32ToNative(4096);
	workVec.y = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[6]);
	workVec.z = Fixed32ToNative(4096);
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[7]);
}

#ifndef _NOTDS
long long Int64Div(int left, int right) {
	REG_DIVCNT = DIV_64_32;

	while (REG_DIVCNT & DIV_BUSY);

	REG_DIV_NUMER = ((int64)left) << 12;
	REG_DIV_DENOM_L = right;

	while (REG_DIVCNT & DIV_BUSY);

	return REG_DIV_RESULT;
}
#endif