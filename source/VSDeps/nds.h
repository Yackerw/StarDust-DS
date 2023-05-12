#ifndef __NDS_H
#define __NDS_H
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#define uint unsigned int

#define _NOTDS

#define sinLerp sinf
#define cosLerp cosf
#define tanLerp tanf
#define acosLerp acosf
#define asinLerp asinf

#define f32tofloat(value) value
#define floattof32(value) value
#define sqrtf32 sqrtf

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

#define mulf32(left, right) ((left)*(right))

#define divf32(left, right) ((left)/(right))

#define RGB15(r, g, b) (r & 0x1F) | ((g & 0x1F) << 5) | ((b & 0x1F) << 10)
typedef struct {
	float m[16];
} m4x4;

typedef short t16;
typedef short v16;
typedef unsigned short u16;

extern u16(*keysCallback)();

void scanKeys();

u16 keysDown();

u16 keysHeld();

u16 keysUp();
#endif