#include "sdsound.h"
#include "sdfile.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#ifdef _WIN32
#include "PC/pcaudio.h"
#endif
int currSoundId;
SoundEffect* currMusic;
unsigned char* mainMusic;
int currMusicId = -1;
typedef struct {
	FILE* f;
	void (*callBack)(void* data, SoundEffect* sound);
	void* callBackData;
	SoundEffect* se;
}  AsyncWavData;
#ifndef _NOTDS
#include <nds/ndstypes.h>
#include "sdmath.h"
#include <calico.h>

bool playingMusic;
PlayingSoundData currMusicData;

int sfxIds[16];

SoundData localSound;
Mutex soundMutex;

#define FIFO_SOUND_PLAY 0
#define FIFO_SOUND_STOP 1
#define FIFO_SOUND_PAN 2
#define FIFO_SOUND_VOLUME 3

typedef struct {
	int type;
	void* data;
} FIFOAudio;

typedef struct {
	int id;
	int value;
} FIFOAudioParameter;

FIFOAudio FIFOData;
FIFOAudioParameter FIFOParameters;

int prevTimerRemainder;
int prevTimerPos;
int prevMusicRemainder;

char musicBuffer[30720];
int musicBufferReadOffset = 0;
int musicBufferOffset = 0;

Mutex musicMutex;

void MusicCallback(int length, char* dest) {
	memcpy(dest, &musicBuffer[musicBufferOffset], length * currMusic->bytesPerSample);
	musicBufferOffset += length * currMusic->bytesPerSample;
	if (musicBufferOffset >= 30720) {
		musicBufferOffset -= 30720;
	}
}

void MusicRead(int length, char* dest) {
	int readSize = 1;
	if (currMusicData.soundPosition + length * readSize >= (currMusicData.loopEnd)) {
		int readDeficit = (currMusicData.loopEnd * readSize) - currMusicData.soundPosition;
		// align to two
		readDeficit = readDeficit & 0xFFFFFFFE;
		fread(dest, readDeficit * currMusic->bytesPerSample, readSize, currMusic->streamFile);
		fseek(currMusic->streamFile, (currMusicData.loopStart * currMusic->bytesPerSample) + currMusic->fOffset, SEEK_SET);
		fread(dest + readDeficit * currMusic->bytesPerSample, length - readDeficit, readSize, currMusic->streamFile);
	}
	else {
		fread(dest, length, readSize, currMusic->streamFile);
	}
	if (currMusic->bytesPerSample == 1) {
		const int len = length * readSize;
		for (int i = 0; i < len; ++i) {
			dest[i] += 128;
		}
	}
	currMusicData.soundPosition = (ftell(currMusic->streamFile) - currMusic->fOffset) / currMusic->bytesPerSample;
}

void UpdateMusicBuffer() {
	mutexLock(&musicMutex);
	int startRead = musicBufferReadOffset;
	int endRead = musicBufferOffset;
	if (startRead == endRead) {
		mutexUnlock(&musicMutex);
		return;
	}
	int lastSample = 30720;
	if (endRead > lastSample) {
		endRead -= lastSample;
	}
	// do two reads if we're overflowing
	if (endRead < startRead) {
		if (lastSample - startRead != 0) {
			MusicRead(lastSample - startRead, &musicBuffer[startRead]);
		}
		MusicRead(endRead, musicBuffer);
	}
	else {
		MusicRead(endRead - startRead, &musicBuffer[startRead]);
	}
	musicBufferReadOffset = musicBufferOffset;
	mutexUnlock(&musicMutex);
}

void MusicTimerCallback() {
	int elapsed = 1024;
	int startRead = prevTimerPos;
	int endRead = prevTimerPos + elapsed;
	int lastSample = 4096 / currMusic->bytesPerSample;
	if (endRead > lastSample) {
		endRead = endRead - lastSample;
	}
	// do two reads if we're overflowing
	if (endRead < startRead) {
		if (lastSample - startRead != 0) {
			MusicCallback(lastSample - startRead, (char*)&currMusic->samples[startRead * currMusic->bytesPerSample]);
		}
		MusicCallback(endRead, (char*)currMusic->samples);
	}
	else {
		MusicCallback(endRead - startRead, (char*)&currMusic->samples[startRead * currMusic->bytesPerSample]);
	}
	prevTimerPos = endRead;
	// finally, flush the cache
	DC_FlushRange(currMusic->samples, 4096);
}

