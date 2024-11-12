#ifndef __NDS_H
#define __NDS_H
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#define uint unsigned int

#define _NOTDS

#define f32tofloat(value) ((value) / 4096.0f)
#define floattof32(value) ((int)((value) * 4096))

#define KEY_A 1
#define KEY_B 2
#define KEY_SELECT 4
#define KEY_START 8
#define KEY_RIGHT 16
#define KEY_LEFT 32
#define KEY_UP 64
#define KEY_DOWN 128
#define KEY_R 256
#define KEY_L 512
#define KEY_X 1024
#define KEY_Y 2048
#define KEY_TOUCH 4096
#define KEY_LID 8192

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

extern u16(*keysCallback)();

void scanKeys();

u16 keysDown();

u16 keysHeld();

u16 keysUp();

int sinLerp(int angle);
int cosLerp(int angle);
int tanLerp(int angle);
int acosLerp(int input);
int asinLerp(int input);
int sqrtf32(int input);
#endif