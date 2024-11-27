#include <math.h>
#include <nds.h>
#include <stdio.h>
#include "sdmath.h"
#include <string.h>
#include <stdlib.h>

//#define FixedToRadians 10430
#define FixedToRadians 5215

#define ZeroMatrix(matrix) for(int i = 0; i < 16; ++i) {matrix->m[i] = 0;}

#define mulf32fast(a, b) (((a) * (b)) >> 12)
#define divf32fast(a, b) (((a) << 12) / (b))

ITCM_CODE void MakeTranslationMatrix(f32 x, f32 y, f32 z, m4x4 *retValue) {
	ZeroMatrix(retValue);
	retValue->m[r1x] = 4096;
	retValue->m[r2y] = 4096;
	retValue->m[r3z] = 4096;
	retValue->m[r4w] = 4096;
	retValue->m[r1w] = x;
	retValue->m[r2w] = y;
	retValue->m[r3w] = z;
}

ITCM_CODE void MakeScaleMatrix(f32 x, f32 y, f32 z, m4x4 *retValue) {
	ZeroMatrix(retValue);
	retValue->m[r1x] = x;
	retValue->m[r2y] = y;
	retValue->m[r3z] = z;
	retValue->m[r4w] = 4096;
}

ITCM_CODE void Combine3x3Matrices(m4x4* left, m4x4* right, m4x4* retValue) {
	retValue->m[r1x] = mulf32(left->m[r1x], right->m[r1x]) + mulf32(left->m[r1y], right->m[r2x]) + mulf32(left->m[r1z], right->m[r3x]);// + mulf32(left->m[r1w], right->m[r4x]);
    retValue->m[r1y] = mulf32(left->m[r1x], right->m[r1y]) + mulf32(left->m[r1y], right->m[r2y]) + mulf32(left->m[r1z], right->m[r3y]);// + mulf32(left->m[r1w], right->m[r4y]);
    retValue->m[r1z] = mulf32(left->m[r1x], right->m[r1z]) + mulf32(left->m[r1y], right->m[r2z]) + mulf32(left->m[r1z], right->m[r3z]);// + mulf32(left->m[r1w], right->m[r4z]);
    retValue->m[r1w] = 0;

    retValue->m[r2x] = mulf32(left->m[r2x], right->m[r1x]) + mulf32(left->m[r2y], right->m[r2x]) + mulf32(left->m[r2z], right->m[r3x]);// + left->m[r2w] * right->m[r4x];
    retValue->m[r2y] = mulf32(left->m[r2x], right->m[r1y]) + mulf32(left->m[r2y], right->m[r2y]) + mulf32(left->m[r2z], right->m[r3y]);// + left->m[r2w] * right->m[r4y];
    retValue->m[r2z] = mulf32(left->m[r2x], right->m[r1z]) + mulf32(left->m[r2y], right->m[r2z]) + mulf32(left->m[r2z], right->m[r3z]);// + left->m[r2w] * right->m[r4z];
    retValue->m[r2w] = 0;

    retValue->m[r3x] = mulf32(left->m[r3x], right->m[r1x]) + mulf32(left->m[r3y], right->m[r2x]) + mulf32(left->m[r3z], right->m[r3x]);// + left->m[r3w] * right->m[r4x];
    retValue->m[r3y] = mulf32(left->m[r3x], right->m[r1y]) + mulf32(left->m[r3y], right->m[r2y]) + mulf32(left->m[r3z], right->m[r3y]);// + left->m[r3w] * right->m[r4y];
    retValue->m[r3z] = mulf32(left->m[r3x], right->m[r1z]) + mulf32(left->m[r3y], right->m[r2z]) + mulf32(left->m[r3z], right->m[r3z]);// + left->m[r3w] * right->m[r4z];
    retValue->m[r3w] = 0;

	retValue->m[r4x] = 0;
	retValue->m[r4y] = 0;
	retValue->m[r4z] = 0;
	retValue->m[r4w] = 4096;
}

