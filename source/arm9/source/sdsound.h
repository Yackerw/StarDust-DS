#ifndef SDSOUND
#define SDSOUND
#include <nds.h>
#include "sdmath.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
	uint32_t RIFF;
	uint32_t fileSize;
	uint32_t WAVE;
	uint32_t fmt;
	uint32_t formatDataLength;
	u16 format;
	u16 channelCount;
	uint32_t sampleRate;
	uint32_t bytesPerSec;
	u16 bytesInSamplesTimesChannels;
	u16 bitsPerSample;
	uint32_t DATA;
	uint32_t dataSize;
} WavHeader;

typedef struct {
	bool stereo;
	uint32_t sampleRate;
	int bytesPerSample;
	uint32_t dataSize;
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

int LoadWavAsync(char* input, void (*callBack)(void* data, SoundEffect* sound), void* callBackData);

void UpdateMusicBuffer();

void DestroySoundEffect(SoundEffect* sound);

int PlaySound(SoundData *sound);

void StopSoundEffect(int id);

void SetSoundPan(int id, f32 pan);

void SetSoundVolume(int id, f32 volume);

int GetMusicPosition();

void UninitializeAudio();
#endif