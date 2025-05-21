#include "stdafx.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <ctime>
#include <atlstr.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include "WASAPICapture.h"
#include "audio_capture_cli.h"

// Declare this variable because it's used in WASAPICapture.cpp
bool DisableMMCSS = false;

#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

// Print audio format parameters
void PrintAudioParameters(const WAVEFORMATEX* WaveFormat)
{
    printf("{\n");
    printf("  \"formatTag\": %u,\n", WaveFormat->wFormatTag);
    printf("  \"channels\": %u,\n", WaveFormat->nChannels);
    printf("  \"samplesPerSec\": %u,\n", WaveFormat->nSamplesPerSec);
    printf("  \"avgBytesPerSec\": %u,\n", WaveFormat->nAvgBytesPerSec);
    printf("  \"blockAlign\": %u,\n", WaveFormat->nBlockAlign);
    printf("  \"bitsPerSample\": %u,\n", WaveFormat->wBitsPerSample);
    printf("  \"extraSize\": %u\n", WaveFormat->cbSize);
    printf("}\n");
}

// Helper function to save PCM data
bool WritePcmFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize)
{
    std::cout << "Writing PCM file. Buffer size: " << BufferSize << " bytes\n";

    // Write raw PCM data directly
    DWORD bytesWritten;
    if (!WriteFile(FileHandle, Buffer, static_cast<DWORD>(BufferSize), &bytesWritten, NULL))
    {
        printf("Unable to write PCM file: %d\n", GetLastError());
        return false;
    }

    if (bytesWritten != BufferSize)
    {
        printf("Failed to write entire PCM file\n");
        return false;
    }
    
    return true;
}

// Function to save captured audio data to a PCM file
void SaveAudioData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat, std::string fileName)
{
    // No longer print audio parameters here since we do it at startup
    
    printf("Saving to filename: %s.pcm\n", fileName.c_str());
    
    HANDLE pcmFile = CreateFileA(
        (fileName + ".pcm").c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (pcmFile == INVALID_HANDLE_VALUE)
    {
        printf("Unable to open output PCM file: %d\n", GetLastError());
        return;
    }

    if (WritePcmFile(pcmFile, CaptureBuffer, BufferSize))
    {
        printf("Successfully wrote PCM data to %s.pcm\n", fileName.c_str());
    }
    else
    {
        printf("Failed to write PCM file\n");
    }
    
    CloseHandle(pcmFile);
}

// Function to set up audio capture device
void SetupAudioCapture(IMMDeviceEnumerator*& pEnumerator, IMMDevice*& pDevice)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        printf("Failed to initialize COM: %x\n", hr);
        return;
    }

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );
    
    if (FAILED(hr))
    {
        printf("Failed to create device enumerator: %x\n", hr);
        return;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &pDevice
    );

    if (FAILED(hr))
    {
        printf("Failed to get default audio endpoint: %x\n", hr);
        return;
    }
    
    printf("Audio device setup successful\n");
}

// Generate a timestamp string for filename
std::string GetTimestampString()
{
    time_t now = time(0);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    char buffer[80];
    strftime(buffer, 80, "%Y%m%d_%H%M%S", &timeinfo);
    
    return std::string(buffer);
}

// Check if a command line argument exists
bool HasCommandLineArg(int argc, char* argv[], const std::string& arg)
{
    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == arg)
        {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    // Print welcome message
    printf("Simple Audio Capture Tool (Based on WASAPI)\n");
    printf("------------------------------------------\n\n");
    
    // Get capture duration from user
    int durationInSeconds = 0;
    printf("Enter capture duration in seconds: ");
    std::cin >> durationInSeconds;
    
    if (durationInSeconds <= 0)
    {
        printf("Invalid duration. Please enter a positive number.\n");
        return 1;
    }
    
    // Setup COM and audio device
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    
    SetupAudioCapture(pEnumerator, pDevice);
    if (!pDevice)
    {
        printf("Failed to set up audio device.\n");
        return 1;
    }
    
    // Create output filename with timestamp
    std::string outputFilename = "audio_capture_" + GetTimestampString();
    printf("Will save to: %s.pcm\n", outputFilename.c_str());
    
    // Create and initialize the capturer
    CWASAPICapture* capturer = new CWASAPICapture(pDevice, true, eConsole);
    if (!capturer)
    {
        printf("Failed to create audio capturer.\n");
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    int targetLatency = 10; // in milliseconds
    if (!capturer->Initialize(targetLatency))
    {
        printf("Failed to initialize audio capturer.\n");
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }

    // Print audio parameters in JSON format immediately after initialization
    printf("Audio parameters:\n");
    PrintAudioParameters(capturer->MixFormat());
    
    // Calculate buffer size based on duration and audio format
    int durationInMs = durationInSeconds * 1000;
    size_t captureBufferSize = capturer->SamplesPerSecond() * durationInSeconds * capturer->FrameSize();
    BYTE* captureBuffer = new BYTE[captureBufferSize];
    
    if (!captureBuffer)
    {
        printf("Failed to allocate capture buffer.\n");
        capturer->Shutdown();
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    // Start capturing
    printf("Starting audio capture for %d seconds...\n", durationInSeconds);
    if (!capturer->Start(captureBuffer, captureBufferSize))
    {
        printf("Failed to start audio capture.\n");
        delete[] captureBuffer;
        capturer->Shutdown();
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    // Wait for the specified duration
    for (int i = 1; i <= durationInSeconds; i++)
    {
        Sleep(1000);
        printf("\rCapturing: %d/%d seconds", i, durationInSeconds);
    }
    printf("\nCapture complete.\n");
    
    // Stop capturing
    capturer->Stop();
    
    // Save the captured audio
    printf("Saving captured audio to %s.pcm...\n", outputFilename.c_str());
    SaveAudioData(captureBuffer, capturer->BytesCaptured(), capturer->MixFormat(), outputFilename);
    
    // Clean up
    delete[] captureBuffer;
    capturer->Shutdown();
    capturer->Release();
    capturer = NULL;
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
    
    printf("Audio saved successfully. Program complete.\n");
    return 0;
} 