#pragma once

#include <string>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include "WASAPICapture.h"

// Helper function to write PCM data to a file
bool WritePcmFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize);

// Function to save PCM audio data to a file
void SaveAudioData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat, std::string fileName);

// Function to append PCM audio data to an existing file
bool AppendAudioData(HANDLE FileHandle, BYTE* CaptureBuffer, size_t BufferSize);

// Function to set up and initialize the audio capture device
void SetupAudioCapture(IMMDeviceEnumerator*& pEnumerator, IMMDevice*& pDevice);

// Generate a timestamp string for unique filenames
std::string GetTimestampString();

// Print audio format parameters in JSON format
void PrintAudioParameters(const WAVEFORMATEX* WaveFormat); 