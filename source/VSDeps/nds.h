#ifndef __NDS_H
#define __NDS_H
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#define uint unsigned int

#define _NOTDS

#define f32tofloat(value) ((value) / 4096.0f)
#define floattof32(value) ((int)((value) * 4096))

#define mulf32(left, right) ((((long long)(left))*((long long)(right))) >> 12)

#define divf32(left, right) ((((long long)(left)) << 12)/(right))

#define RGB15(r, g, b) (r & 0x1F) | ((g & 0x1F) << 5) | ((b & 0x1F) << 10)
typedef struct {
	union {
		int m[16];
		float mf[16];
	};
} m4x4;

typedef short t16;
typedef short v16;
typedef unsigned short u16;

int sinLerp(int angle);
int cosLerp(int angle);
int tanLerp(int angle);
int acosLerp(int input);
int asinLerp(int input);
int sqrtf32(int input);
#endif