#include <nds.h>
#ifndef _NOTDS
#include <calico.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define f32 int

#define SOUND_FREQ(n)	((-0x1000000 / (n)))

int mulf32(int left, int right) {
	return (left * right) >> 12;
}

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

typedef struct {
	int type;
	void* data;
} FIFOAudio;

typedef struct {
	int id;
	int value;
} FIFOAudioParameter;

typedef struct {
	volatile unsigned int SOUNDCNT;
	volatile unsigned int SOUNDSAD;
	volatile unsigned short SOUNDTMR;
	volatile unsigned short SOUNDPNT;
	volatile unsigned int SOUNDLEN;
} AudioRegister;

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

void PlaySound(SoundData* sd) {
	AudioRegister* audioRegisters = (AudioRegister*)0x04000400;
	int currRegister = 0;
	// get free audio register
	for (int i = 0; i < 16; ++i) {
		if (audioRegisters[i].SOUNDCNT & 0x80000000) {
			// failed to find free register...
			if (i == 15) {
				currRegister = -1;
				break;
			}
			continue;
		}
		currRegister = i;
		break;
	}

	// bail out early, no register was found
	if (currRegister == -1) {
		pxiReply(PxiChannel_User0, -1);
		return;
	}

	// register was found, employ data
	audioRegisters[currRegister].SOUNDSAD = (unsigned int)sd->sound->samples;
	// mark bit to start playing
	unsigned int workValue = 0x80000000;

	// volume
	workValue |= (mulf32(sd->volume, 127) & 0x7F);

	// panning
	workValue |= (mulf32(sd->pan, 127) & 0x7F) << 16;
	// loop
	if (sd->loop) {
		workValue |= 1 << 27;
	}
	else {
		workValue |= 2 << 27;
	}
	// format
	workValue |= (sd->sound->bytesPerSample - 1) << 29;

	// loop point
	if (sd->loop) {
		audioRegisters[currRegister].SOUNDPNT = (sd->loopStart * sd->sound->bytesPerSample) / 4;
	} else {
		audioRegisters[currRegister].SOUNDPNT = 0;
	}
	// audio length
	if (sd->loop && sd->loopEnd > 0) {
		int realLoopEnd = sd->loopEnd * sd->sound->bytesPerSample;
		audioRegisters[currRegister].SOUNDLEN = (sd->sound->dataSize < realLoopEnd ? sd->sound->dataSize : realLoopEnd) / 4;
	}
	else {
		audioRegisters[currRegister].SOUNDLEN = sd->sound->dataSize / 4;
	}

	// frequency maybe?
	audioRegisters[currRegister].SOUNDTMR = SOUND_FREQ((int)sd->sound->sampleRate);

	// aaand go!
	audioRegisters[currRegister].SOUNDCNT = workValue;

	pxiReply(PxiChannel_User0, currRegister);
}

void StopSound(int id) {
	AudioRegister* audioRegisters = (AudioRegister*)0x04000400;
	audioRegisters[id].SOUNDCNT = 0;
	// acknowledge that it's been done
	pxiReply(PxiChannel_User0, 0);
}

void SetSoundPan(FIFOAudioParameter* data) {
	AudioRegister* audioRegisters = (AudioRegister*)0x04000400;
	audioRegisters[data->id].SOUNDCNT &= !((0x7F) << 16);
	audioRegisters[data->id].SOUNDCNT |= mulf32(data->value, 127) << 16;
	// acknowledge that it's been done
	pxiReply(PxiChannel_User0, 0);
}

void SetSoundVolume(FIFOAudioParameter* data) {
	AudioRegister* audioRegisters = (AudioRegister*)0x04000400;
	audioRegisters[data->id].SOUNDCNT &= !0x7F;
	audioRegisters[data->id].SOUNDCNT |= mulf32(data->value, 127);
	// acknowledge that it's been done
	pxiReply(PxiChannel_User0, 0);
}

void PlayAudioCallback(void* userdata, long unsigned int address) {
	FIFOAudio* data = (FIFOAudio*)address;
	switch (data->type) {
	case 0:
	{
		PlaySound(data->data);
	}
	break;
	case 1: {
		StopSound((int)data->data);
	}
		break;
	case 2: {
		SetSoundPan(data->data);
	}
		  break;
	case 3: {
		SetSoundVolume(data->data);
	}
		  break;
	}
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	// Read settings from NVRAM
	envReadNvramSettings();

	// Set up extended keypad server (X/Y/hinge)
	keypadStartExtServer();

	// Configure and enable VBlank interrupt
	lcdSetIrqMask(DISPSTAT_IE_ALL, DISPSTAT_IE_VBLANK);
	irqEnable(IRQ_VBLANK);

	// Set up RTC
	rtcInit();
	rtcSyncTime();

	// Initialize power management
	pmInit();

	// Set up block device peripherals
	blkInit();

	// Set up touch screen driver
	touchInit();
	touchStartServer(80, MAIN_THREAD_PRIO);

	// Set up sound and mic driver
	soundStartServer(MAIN_THREAD_PRIO-0x10);
	micStartServer(MAIN_THREAD_PRIO-0x18);

	// Set up wireless manager
	wlmgrStartServer(MAIN_THREAD_PRIO-8);

	pxiSetHandler(PxiChannel_User0, PlayAudioCallback, NULL);

	// Keep the ARM7 mostly idle
	while (pmMainLoop()) {
		threadWaitForVBlank();
	}
	return 0;
}
#endif