// this function is HYPER optimized, and excludes row 4. consider it a 4x3 matrix instead
ITCM_CODE void CombineMatrices(m4x4 *left, m4x4 *right, m4x4 *retValue) {
	// i think this should work?
    retValue->m[r1x] = mulf32(left->m[r1x], right->m[r1x]) + mulf32(left->m[r1y], right->m[r2x]) + mulf32(left->m[r1z], right->m[r3x]);
    retValue->m[r1y] = mulf32(left->m[r1x], right->m[r1y]) + mulf32(left->m[r1y], right->m[r2y]) + mulf32(left->m[r1z], right->m[r3y]);
    retValue->m[r1z] = mulf32(left->m[r1x], right->m[r1z]) + mulf32(left->m[r1y], right->m[r2z]) + mulf32(left->m[r1z], right->m[r3z]);
    retValue->m[r1w] = mulf32(left->m[r1x], right->m[r1w]) + mulf32(left->m[r1y], right->m[r2w]) + mulf32(left->m[r1z], right->m[r3w]) + left->m[r1w];

    retValue->m[r2x] = mulf32(left->m[r2x], right->m[r1x]) + mulf32(left->m[r2y], right->m[r2x]) + mulf32(left->m[r2z], right->m[r3x]);
    retValue->m[r2y] = mulf32(left->m[r2x], right->m[r1y]) + mulf32(left->m[r2y], right->m[r2y]) + mulf32(left->m[r2z], right->m[r3y]);
	retValue->m[r2z] = mulf32(left->m[r2x], right->m[r1z]) + mulf32(left->m[r2y], right->m[r2z]) + mulf32(left->m[r2z], right->m[r3z]);
    retValue->m[r2w] = mulf32(left->m[r2x], right->m[r1w]) + mulf32(left->m[r2y], right->m[r2w]) + mulf32(left->m[r2z], right->m[r3w]) + left->m[r2w];

    retValue->m[r3x] = mulf32(left->m[r3x], right->m[r1x]) + mulf32(left->m[r3y], right->m[r2x]) + mulf32(left->m[r3z], right->m[r3x]);
    retValue->m[r3y] = mulf32(left->m[r3x], right->m[r1y]) + mulf32(left->m[r3y], right->m[r2y]) + mulf32(left->m[r3z], right->m[r3y]);
    retValue->m[r3z] = mulf32(left->m[r3x], right->m[r1z]) + mulf32(left->m[r3y], right->m[r2z]) + mulf32(left->m[r3z], right->m[r3z]);
    retValue->m[r3w] = mulf32(left->m[r3x], right->m[r1w]) + mulf32(left->m[r3y], right->m[r2w]) + mulf32(left->m[r3z], right->m[r3w]) + left->m[r3w];
	retValue->m[r4x] = 0;
	retValue->m[r4y] = 0;
	retValue->m[r4z] = 0;
	retValue->m[r4w] = 4096;
}

ITCM_CODE void CombineMatricesFull(m4x4* left, m4x4* right, m4x4* retValue) {
	retValue->m[r1x] = mulf32(left->m[r1x], right->m[r1x]) + mulf32(left->m[r1y], right->m[r2x]) + mulf32(left->m[r1z], right->m[r3x]) + mulf32(left->m[r1w], right->m[r4x]);
	retValue->m[r1y] = mulf32(left->m[r1x], right->m[r1y]) + mulf32(left->m[r1y], right->m[r2y]) + mulf32(left->m[r1z], right->m[r3y]) + mulf32(left->m[r1w], right->m[r4y]);
	retValue->m[r1z] = mulf32(left->m[r1x], right->m[r1z]) + mulf32(left->m[r1y], right->m[r2z]) + mulf32(left->m[r1z], right->m[r3z]) + mulf32(left->m[r1w], right->m[r4z]);
	retValue->m[r1w] = mulf32(left->m[r1x], right->m[r1w]) + mulf32(left->m[r1y], right->m[r2w]) + mulf32(left->m[r1z], right->m[r3w]) + mulf32(left->m[r1w], right->m[r4w]);

	retValue->m[r2x] = mulf32(left->m[r2x], right->m[r1x]) + mulf32(left->m[r2y], right->m[r2x]) + mulf32(left->m[r2z], right->m[r3x]) + mulf32(left->m[r2w], right->m[r4x]);
	retValue->m[r2y] = mulf32(left->m[r2x], right->m[r1y]) + mulf32(left->m[r2y], right->m[r2y]) + mulf32(left->m[r2z], right->m[r3y]) + mulf32(left->m[r2w], right->m[r4y]);
	retValue->m[r2z] = mulf32(left->m[r2x], right->m[r1z]) + mulf32(left->m[r2y], right->m[r2z]) + mulf32(left->m[r2z], right->m[r3z]) + mulf32(left->m[r2w], right->m[r4z]);
	retValue->m[r2w] = mulf32(left->m[r2x], right->m[r1w]) + mulf32(left->m[r2y], right->m[r2w]) + mulf32(left->m[r2z], right->m[r3w]) + mulf32(left->m[r2w], right->m[r4w]);

	retValue->m[r3x] = mulf32(left->m[r3x], right->m[r1x]) + mulf32(left->m[r3y], right->m[r2x]) + mulf32(left->m[r3z], right->m[r3x]) + mulf32(left->m[r3w], right->m[r4x]);
	retValue->m[r3y] = mulf32(left->m[r3x], right->m[r1y]) + mulf32(left->m[r3y], right->m[r2y]) + mulf32(left->m[r3z], right->m[r3y]) + mulf32(left->m[r3w], right->m[r4y]);
	retValue->m[r3z] = mulf32(left->m[r3x], right->m[r1z]) + mulf32(left->m[r3y], right->m[r2z]) + mulf32(left->m[r3z], right->m[r3z]) + mulf32(left->m[r3w], right->m[r4z]);
	retValue->m[r3w] = mulf32(left->m[r3x], right->m[r1w]) + mulf32(left->m[r3y], right->m[r2w]) + mulf32(left->m[r3z], right->m[r3w]) + mulf32(left->m[r3w], right->m[r4w]);

	retValue->m[r4x] = mulf32(left->m[r4x], right->m[r1x]) + mulf32(left->m[r4y], right->m[r2x]) + mulf32(left->m[r4z], right->m[r3x]) + mulf32(left->m[r4w], right->m[r4x]);
	retValue->m[r4y] = mulf32(left->m[r4x], right->m[r1y]) + mulf32(left->m[r4y], right->m[r2y]) + mulf32(left->m[r4z], right->m[r3y]) + mulf32(left->m[r4w], right->m[r4y]);
	retValue->m[r4z] = mulf32(left->m[r4x], right->m[r1z]) + mulf32(left->m[r4y], right->m[r2z]) + mulf32(left->m[r4z], right->m[r3z]) + mulf32(left->m[r4w], right->m[r4z]);
	retValue->m[r4w] = mulf32(left->m[r4x], right->m[r1w]) + mulf32(left->m[r4y], right->m[r2w]) + mulf32(left->m[r4z], right->m[r3w]) + mulf32(left->m[r4w], right->m[r4w]);
}