void PlayMusic(char* filedir, int offset) {
	if (currMusic != NULL) {
		StopMusic();
	}
	PlayingSoundData sd;
	sd.pan = 2048;
	sd.pitch = 4096;
	sd.volume = 4096;
	sd.sound = currMusic;
	sd.loop = true;
	sd.soundPosition = 0;
	if (strcmp(".wav", &filedir[strlen(filedir) - 4]) == 0) {
		currMusic = LoadWavStreamed(filedir);
	}
	if (currMusic == NULL) {
		return;
	}
	sd.sound = currMusic;
	char loopFileDir[512];
	sprintf(loopFileDir, "%s%s", filedir, ".loop");
	FILE* loopFile = fopen(loopFileDir, "rb");
	if (loopFile != NULL) {
		fread(&sd.loopStart, 4, 1, loopFile);
		fread(&sd.loopEnd, 4, 1, loopFile);
		fclose(loopFile);
	}
	else {
		sd.loopStart = 0;
		fseek(currMusic->streamFile, 0, SEEK_END);
		sd.loopEnd = ftell(currMusic->streamFile) - currMusic->fOffset;

		int readSize = 1;
		if (currMusic->stereo) {
			readSize = 2;
		}
		readSize *= currMusic->bytesPerSample;
		sd.loopEnd /= readSize;

		fseek(currMusic->streamFile, 0, SEEK_SET);
	}
	fseek(currMusic->streamFile, currMusic->fOffset, SEEK_SET);
	SoundData toPlay;
	currMusicData = sd;
	currMusic->samples = malloc(4096);

	toPlay.sound = sd.sound;
	toPlay.pan = sd.pan;
	toPlay.pitch = sd.pitch;
	toPlay.volume = sd.volume;
	toPlay.loop = true;
	toPlay.loopStart = 0;
	toPlay.loopEnd = 4096 / sd.sound->bytesPerSample;

	musicBufferReadOffset = 0;
	musicBufferOffset = 0;

	currMusicData.soundPosition = offset;
	fseek(currMusic->streamFile, currMusic->fOffset + (offset * currMusic->bytesPerSample), SEEK_SET);

	MusicRead(30720, musicBuffer);

	MusicCallback(4096 / sd.sound->bytesPerSample, (char*)sd.sound->samples);
	DC_FlushRange(currMusic->samples, 4096);

	prevTimerPos = 0;
	prevTimerRemainder = 0;
	prevMusicRemainder = 0;

	int tmp = currMusic->dataSize;
	currMusic->dataSize = 4096;

	DC_FlushRange(currMusic, sizeof(SoundEffect));

	currMusicId = PlaySound(&toPlay);
	currMusic->dataSize = tmp;
	if (currMusicId == -1) {
		StopMusic();
		playingMusic = false;
		return;
	}
	// start a timer to trigger every 1024 samples
	timerStart(0, ClockDivider_1024, 65536 - 0x2000000 / (0x1000000 / (0x1000000 / sd.sound->sampleRate)), MusicTimerCallback);
	playingMusic = true;
}

void InitSound() {
	
}

void StopMusic() {
	if (currMusic != NULL) {
		// this doesn't get freed by the shared code...
		free(currMusic->samples);
		DestroySoundEffect(currMusic);
	}
	timerStop(0);
	StopSoundEffect(currMusicId);
	playingMusic = false;
	currMusic = NULL;
}

void UninitializeAudio() {
	soundDisable();
}


