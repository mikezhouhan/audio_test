#pragma once

#include <string>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include "WASAPICapture.h"

// Helper function to save wave data to a file
bool WriteWaveFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize, const WAVEFORMATEX* WaveFormat);

// Function to save the captured audio data as a WAV file
void SaveWaveData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat, std::string fileName);

// Function to set up and initialize the audio capture device
void SetupAudioCapture(IMMDeviceEnumerator*& pEnumerator, IMMDevice*& pDevice);

// Generate a timestamp string for unique filenames
std::string GetTimestampString(); 