ITCM_CODE void TransposeMatrix(m4x4* input, m4x4* output) {
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

ITCM_CODE void MakeRotationMatrix(Quaternion *input, m4x4 *retValue) {
	f32 ww = mulf32fast(input->w, input->w);
	f32 xy = mulf32fast(input->x, input->y);
	f32 yz = mulf32fast(input->y, input->z);
	f32 xz = mulf32fast(input->x, input->z);
	f32 wx = mulf32fast(input->w, input->x);
	f32 wy = mulf32fast(input->w, input->y);
	f32 wz = mulf32fast(input->w, input->z);
	// oh god i have no idea what any of this means
	retValue->m[r1x] = 2 * (ww + mulf32fast(input->x, input->x)) - 4096;
	retValue->m[r1y] = 2 * (xy - wz);
	retValue->m[r1z] = 2 * (xz + wy);
	
	retValue->m[r2x] = 2 * (xy + wz);
	retValue->m[r2y] = 2 * (ww + mulf32fast(input->y, input->y)) - 4096;
	retValue->m[r2z] = 2 * (yz - wx);
	
	retValue->m[r3x] = 2 * (xz - wy);
	retValue->m[r3y] = 2 * (yz + wx);
	retValue->m[r3z] = 2 * (ww + mulf32fast(input->z, input->z)) - 4096;
	
	retValue->m[r1w] = 0;
	retValue->m[r2w] = 0;
	retValue->m[r3w] = 0;
	retValue->m[r4x] = 0;
	retValue->m[r4y] = 0;
	retValue->m[r4z] = 0;
	retValue->m[r4w] = 4096;
}

ITCM_CODE void EulerToQuat(f32 x, f32 y, f32 z, Quaternion *q) {
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

ITCM_CODE void QuatTimesQuat(Quaternion *left, Quaternion *right, Quaternion *out) {
	out->w = mulf32(left->w, right->w) - mulf32(left->x, right->x) - mulf32(left->y, right->y) - mulf32(left->z, right->z);
	out->x = mulf32(left->w, right->x) + mulf32(left->x, right->w) + mulf32(left->y, right->z) - mulf32(left->z, right->y);
	out->y = mulf32(left->w, right->y) - mulf32(left->x, right->z) + mulf32(left->y, right->w) + mulf32(left->z, right->x);
	out->z = mulf32(left->w, right->z) + mulf32(left->x, right->y) - mulf32(left->y, right->x) + mulf32(left->z, right->w);
}

ITCM_CODE void QuatNormalize(Quaternion *input) {
	f32 magnitude = sqrtf32(mulf32(input->x, input->x) + mulf32(input->y, input->y) + mulf32(input->z, input->z) + mulf32(input->w, input->w));
	if (magnitude == 0) {
		input->w = 4096;
		return;
	}
	input->x = divf32(input->x, magnitude);
	input->y = divf32(input->y, magnitude);
	input->z = divf32(input->z, magnitude);
	input->w = divf32(input->w, magnitude);
}

ITCM_CODE void QuatNormalizeFast(Quaternion* input) {
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

ITCM_CODE void QuatSlerp(Quaternion *left, Quaternion *right, Quaternion *out, f32 t) {
	f32 cosOmega = mulf32fast(left->x, right->x) + mulf32fast(left->y, right->y) + mulf32fast(left->z, right->z) + mulf32fast(left->w, right->w);
	Quaternion usedRight;
	usedRight = *right;
	// flip signs if necessary to maintain shortest path
	if (cosOmega < 0) {
		cosOmega = -cosOmega;
		usedRight.x = -usedRight.x;
		usedRight.y = -usedRight.y;
		usedRight.z = -usedRight.z;
		usedRight.w = -usedRight.w;
	}
	// clamp cosomega to 1.0
	if (cosOmega > 4096) {
		cosOmega = 4096;
	}
	f32 scaleFrom;
	f32 scaleTo;
	// algorithm doesn't work well for extreme values, employ regular lerp
	if (cosOmega > 4095) {
		scaleFrom = 4096 - t;
		scaleTo = t;
	} else {
		// standard slerp
		f32 omega = acosLerp(cosOmega);
		f32 sinOmega = sqrtf32(4096 - mulf32fast(cosOmega, cosOmega));
		omega = mulf32fast(omega, RotationToFixedRadians);
		scaleFrom = divf32fast(sinLerp(mulf32fast(4096 - t, omega)), sinOmega);
		scaleTo = divf32fast(sinLerp(mulf32fast(t, omega)), sinOmega);
	}
	out->x = (scaleFrom * left->x + scaleTo * usedRight.x) >> 12;
	out->y = (scaleFrom * left->y + scaleTo * usedRight.y) >> 12;
	out->z = (scaleFrom * left->z + scaleTo * usedRight.z) >> 12;
	out->w = (scaleFrom * left->w + scaleTo * usedRight.w) >> 12;
	
	// maybe normalize?
	QuatNormalizeFast(out);
}

ITCM_CODE f32 DotProduct(Vec3 *left, Vec3 *right) {
	return mulf32(left->x, right->x) + mulf32(left->y, right->y) + mulf32(left->z, right->z);
}

ITCM_CODE void CrossProduct(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = mulf32(left->y, right->z) - mulf32(left->z, right->y);
	out->y = -(mulf32(left->x, right->z) - mulf32(left->z, right->x));
	out->z = mulf32(left->x, right->y) - mulf32(left->y, right->x);
}

ITCM_CODE void QuatTimesVec3(Quaternion *quat, Vec3 *vec, Vec3 *out) {
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

ITCM_CODE void QuaternionInverse(Quaternion *quat, Quaternion *out) {
	out->x = -quat->x;
	out->y = -quat->y;
	out->z = -quat->z;
	out->w = quat->w;
}

ITCM_CODE void QuaternionFromAngleAxis(f32 angle, Vec3 *axis, Quaternion *out) {
	angle /= 2;
	f32 s = sinLerp(angle);
	out->x = mulf32(s, axis->x);
	out->y = mulf32(s, axis->y);
	out->z = mulf32(s, axis->z);
	out->w = cosLerp(angle);
}

ITCM_CODE void VectorFromToRotation(Vec3 *v1, Vec3 *v2, Quaternion *out) {
	f32 dot = DotProduct(v1, v2);
	Vec3 a;
	if (dot <= -4095) {
		Vec3 tmp;
		tmp.x = 4096;
		tmp.y = 0;
		tmp.z = 0;
		CrossProduct(&tmp, v1, &a);
		if (SqrMagnitude(&a) == 0) {
			tmp.x = 0;
			tmp.y = 4096;
			CrossProduct(&tmp, v1, &a);
		}
		Normalize(&a, &a);
		QuaternionFromAngleAxis(mulf32(180*4096, FixedDegreesToRotation), &a, out);
		return;
	}
	if (dot >= 4095) {
		// this is dumb
		out->x = 0;
		out->y = 0;
		out->z = 0;
		out->w = 4096;
	}
	
	CrossProduct(v1, v2, &a);
	out->x = a.x;
	out->y = a.y;
	out->z = a.z;
	out->w = sqrtf32(mulf32(SqrMagnitude(v1), SqrMagnitude(v2))) + dot;
	QuatNormalize(out);
}

ITCM_CODE f32 Magnitude(Vec3 *vec) {
	return sqrtf32(mulf32(vec->x, vec->x) + mulf32(vec->y, vec->y) + mulf32(vec->z, vec->z));
}

ITCM_CODE f32 SqrMagnitude(Vec3 *vec) {
	return mulf32(vec->x, vec->x) + mulf32(vec->y, vec->y) + mulf32(vec->z, vec->z);
}

ITCM_CODE void Normalize(Vec3 *vec, Vec3 *out) {
	f32 magnitude = Magnitude(vec);
	if (magnitude == 0) {
		out->x = 0;
		out->y = 0;
		out->z = 0;
		return;
	}
	out->x = divf32(vec->x, magnitude);
	out->y = divf32(vec->y, magnitude);
	out->z = divf32(vec->z, magnitude);
}

ITCM_CODE void Vec3Addition(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = left->x + right->x;
	out->y = left->y + right->y;
	out->z = left->z + right->z;
}

ITCM_CODE void Vec3Subtraction(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = left->x - right->x;
	out->y = left->y - right->y;
	out->z = left->z - right->z;
}

ITCM_CODE void Vec3Multiplication(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = mulf32(left->x, right->x);
	out->y = mulf32(left->y, right->y);
	out->z = mulf32(left->z, right->z);
}

ITCM_CODE void Vec3Division(Vec3 *left, Vec3 *right, Vec3 *out) {
	out->x = divf32(left->x, right->x);
	out->y = divf32(left->y, right->y);
	out->z = divf32(left->z, right->z);
}

ITCM_CODE f32 Lerp(f32 left, f32 right, f32 t) {
	return left + mulf32(t, right - left);
}

ITCM_CODE f32 Atan2(f32 y, f32 x) {
	// this implementation has issues in floating point for some reason *shrug*
	const f32 b = 2442;
	// arc tangent in first quadrant
	f32 bx_a = abs(mulf32(b, mulf32(x, y)));
	f32 num = bx_a + mulf32(y, y);
	f32 atan_1q = divf32(num, mulf32(x, x) + bx_a + num);
	// multiply by half pi
	atan_1q = mulf32(atan_1q,6434);
	// now set up the quadrant
	if (x < 0) {
		atan_1q = 6434 + (6434 - atan_1q);
	}
	if (y < 0) {
		atan_1q = -atan_1q;
	}
	return mulf32(atan_1q, FixedRadiansToRotation);
}

ITCM_CODE void Reflect(Vec3 *a, Vec3 *b, Vec3 *out) {
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

ITCM_CODE f32 Clamp(f32 value, f32 min, f32 max) {
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
	return value;
}

ITCM_CODE int iClamp(int value, int min, int max) {
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
	return value;
}

ITCM_CODE f32 Max(f32 value, f32 max) {
	if (max > value) {
		return max;
	}
	return value;
}

ITCM_CODE f32 Min(f32 value, f32 min) {
	if (min < value) {
		return min;
	}
	return value;
}

ITCM_CODE f32 DeltaAngle(f32 dir1, f32 dir2)
{
	f32 a = dir2 - dir1;

	a += 32767/2;

	if (a < 0)
	{
		a += 32767;
	}

	if (a > 32767)
	{
		a -= 32767;
	}

	a -= 32767/2;

	return a;
}

ITCM_CODE f32 Pow(f32 value, f32 toPow) {
	f32 retValue;
	if (toPow >= 4096) {
		retValue = value;
		toPow -= 4096;
	} else {
		retValue = Lerp(4096, value, toPow);
	}
	while (toPow >= 4096) {
		retValue = mulf32(retValue, value);
		toPow -= 4096;
	}
	if (toPow > 0) {
		retValue = mulf32(retValue, Lerp(4096, value, toPow));
	}
	return retValue;
}

ITCM_CODE void NormalFromVerts(Vec3s *vert1, Vec3s *vert2, Vec3s *vert3, Vec3s *out) {
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
	f32 magnitude = sqrtf32(out->x * out->x + out->y * out->y + out->z * out->z);
	out->x = divf32(out->x, magnitude);
	out->y = divf32(out->y, magnitude);
	out->z = divf32(out->z, magnitude);
}

// above function is unreliable at low precision
ITCM_CODE void NormalFromVertsFloat(Vec3s* vert1, Vec3s* vert2, Vec3s* vert3, Vec3s* out) {
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

ITCM_CODE void FrustumToMatrix(f32 xmin, f32 xmax, f32 ymin, f32 ymax, f32 near, f32 far, m4x4 *ret) {
	ret->m[r1x] = divf32(2 * near, xmax - xmin);
	ret->m[r1y] = 0;
	ret->m[r1z] = divf32(xmax + xmin, xmax - xmin);
	ret->m[r1w] = 0;
	ret->m[r2x] = 0;
	ret->m[r2y] = divf32(2 * near, ymax - ymin);
	ret->m[r2z] = divf32(ymax + ymin, ymax - ymin);
	ret->m[r2w] = 0;
	ret->m[r3x] = 0;
	ret->m[r3y] = 0;
	ret->m[r3z] = -divf32(far + near, far - near);
	ret->m[r3w] = -divf32(2 * mulf32(far, near), far - near);
	ret->m[r4x] = 0;
	ret->m[r4y] = 0;
	ret->m[r4z] = -4096;
	ret->m[r4w] = 0;
}

ITCM_CODE void MakePerspectiveMatrix(f32 fov, f32 aspect, f32 near, f32 far, m4x4* ret) {
	f32 xmin, xmax, ymin, ymax;

	ymax = mulf32(near, tanLerp(fov / 2));

	ymin = -ymax;
	xmin = mulf32(ymin, aspect);
	xmax = mulf32(ymax, aspect);

	FrustumToMatrix(xmin, xmax, ymin, ymax, near, far, ret);
}

ITCM_CODE f32 f32Mod(f32 left, f32 right) {
	return left % right;
}

ITCM_CODE void MatrixTimesVec3(m4x4 *left, Vec3 *right, Vec3 *ret) {
	ret->x = mulf32(left->m[r1x], right->x) + mulf32(left->m[r1y], right->y) + mulf32(left->m[r1z], right->z) + left->m[r1w];
	ret->y = mulf32(left->m[r2x], right->x) + mulf32(left->m[r2y], right->y) + mulf32(left->m[r2z], right->z) + left->m[r2w];
	ret->z = mulf32(left->m[r3x], right->x) + mulf32(left->m[r3y], right->y) + mulf32(left->m[r3z], right->z) + left->m[r3w];
}

ITCM_CODE f32 f32abs(f32 input) {
	return abs(input);
}

ITCM_CODE void ExtractPlanesFromProj(
	const m4x4* mat,
	Vec4* left, Vec4* right,
	Vec4* bottom, Vec4* top,
	Vec4* near, Vec4* far)
{

	m4x4 transMat;
	TransposeMatrix(mat, &transMat);

	// uh...i need to recode this...
	for (int i = 4; i--; ) left->coords[i] = transMat.m[12 + i] + transMat.m[i];
	for (int i = 4; i--; ) right->coords[i] = transMat.m[12 + i] - transMat.m[i];
	for (int i = 4; i--; ) bottom->coords[i] = transMat.m[12 + i] + transMat.m[4 + i];
	for (int i = 4; i--; ) top->coords[i] = transMat.m[12 + i] - transMat.m[4 + i];
	for (int i = 4; i--; ) near->coords[i] = transMat.m[12 + i] + transMat.m[8 + i];
	for (int i = 4; i--; ) far->coords[i] = transMat.m[12 + i] - transMat.m[8 + i];
	// i'm not really sure why, but things i see online seem to suggest you DONT normalize these
	/*Normalize((Vec3*)left, (Vec3*)left);
	Normalize((Vec3*)right, (Vec3*)right);
	Normalize((Vec3*)bottom, (Vec3*)bottom);
	Normalize((Vec3*)top, (Vec3*)top);
	Normalize((Vec3*)near, (Vec3*)near);
	Normalize((Vec3*)far, (Vec3*)far);*/
}

// kinda omitted this one 'cause it's so seldom used
bool InvertMatrix(const m4x4* m, m4x4* invOut)
{
	m4x4 inv;
	float det;
	int i;

	inv.m[r1x] = m->m[r2y] * m->m[r3z] * m->m[r4w] -
		m->m[r2y] * m->m[r3w] * m->m[r4z] -
		m->m[r3y] * m->m[r2z] * m->m[r4w] +
		m->m[r3y] * m->m[r2w] * m->m[r4z] +
		m->m[r4y] * m->m[r2z] * m->m[r3w] -
		m->m[r4y] * m->m[r2w] * m->m[r3z];

	inv.m[r2x] = -m->m[r2x] * m->m[r3z] * m->m[r4w] +
		m->m[r2x] * m->m[r3w] * m->m[r4z] +
		m->m[r3x] * m->m[r2z] * m->m[r4w] -
		m->m[r3x] * m->m[r2w] * m->m[r4z] -
		m->m[r4x] * m->m[r2z] * m->m[r3w] +
		m->m[r4x] * m->m[r2w] * m->m[r3z];

	inv.m[r3x] = m->m[r2x] * m->m[r3y] * m->m[r4w] -
		m->m[r2x] * m->m[r3w] * m->m[r4y] -
		m->m[r3x] * m->m[r2y] * m->m[r4w] +
		m->m[r3x] * m->m[r2w] * m->m[r4y] +
		m->m[r4x] * m->m[r2y] * m->m[r3w] -
		m->m[r4x] * m->m[r2w] * m->m[r3y];

	inv.m[r4x] = -m->m[r2x] * m->m[r3y] * m->m[r4z] +
		m->m[r2x] * m->m[r3z] * m->m[r4y] +
		m->m[r3x] * m->m[r2y] * m->m[r4z] -
		m->m[r3x] * m->m[r2z] * m->m[r4y] -
		m->m[r4x] * m->m[r2y] * m->m[r3z] +
		m->m[r4x] * m->m[r2z] * m->m[r3y];

	inv.m[r1y] = -m->m[r1y] * m->m[r3z] * m->m[r4w] +
		m->m[r1y] * m->m[r3w] * m->m[r4z] +
		m->m[r3y] * m->m[r1z] * m->m[r4w] -
		m->m[r3y] * m->m[r1w] * m->m[r4z] -
		m->m[r4y] * m->m[r1z] * m->m[r3w] +
		m->m[r4y] * m->m[r1w] * m->m[r3z];

	inv.m[r2y] = m->m[r1x] * m->m[r3z] * m->m[r4w] -
		m->m[r1x] * m->m[r3w] * m->m[r4z] -
		m->m[r3x] * m->m[r1z] * m->m[r4w] +
		m->m[r3x] * m->m[r1w] * m->m[r4z] +
		m->m[r4x] * m->m[r1z] * m->m[r3w] -
		m->m[r4x] * m->m[r1w] * m->m[r3z];

	inv.m[r3y] = -m->m[r1x] * m->m[r3y] * m->m[r4w] +
		m->m[r1x] * m->m[r3w] * m->m[r4y] +
		m->m[r3x] * m->m[r1y] * m->m[r4w] -
		m->m[r3x] * m->m[r1w] * m->m[r4y] -
		m->m[r4x] * m->m[r1y] * m->m[r3w] +
		m->m[r4x] * m->m[r1w] * m->m[r3y];

	inv.m[r4y] = m->m[r1x] * m->m[r3y] * m->m[r4z] -
		m->m[r1x] * m->m[r3z] * m->m[r4y] -
		m->m[r3x] * m->m[r1y] * m->m[r4z] +
		m->m[r3x] * m->m[r1z] * m->m[r4y] +
		m->m[r4x] * m->m[r1y] * m->m[r3z] -
		m->m[r4x] * m->m[r1z] * m->m[r3y];

	inv.m[r1z] = m->m[r1y] * m->m[r2z] * m->m[r4w] -
		m->m[r1y] * m->m[r2w] * m->m[r4z] -
		m->m[r2y] * m->m[r1z] * m->m[r4w] +
		m->m[r2y] * m->m[r1w] * m->m[r4z] +
		m->m[r4y] * m->m[r1z] * m->m[r2w] -
		m->m[r4y] * m->m[r1w] * m->m[r2z];

	inv.m[r2z] = -m->m[r1x] * m->m[r2z] * m->m[r4w] +
		m->m[r1x] * m->m[r2w] * m->m[r4z] +
		m->m[r2x] * m->m[r1z] * m->m[r4w] -
		m->m[r2x] * m->m[r1w] * m->m[r4z] -
		m->m[r4x] * m->m[r1z] * m->m[r2w] +
		m->m[r4x] * m->m[r1w] * m->m[r2z];

	inv.m[r3z] = m->m[r1x] * m->m[r2y] * m->m[r4w] -
		m->m[r1x] * m->m[r2w] * m->m[r4y] -
		m->m[r2x] * m->m[r1y] * m->m[r4w] +
		m->m[r2x] * m->m[r1w] * m->m[r4y] +
		m->m[r4x] * m->m[r1y] * m->m[r2w] -
		m->m[r4x] * m->m[r1w] * m->m[r2y];

	inv.m[r4z] = -m->m[r1x] * m->m[r2y] * m->m[r4z] +
		m->m[r1x] * m->m[r2z] * m->m[r4y] +
		m->m[r2x] * m->m[r1y] * m->m[r4z] -
		m->m[r2x] * m->m[r1z] * m->m[r4y] -
		m->m[r4x] * m->m[r1y] * m->m[r2z] +
		m->m[r4x] * m->m[r1z] * m->m[r2y];

	inv.m[r1w] = -m->m[r1y] * m->m[r2z] * m->m[r3w] +
		m->m[r1y] * m->m[r2w] * m->m[r3z] +
		m->m[r2y] * m->m[r1z] * m->m[r3w] -
		m->m[r2y] * m->m[r1w] * m->m[r3z] -
		m->m[r3y] * m->m[r1z] * m->m[r2w] +
		m->m[r3y] * m->m[r1w] * m->m[r2z];

	inv.m[r2w] = m->m[r1x] * m->m[r2z] * m->m[r3w] -
		m->m[r1x] * m->m[r2w] * m->m[r3z] -
		m->m[r2x] * m->m[r1z] * m->m[r3w] +
		m->m[r2x] * m->m[r1w] * m->m[r3z] +
		m->m[r3x] * m->m[r1z] * m->m[r2w] -
		m->m[r3x] * m->m[r1w] * m->m[r2z];

	inv.m[r3w] = -m->m[r1x] * m->m[r2y] * m->m[r3w] +
		m->m[r1x] * m->m[r2w] * m->m[r3y] +
		m->m[r2x] * m->m[r1y] * m->m[r3w] -
		m->m[r2x] * m->m[r1w] * m->m[r3y] -
		m->m[r3x] * m->m[r1y] * m->m[r2w] +
		m->m[r3x] * m->m[r1w] * m->m[r2y];

	inv.m[r4w] = m->m[r1x] * m->m[r2y] * m->m[r3z] -
		m->m[r1x] * m->m[r2z] * m->m[r3y] -
		m->m[r2x] * m->m[r1y] * m->m[r3z] +
		m->m[r2x] * m->m[r1z] * m->m[r3y] +
		m->m[r3x] * m->m[r1y] * m->m[r2z] -
		m->m[r3x] * m->m[r1z] * m->m[r2y];

	det = m->m[r1x] * inv.m[r1x] + m->m[r1y] * inv.m[r2x] + m->m[r1z] * inv.m[r3x] + m->m[r1w] * inv.m[r4x];

	if (det == 0)
		return false;
	det = 1.0f / det;

	for (i = 0; i < 16; i++)
		invOut->m[i] = inv.m[i] * det;

	return true;
}

// this function does not work on ds
// addendum: this function may actually work on DS now, but i'm not using it
void GenerateViewFrustum(m4x4* matrix, ViewFrustum* frustumOut) {
	ExtractPlanesFromProj(matrix, &frustumOut->planes[0], &frustumOut->planes[1], &frustumOut->planes[2], &frustumOut->planes[3], &frustumOut->planes[4], &frustumOut->planes[5]);
	m4x4 inverted;
	InvertMatrix(matrix, &inverted);
	// we have to generate all the vertices of the matrix for our frustum AABB code...
	Vec3 workVec = { { { -4096, -4096, -4096 } } };
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[0]);
	workVec.z = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[1]);
	workVec.z = -4096;
	workVec.y = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[2]);
	workVec.z = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[3]);
	workVec.y = -4096;
	workVec.z = -4096;
	workVec.x = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[4]);
	workVec.z = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[5]);
	workVec.z = -4096;
	workVec.y = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[6]);
	workVec.z = 4096;
	MatrixTimesVec3(&inverted, &workVec, &frustumOut->points[7]);
}

