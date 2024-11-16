#include "nds.h"
#include <math.h>

#define AngleToFloat(angle) (((angle) / 16383.0f) * 3.1415926535f)
#define FloatToAngle(value) ((int)(((value) / 3.1415926535f) * 16383));

int sinLerp(int angle) {
	return floattof32(sinf(AngleToFloat(angle)));
}
int cosLerp(int angle) {
	return floattof32(cosf(AngleToFloat(angle)));
}
int tanLerp(int angle) {
	return floattof32(tanf(AngleToFloat(angle)));
}
int acosLerp(int input) {
	return FloatToAngle(acosf(f32tofloat(input)));
}
int asinLerp(int input) {
	return FloatToAngle(asinf(f32tofloat(input)));
}
int sqrtf32(int input) {
	return floattof32(sqrtf(f32tofloat(input)));
}