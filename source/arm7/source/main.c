#include <nds.h>
#ifndef _NOTDS
#include <dswifi7.h>
#include <maxmod7.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	int type;
	char* fileToRead;
	char* readBuffer;
	int bytesToRead;
	int loopEnd;
	int loopStart;
	FILE* file;
	int fileLength;
	bool loop;
	volatile bool done;
} AsyncReadData;

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

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

void PlaySound(SoundData* sd) {
	AudioRegister* audioRegisters = 0x04000400;
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
		fifoSendValue32(FIFO_USER_01, -1);
		return;
	}

	// register was found, employ data
	audioRegisters[currRegister].SOUNDSAD = sd->sound->samples;
	// mark bit to start playing
	unsigned int workValue = 0x80000000;

	// volume
	workValue |= mulf32(sd->volume, 127);

	// panning
	workValue |= mulf32(sd->pan, 127) << 16;
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
	audioRegisters[currRegister].SOUNDPNT = (sd->loopStart * sd->sound->bytesPerSample) / 4;
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

	fifoSendValue32(FIFO_USER_01, currRegister);
}

void StopSound(int id) {
	AudioRegister* audioRegisters = 0x04000400;
	audioRegisters[id].SOUNDCNT = 0;
	// acknowledge that it's been done
	fifoSendValue32(FIFO_USER_01, 0);
}

void SetSoundPan(FIFOAudioParameter* data) {
	AudioRegister* audioRegisters = 0x04000400;
	audioRegisters[data->id].SOUNDCNT &= !((0x7F) << 16);
	audioRegisters[data->id].SOUNDCNT |= mulf32(data->value, 127) << 16;
	// acknowledge that it's been done
	fifoSendValue32(FIFO_USER_01, 0);
}

void SetSoundVolume(FIFOAudioParameter* data) {
	AudioRegister* audioRegisters = 0x04000400;
	audioRegisters[data->id].SOUNDCNT &= !0x7F;
	audioRegisters[data->id].SOUNDCNT |= mulf32(data->value, 127);
	// acknowledge that it's been done
	fifoSendValue32(FIFO_USER_01, 0);
}

void PlayAudioCallback(void* address, void* userdata) {
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
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	touchInit();

	SetYtrigger(80);

	if (isDSiMode()) {
		installWifiFIFO();
	}
	// ??? audio stops working without this
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	setPowerButtonCB(powerButtonCB);

	fifoSetAddressHandler(FIFO_USER_01, PlayAudioCallback, NULL);

	// Keep the ARM7 mostly idle
	while (!exitflag) {
		swiWaitForVBlank();
	}
	return 0;
}
#endif