ITCM_CODE int sign(int value) {
	if (value < 0) { return -1; }
	else { return 1; }
}

ITCM_CODE void QuaternionToEuler(Quaternion* quaternion, Vec3* euler) {
	// Roll (x-axis rotation)
	f32 sinr_cosp = mulf32(2 * 4096, (mulf32(quaternion->w, quaternion->x) + mulf32(quaternion->y, quaternion->z)));
	f32 cosr_cosp = 4096 - mulf32(2 * 4096, (mulf32(quaternion->x, quaternion->x) + mulf32(quaternion->y, quaternion->y)));
	euler->x = Atan2(sinr_cosp, cosr_cosp);

	// Pitch (y-axis rotation)
	f32 sinp = mulf32(2 * 4096, (mulf32(quaternion->w, quaternion->y) - mulf32(quaternion->z, quaternion->x)));
	if (f32abs(sinp) >= 4096)
		euler->y = PI / 2 * sign(sinp); // use 90 degrees if out of range
	else
		euler->y = asinLerp(sinp);

	// Yaw (z-axis rotation)
	f32 siny_cosp = mulf32(4096 * 2, (mulf32(quaternion->w, quaternion->z) + mulf32(quaternion->x, quaternion->y)));
	f32 cosy_cosp = 4096 - mulf32(2 * 4096, (mulf32(quaternion->y, quaternion->y) + mulf32(quaternion->z, quaternion->z)));
	euler->z = Atan2(siny_cosp, cosy_cosp);
}

ITCM_CODE f32 f32rand(f32 min, f32 max) {
#ifndef _NOTDS
	// simple int rand
	return (rand() % (max - min)) + min;
#else
	// convert to 0-1 and then min max it
	float rng = rand() / (float)RAND_MAX;
	rng *= max - min;
	rng += min;
	return rng;
#endif
}

ITCM_CODE long long Int64Div(int left, int right) {
#ifndef _NOTDS
	REG_DIVCNT = DIV_64_32;

	while (REG_DIVCNT & DIV_BUSY);

	REG_DIV_NUMER = ((long long)left) << 12;
	REG_DIV_DENOM_L = right;

	while (REG_DIVCNT & DIV_BUSY);

	return REG_DIV_RESULT;
#else
	return (((long long)left) << 12) / right;
#endif
}