int PlaySound(SoundData* sound) {
	mutexLock(&soundMutex);
	FIFOData.type = FIFO_SOUND_PLAY;
	localSound = (*sound);
	FIFOData.data = &localSound;
	DC_FlushRange(&FIFOData, sizeof(FIFOAudio));
	DC_FlushRange(&localSound, sizeof(SoundData));
	int soundValue = pxiSendAndReceive(PxiChannel_User0, (unsigned int)&FIFOData);
	mutexUnlock(&soundMutex);
	if (soundValue == -1) {
		return -1;
	}
	sfxIds[soundValue] = currSoundId;
	return currSoundId++;
}

int GetChannel(int id) {
	// get channel it's playing on
	int currChannel = -1;
	for (int i = 0; i < 16; ++i) {
		if (sfxIds[i] == id) {
			currChannel = i;
			break;
		}
	}
	return currChannel;
}

void StopSoundEffect(int id) {
	int currChannel = GetChannel(id);

	// build FIFO message
	FIFOData.type = FIFO_SOUND_STOP;
	FIFOData.data = (void*)currChannel;
	DC_FlushRange(&FIFOData, sizeof(FIFOAudio));
	pxiSendAndReceive(PxiChannel_User0, (unsigned int)&FIFOData);
}

void SetSoundPan(int id, f32 pan) {
	int currChannel = GetChannel(id);

	if (currChannel == -1) {
		return;
	}

	// build FIFO message
	FIFOData.type = FIFO_SOUND_PAN;
	FIFOData.data = (void*)&FIFOParameters;
	DC_FlushRange(&FIFOData, sizeof(FIFOAudio));
	FIFOParameters.id = currChannel;
	FIFOParameters.value = pan;
	DC_FlushRange(&FIFOParameters, sizeof(FIFOAudioParameter));
	pxiSendAndReceive(PxiChannel_User0, (unsigned int)&FIFOData);
}

void SetSoundVolume(int id, f32 volume) {
	int currChannel = GetChannel(id);

	if (currChannel == -1) {
		return;
	}

	// build FIFO message
	FIFOData.type = FIFO_SOUND_VOLUME;
	FIFOData.data = (void*)&FIFOParameters;
	DC_FlushRange(&FIFOData, sizeof(FIFOAudio));
	FIFOParameters.id = currChannel;
	FIFOParameters.value = volume;
	DC_FlushRange(&FIFOParameters, sizeof(FIFOAudioParameter));
	pxiSendAndReceive(PxiChannel_User0, (unsigned int)&FIFOData);
}

int GetMusicPosition() {
	if (!playingMusic) {
		return -1;
	}
	// todo: make this better ig
	int retValue = (ftell(currMusic->streamFile) - currMusic->fOffset - 30720);
	if (retValue < 0) {
		retValue = currMusicData.loopEnd + retValue;
	}
	return retValue / currMusic->bytesPerSample;
}
#endif

SoundEffect* LoadWav(char* input) {
	char* fileName = DirToNative(input);
	FILE* f = fopen(fileName, "rb");
	free(fileName);
	if (f == NULL) {
		return NULL;
	}
	WavHeader wavHeader;
	fread_MusicYielding(&wavHeader, sizeof(wavHeader), 1, f);
	SoundEffect* audioInfo = (SoundEffect*)malloc(sizeof(SoundEffect));
	audioInfo->sampleRate = wavHeader.sampleRate;
	audioInfo->dataSize = wavHeader.dataSize;
	audioInfo->stereo = wavHeader.channelCount == 2;
	audioInfo->bytesPerSample = wavHeader.bitsPerSample / 8;
	audioInfo->samples = (unsigned char*)malloc(wavHeader.dataSize);
	fread_MusicYielding(audioInfo->samples, wavHeader.dataSize, 1, f);
	fclose(f);
#ifndef _NOTDS
	// convert unsigned 8 bit to signed 8 bit
	if (audioInfo->bytesPerSample == 1) {
		for (int i = 0; i < audioInfo->dataSize; ++i) {
			audioInfo->samples[i] += 128;
		}
	}
	DC_FlushRange(audioInfo->samples, wavHeader.dataSize);
#endif
	audioInfo->streamed = false;
	return audioInfo;
}

