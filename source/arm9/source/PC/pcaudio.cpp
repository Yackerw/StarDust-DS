#include "pcaudio.h"
#include <xaudio2.h>
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>

IXAudio2* XAudioEngine;
IXAudio2MasteringVoice *masteringVoice;
IXAudio2SourceVoice* baseVoice;
void (*audioCallback)(void*, int);
HANDLE audioThreadHandle;
volatile bool killAudioThread = false;
HANDLE audioMutex;

struct StreamingVoiceContext : public IXAudio2VoiceCallback {
	HANDLE hBufferEndEvent;
	StreamingVoiceContext() : hBufferEndEvent(CreateEvent(NULL, FALSE, FALSE, NULL)) {}
	~StreamingVoiceContext() { CloseHandle(hBufferEndEvent); }
	char STREAMBUFFERS[3][2048];
	int currBuffer = 0;
	void _stdcall OnBufferEnd(void*) { 
		SetEvent(hBufferEndEvent);
	}

	void _stdcall OnVoiceProcessingPassStart(UINT32) {

	}
	void _stdcall OnVoiceProcessingPassEnd() {

	}

	void _stdcall OnStreamEnd() {

	}

	void _stdcall OnBufferStart(void*) {

	}

	void _stdcall OnLoopEnd(void*) {

	}

	void _stdcall OnVoiceError(void*, HRESULT) {

	}

};

StreamingVoiceContext* streamContext;

DWORD WINAPI AudioThreadFunction(LPVOID lparam) {
	while (!killAudioThread) {
		if (audioCallback != NULL) {
			WaitForSingleObject(audioMutex, INFINITE);
			audioCallback(streamContext->STREAMBUFFERS[streamContext->currBuffer], 2048);
			XAUDIO2_BUFFER buff = { 0 };
			buff.AudioBytes = 2048;
			buff.pAudioData = (BYTE*)streamContext->STREAMBUFFERS[streamContext->currBuffer];
			baseVoice->SubmitSourceBuffer(&buff);
			++streamContext->currBuffer;
			streamContext->currBuffer %= 3;
			ReleaseMutex(audioMutex);
			WaitForSingleObjectEx(streamContext->hBufferEndEvent, INFINITE, true);
		}

	}
	return 0;
}

bool InitializeXAudio2() {
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		printf("XAudio2 error: %i", hr);
		return false;
	}
	hr = XAudio2Create(&XAudioEngine, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		printf("XAudio2 error: %i", hr);
		return false;
	}
	hr = XAudioEngine->CreateMasteringVoice(&masteringVoice, 2, 44100, 0, NULL, NULL);
	if (FAILED(hr)) {
		printf("XAudio2 error: %i", hr);
		return false;
	}
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = 2;
	waveFormat.nSamplesPerSec = 44100;
	waveFormat.nAvgBytesPerSec = 44100 * 4;
	waveFormat.nBlockAlign = 4;
	waveFormat.wBitsPerSample = 16;
	waveFormat.cbSize = 0;
	streamContext = new StreamingVoiceContext();
	hr = XAudioEngine->CreateSourceVoice(&baseVoice, &waveFormat, 0, 2.0f, streamContext, NULL, NULL);
	if (FAILED(hr)) {
		printf("XAudio2 error: %i", hr);
		return false;
	}

	baseVoice->Start();
	// initialize buffer...
	XAUDIO2_BUFFER buff = { 0 };
	buff.AudioBytes = 1024;
	buff.pAudioData = (BYTE*)streamContext->STREAMBUFFERS[0];
	baseVoice->SubmitSourceBuffer(&buff);

	// create audio thread
	audioThreadHandle = CreateThread(NULL, 4096, AudioThreadFunction, NULL, 0, NULL);

	audioMutex = CreateMutex(NULL, false, NULL);
	return true;
}

void UninitializeXAudio2() {
	killAudioThread = true;
	WaitForSingleObject(audioThreadHandle, INFINITE);
	CloseHandle(audioThreadHandle);
	CloseHandle(audioMutex);
}