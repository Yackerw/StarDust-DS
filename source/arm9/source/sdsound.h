#ifndef SDSOUND
#define SDSOUND
#include <nds.h>
#include "sdmath.h"
#include <stdio.h>

typedef struct {
	uint RIFF;
	uint fileSize;
	uint WAVE;
	uint fmt;
	uint formatDataLength;
	u16 format;
	u16 channelCount;
	uint sampleRate;
	uint bytesPerSec;
	u16 bytesInSamplesTimesChannels;
	u16 bitsPerSample;
	uint DATA;
	uint dataSize;
} WavHeader;

typedef struct {
	bool stereo;
	uint sampleRate;
	int bytesPerSample;
	uint dataSize;
	bool streamed;
	int fOffset;
	union {
		unsigned char* samples;
		short* shortSamples;
	};
	FILE* streamFile;
} SoundEffect;

typedef struct {
	f32 pan;
	f32 volume;
	f32 pitch;
	bool loop;
	unsigned int loopEnd;
	unsigned int loopStart;
	SoundEffect* sound;
} SoundData;

typedef struct PlayingSoundData PlayingSoundData;

struct PlayingSoundData {
	f32 pan;
	f32 volume;
	f32 pitch;
	SoundEffect* sound;
	bool loop;
	float soundPosition;
	float realSoundPosition;
	int id;
	unsigned int loopEnd;
	unsigned int loopStart;
	unsigned short cachedSamples[2];
	PlayingSoundData* prev;
	PlayingSoundData* next;
};

extern unsigned int mainMusicPosition;

void PlayMusic(char *filedir, int offset);

void StopMusic();

void InitSound();

SoundEffect *LoadWav(char* input);

SoundEffect* LoadWavStreamed(char* input);

void UpdateMusicBuffer();

void DestroySoundEffect(SoundEffect* sound);

int PlaySound(SoundData *sound);

void StopSoundEffect(int id);

void SetSoundPan(int id, f32 pan);

void SetSoundVolume(int id, f32 volume);

int GetMusicPosition();

void UninitializeAudio();
#endif