SoundEffect* LoadWavStreamed(char* input) {
	char* fileName = DirToNative(input);
	FILE* f = fopen(fileName, "rb");
	free(fileName);
	if (f == NULL) {
		return NULL;
	}
	WavHeader wavHeader;
	fread_MusicYielding(&wavHeader, sizeof(wavHeader), 1, f);
	SoundEffect* audioInfo = (SoundEffect*)malloc(sizeof(SoundEffect));
	audioInfo->sampleRate = wavHeader.sampleRate;
	audioInfo->dataSize = wavHeader.dataSize;
	audioInfo->stereo = wavHeader.channelCount == 2;
	audioInfo->bytesPerSample = wavHeader.bitsPerSample / 8;
	audioInfo->streamFile = f;
	audioInfo->fOffset = sizeof(wavHeader);
	audioInfo->streamed = true;
	return audioInfo;
}

void DestroySoundEffect(SoundEffect* sound) {
	if (!sound->streamed) {
		free(sound->samples);
	}
	free(sound);
}

void LoadWavAsyncCallback(void* data, bool success) {
	AsyncWavData* awd = (AsyncWavData*)data;
	fclose(awd->f);
	if (!success) {
		free(awd->se->samples);
		free(awd->se);
		awd->se = NULL;
	}
	awd->callBack(awd->callBackData, awd->se);
	free(awd);
}

int LoadWavAsync(char* input, void (*callBack)(void* data, SoundEffect* sound), void* callBackData) {
	if (callBack == NULL) {
		// ?
		return -1;
	}
	char* fileName = DirToNative(input);
	FILE* f = fopen(fileName, "rb");
	free(fileName);
	if (f == NULL) {
		callBack(callBackData, NULL);
		return -1;
	}

	WavHeader wavHeader;
	fread_MusicYielding(&wavHeader, sizeof(wavHeader), 1, f);
	SoundEffect* audioInfo = (SoundEffect*)malloc(sizeof(SoundEffect));
	audioInfo->sampleRate = wavHeader.sampleRate;
	audioInfo->dataSize = wavHeader.dataSize;
	audioInfo->stereo = wavHeader.channelCount == 2;
	audioInfo->bytesPerSample = wavHeader.bitsPerSample / 8;
	audioInfo->samples = (unsigned char*)malloc(wavHeader.dataSize);

	AsyncWavData* awd = (AsyncWavData*)malloc(sizeof(AsyncWavData));
	awd->f = f;
	awd->callBack = callBack;
	awd->callBackData = callBackData;
	awd->se = audioInfo;

	return fread_Async(audioInfo->samples, wavHeader.dataSize, 1, f, 0, LoadWavAsyncCallback, awd);
}

#ifdef _WIN32
PlayingSoundData firstSound;

void StopSoundEffectInternal(int id) {
	PlayingSoundData* currPSD = firstSound.next;
	while (currPSD != NULL) {
		if (currPSD->id == id) {
			currPSD->prev->next = currPSD->next;
			if (currPSD->next != NULL) {
				currPSD->next->prev = currPSD->prev;
			}
			free(currPSD);
			return;
		}
		currPSD = currPSD->next;
	}
}

#define Maxf(x, y) ((x) < (y) ? (y) : (x))
#define Minf(x, y) ((x) > (y) ? (y) : (x))

void MixByte(PlayingSoundData* currSound, short* output, const int samplesToWrite) {
	const bool stereo = currSound->sound->stereo;
	float pan = f32tofloat(currSound->pan) * 2.0f - 1.0f;
	const float leftVolume = f32tofloat(currSound->volume) * (1.0f - Maxf(pan, 0));
	const float rightVolume = f32tofloat(currSound->volume) * (1.0f + Minf(pan, 0));
	const int dataSize = currSound->sound->dataSize;

	const float incrementor = (currSound->sound->sampleRate / 44100.0f) * f32tofloat(currSound->pitch);

	for (int i = 0; i < samplesToWrite; i += 2) {
		if (currSound->soundPosition >= dataSize) {
			if (!currSound->loop) {
				StopSoundEffectInternal(currSound->id);
				break;
			}
			else {
				currSound->soundPosition = currSound->loopStart * stereo ? 2 : 1;
			}
		}
		if (stereo) {
			output[i] = iClamp(output[i] + (((127 - currSound->sound->samples[(((int)currSound->soundPosition) / 2) * 2]) * 256) * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (((127 - currSound->sound->samples[(((int)currSound->soundPosition) / 2) * 2 + 1]) * 256) * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor + incrementor;
			if (currSound->soundPosition >= currSound->loopEnd * 2 && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart * 2;
			}
		}
		else {
			output[i] = iClamp(output[i] + (((127 - currSound->sound->samples[(int)currSound->soundPosition]) * 256) * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (((127 - currSound->sound->samples[(int)currSound->soundPosition]) * 256) * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor;
			if (currSound->soundPosition >= currSound->loopEnd && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart;
			}
		}
	}
}

void MixShort(PlayingSoundData* currSound, short* output, const int samplesToWrite) {
	const bool stereo = currSound->sound->stereo;
	float pan = f32tofloat(currSound->pan) * 2.0f - 1.0f;
	const float leftVolume = f32tofloat(currSound->volume) * (1.0f - Maxf(pan, 0));
	const float rightVolume = f32tofloat(currSound->volume) * (1.0f + Minf(pan, 0));
	const int dataSizeDiv2 = currSound->sound->dataSize / 2;

	const float incrementor = (currSound->sound->sampleRate / 44100.0f) * f32tofloat(currSound->pitch);

	for (int i = 0; i < samplesToWrite; i += 2) {
		if (currSound->soundPosition >= dataSizeDiv2) {
			if (!currSound->loop) {
				StopSoundEffectInternal(currSound->id);
				break;
			}
			else {
				currSound->soundPosition = currSound->loopStart * stereo ? 2 : 1;
			}
		}
		if (stereo) {
			output[i] = iClamp(output[i] + (currSound->sound->shortSamples[(((int)currSound->soundPosition) / 2) * 2] * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (currSound->sound->shortSamples[(((int)currSound->soundPosition) / 2) * 2 + 1] * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor + incrementor;
			if (currSound->soundPosition >= currSound->loopEnd * 2 && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart * 2;
			}
		}
		else {
			output[i] = iClamp(output[i] + (currSound->sound->shortSamples[(int)currSound->soundPosition] * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (currSound->sound->shortSamples[(int)currSound->soundPosition] * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor;
			if (currSound->soundPosition >= currSound->loopEnd && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart;
			}
		}
	}
}

void MixByteStreamed(PlayingSoundData* currSound, short* output, const int samplesToWrite) {
	const bool stereo = currSound->sound->stereo;
	float pan = f32tofloat(currSound->pan) * 2.0f - 1.0f;
	const float leftVolume = f32tofloat(currSound->volume) * (1.0f - Maxf(pan, 0));
	const float rightVolume = f32tofloat(currSound->volume) * (1.0f + Minf(pan, 0));
	const int dataSize = currSound->sound->dataSize;

	const float incrementor = (currSound->sound->sampleRate / 44100.0f) * f32tofloat(currSound->pitch);

	for (int i = 0; i < samplesToWrite; i += 2) {
		if (currSound->soundPosition >= dataSize) {
			if (!currSound->loop) {
				StopSoundEffectInternal(currSound->id);
				break;
			}
			else {
				currSound->soundPosition = currSound->loopStart * stereo ? 2 : 1;
			}
		}
		if (stereo) {
			fseek(currSound->sound->streamFile, currSound->soundPosition + currSound->sound->fOffset, SEEK_SET);
			unsigned char inputAudio[2];
			fread(inputAudio, 2, 1, currSound->sound->streamFile);
			output[i] = iClamp(output[i] + (((127 - inputAudio[0]) * 256) * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (((127 - inputAudio[1]) * 256) * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor + incrementor;
			if (currSound->soundPosition >= currSound->loopEnd * 2 && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart * 2;
			}
		}
		else {
			fseek(currSound->sound->streamFile, currSound->soundPosition + currSound->sound->fOffset, SEEK_SET);
			unsigned char inputAudio[2];
			fread(inputAudio, 1, 1, currSound->sound->streamFile);
			output[i] = iClamp(output[i] + (((127 - inputAudio[0]) * 256) * leftVolume), -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + (((127 - inputAudio[0]) * 256) * rightVolume), -32768, 32767);
			currSound->soundPosition += incrementor;
			if (currSound->soundPosition >= currSound->loopEnd && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart;
			}
		}
	}
}

void MixShortStreamed(PlayingSoundData* currSound, short* output, const int samplesToWrite) {
	const bool stereo = currSound->sound->stereo;
	float pan = f32tofloat(currSound->pan) * 2.0f - 1.0f;
	const float leftVolume = f32tofloat(currSound->volume) * (1.0f - Maxf(pan, 0));
	const float rightVolume = f32tofloat(currSound->volume) * (1.0f + Minf(pan, 0));
	const int dataSizeDiv2 = currSound->sound->dataSize / 2;

	const float incrementor = (currSound->sound->sampleRate / 44100.0f) * f32tofloat(currSound->pitch);

	for (int i = 0; i < samplesToWrite; i += 2) {
		if (currSound->soundPosition >= dataSizeDiv2) {
			if (!currSound->loop) {
				StopSoundEffectInternal(currSound->id);
				break;
			}
			else {
				currSound->soundPosition = currSound->loopStart * stereo ? 2 : 1;
			}
		}
		if (stereo) {
			fseek(currSound->sound->streamFile, currSound->soundPosition * 2 + currSound->sound->fOffset, SEEK_SET);
			short inputAudio[2];
			fread(inputAudio, 4, 1, currSound->sound->streamFile);
			output[i] = iClamp(output[i] + inputAudio[0] * leftVolume, -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + 127 - inputAudio[1] * rightVolume, -32768, 32767);
			currSound->soundPosition += incrementor + incrementor;
			if (currSound->soundPosition >= currSound->loopEnd * 2 && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart * 2;
			}
		}
		else {
			fseek(currSound->sound->streamFile, currSound->soundPosition * 2 + currSound->sound->fOffset, SEEK_SET);
			short inputAudio[2];
			fread(inputAudio, 2, 1, currSound->sound->streamFile);
			output[i] = iClamp(output[i] + inputAudio[0] * leftVolume, -32768, 32767);
			output[i + 1] = iClamp(output[i + 1] + inputAudio[0] * rightVolume, -32768, 32767);
			currSound->soundPosition += incrementor;
			if (currSound->soundPosition >= currSound->loopEnd && currSound->loopEnd != 0) {
				currSound->soundPosition = currSound->loopStart;
			}
		}
	}
}

void AudioMixer(void* output, int bytesToWrite) {
	memset(output, 0, bytesToWrite);
	short* shoutput = (short*)output;
	PlayingSoundData* currSound = firstSound.next;
	const int shortsToWrite = bytesToWrite / 2;
	while (currSound != NULL) {
		PlayingSoundData* nextSound = currSound->next;
		if (!currSound->sound->streamed) {
			switch (currSound->sound->bytesPerSample) {
			case 1:
				MixByte(currSound, shoutput, shortsToWrite);
				break;
			case 2:
				MixShort(currSound, shoutput, shortsToWrite);
				break;
			}
		}
		else {
			switch (currSound->sound->bytesPerSample) {
			case 1:
				MixByteStreamed(currSound, shoutput, shortsToWrite);
				break;
			case 2:
				MixShortStreamed(currSound, shoutput, shortsToWrite);
				break;
			}
		}
		currSound = nextSound;
	}
}

void InitSound() {
	InitializeXAudio2();
	audioCallback = AudioMixer;
}

int PlaySoundOffset(SoundData* sound, int offset) {
	WaitForSingleObject(audioMutex, INFINITE);
	PlayingSoundData* newSound = (PlayingSoundData*)malloc(sizeof(PlayingSoundData));
	newSound->loop = sound->loop;
	newSound->pitch = sound->pitch;
	newSound->pan = sound->pan;
	newSound->sound = sound->sound;
	newSound->next = firstSound.next;
	newSound->prev = &firstSound;
	newSound->volume = sound->volume;
	newSound->soundPosition = offset;
	firstSound.next = newSound;
	newSound->id = currSoundId;
	newSound->loopStart = sound->loopStart;
	newSound->loopEnd = sound->loopEnd;
	newSound->realSoundPosition = 0;
	++currSoundId;

	ReleaseMutex(audioMutex);

	return currSoundId - 1;
}

int PlaySound(SoundData* sound) {
	return PlaySoundOffset(sound, 0);
}

void StopSoundEffect(int id) {
	WaitForSingleObject(audioMutex, INFINITE);
	PlayingSoundData* currPSD = firstSound.next;
	while (currPSD != NULL) {
		if (currPSD->id == id) {
			currPSD->prev->next = currPSD->next;
			if (currPSD->next != NULL) {
				currPSD->next->prev = currPSD->prev;
			}
			free(currPSD);
			ReleaseMutex(audioMutex);
			return;
		}
		currPSD = currPSD->next;
	}
	ReleaseMutex(audioMutex);
}

void PlayMusic(char* filedir, int offset) {
	if (currMusic != NULL) {
		StopMusic();
	}
	SoundData sd;
	sd.pan = 2048;
	sd.pitch = 4096;
	sd.volume = 4096;
	sd.sound = currMusic;
	sd.loop = true;
	if (strcmp(".wav", &filedir[strlen(filedir) - 4]) == 0) {
		currMusic = LoadWavStreamed(filedir);
	}
	else {
		return;
	}
	sd.sound = currMusic;
	if (currMusic == NULL) {
		return;
	}
	char loopFileDir[512];
	sprintf(loopFileDir, "%s%s", filedir, ".loop");
	char* loopFileDirNative = DirToNative(loopFileDir);
	FILE* loopFile = fopen(loopFileDirNative, "rb");
	free(loopFileDirNative);
	if (loopFile != NULL) {
		fread(&sd.loopStart, 4, 1, loopFile);
		fread(&sd.loopEnd, 4, 1, loopFile);
		fclose(loopFile);
	}
	else {
		sd.loopStart = 0;
		sd.loopEnd = 0;
	}
	currMusicId = PlaySoundOffset(&sd, offset);
}

void StopMusic() {
	StopSoundEffect(currMusicId);
	if (currMusic != NULL) {
		DestroySoundEffect(currMusic);
	}
	currMusic = NULL;
}

void UninitializeAudio() {
	UninitializeXAudio2();
}

void UpdateMusicBuffer() {
	// teehee, we do nothing on PC!
}

PlayingSoundData* FindSoundByID(int id) {
	PlayingSoundData* currPSD = firstSound.next;
	while (currPSD != NULL) {
		if (currPSD->id == id) {
			return currPSD;
		}
		currPSD = currPSD->next;
	}
	return NULL;
}

void SetSoundPan(int id, f32 pan) {
	PlayingSoundData* psd = FindSoundByID(id);
	if (psd == NULL) {
		return;
	}
	psd->pan = pan;
}

void SetSoundVolume(int id, f32 volume) {
	PlayingSoundData* psd = FindSoundByID(id);
	if (psd == NULL) {
		return;
	}
	psd->volume = volume;
}

int GetMusicPosition() {
	PlayingSoundData* psd = FindSoundByID(currMusicId);
	if (psd == NULL) {
		return -1;
	}
	return psd->soundPosition;
}

#endif