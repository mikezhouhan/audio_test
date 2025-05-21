#include "stdafx.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <ctime>
#include <atlstr.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <csignal>
#include "WASAPICapture.h"
#include "audio_capture_cli.h"

// Declare this variable because it's used in WASAPICapture.cpp
bool DisableMMCSS = false;

// Global flag for handling Ctrl+C signal
volatile bool g_running = true;

// Signal handler for Ctrl+C
void signalHandler(int signal) {
    if (signal == SIGINT) {
        fprintf(stderr, "\nCtrl+C pressed. Stopping recording...\n");
        g_running = false;
    }
}

#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

// Print audio format parameters to stdout in single line format
void PrintAudioParameters(const WAVEFORMATEX* WaveFormat)
{
    // Output single line JSON to stdout with prefix for easier parsing
    printf("Audio parameters: {\"formatTag\":%u,\"channels\":%u,\"samplesPerSec\":%u,\"avgBytesPerSec\":%u,\"blockAlign\":%u,\"bitsPerSample\":%u,\"extraSize\":%u}\n", 
        WaveFormat->wFormatTag,
        WaveFormat->nChannels,
        WaveFormat->nSamplesPerSec, 
        WaveFormat->nAvgBytesPerSec,
        WaveFormat->nBlockAlign,
        WaveFormat->wBitsPerSample,
        WaveFormat->cbSize);
    fflush(stdout); // Ensure JSON data is immediately sent to stdout
}

// Helper function to save PCM data
bool WritePcmFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize)
{
    fprintf(stderr, "Writing PCM data. Buffer size: %zu bytes\n", BufferSize);

    // Write raw PCM data directly
    DWORD bytesWritten;
    if (!WriteFile(FileHandle, Buffer, static_cast<DWORD>(BufferSize), &bytesWritten, NULL))
    {
        fprintf(stderr, "Unable to write PCM data: %d\n", GetLastError());
        return false;
    }

    if (bytesWritten != BufferSize)
    {
        fprintf(stderr, "Failed to write entire PCM data\n");
        return false;
    }
    
    return true;
}

// Function to save captured audio data to a PCM file
void SaveAudioData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat, std::string fileName)
{
    // No longer print audio parameters here since we do it at startup
    
    fprintf(stderr, "Saving to filename: %s.pcm\n", fileName.c_str());
    
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
        fprintf(stderr, "Unable to open output PCM file: %d\n", GetLastError());
        return;
    }

    if (WritePcmFile(pcmFile, CaptureBuffer, BufferSize))
    {
        fprintf(stderr, "Successfully wrote PCM data to %s.pcm\n", fileName.c_str());
    }
    else
    {
        fprintf(stderr, "Failed to write PCM file\n");
    }
    
    CloseHandle(pcmFile);
}

// Function to append captured audio data to an existing PCM file
bool AppendAudioData(HANDLE FileHandle, BYTE* CaptureBuffer, size_t BufferSize)
{
    return WritePcmFile(FileHandle, CaptureBuffer, BufferSize);
}

// Function to set up audio capture device
void SetupAudioCapture(IMMDeviceEnumerator*& pEnumerator, IMMDevice*& pDevice)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to initialize COM: %x\n", hr);
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
        fprintf(stderr, "Failed to create device enumerator: %x\n", hr);
        return;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &pDevice
    );

    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to get default audio endpoint: %x\n", hr);
        return;
    }
    
    fprintf(stderr, "Audio device setup successful\n");
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

// Get command line argument value as integer
int GetCommandLineArgInt(int argc, char* argv[], const std::string& arg, int defaultValue)
{
    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == arg)
        {
            try {
                return std::stoi(argv[i + 1]);
            }
            catch (...) {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

// Get command line argument value as string
std::string GetCommandLineArgString(int argc, char* argv[], const std::string& arg, const std::string& defaultValue)
{
    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == arg)
        {
            return std::string(argv[i + 1]);
        }
    }
    return defaultValue;
}

int main(int argc, char* argv[])
{
    // Register signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    
    // Parse command line arguments
    int bufferIntervalMs = GetCommandLineArgInt(argc, argv, "--interval", 100);
    if (bufferIntervalMs <= 0) {
        fprintf(stderr, "Invalid interval value. Using default: 100ms\n");
        bufferIntervalMs = 100;
    }
    
    // Get output file path from command line or use default
    std::string outputFilePath = GetCommandLineArgString(argc, argv, "--output", "cache.pcm");
    
    // Print welcome message
    fprintf(stderr, "Simple Audio Capture Tool (Based on WASAPI)\n");
    fprintf(stderr, "------------------------------------------\n");
    fprintf(stderr, "Recording will continue until you press Ctrl+C to stop\n");
    fprintf(stderr, "Buffer interval: %d ms\n", bufferIntervalMs);
    fprintf(stderr, "Output file: %s\n\n", outputFilePath.c_str());
    
    // Setup COM and audio device
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    
    SetupAudioCapture(pEnumerator, pDevice);
    if (!pDevice)
    {
        fprintf(stderr, "Failed to set up audio device.\n");
        return 1;
    }
    
    // Create output file
    HANDLE pcmFile = CreateFileA(
        outputFilePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (pcmFile == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Unable to create output PCM file: %d\n", GetLastError());
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    fprintf(stderr, "Will save to: %s\n", outputFilePath.c_str());
    
    // Create and initialize the capturer
    CWASAPICapture* capturer = new CWASAPICapture(pDevice, true, eConsole);
    if (!capturer)
    {
        fprintf(stderr, "Failed to create audio capturer.\n");
        CloseHandle(pcmFile);
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    int targetLatency = 10; // in milliseconds
    if (!capturer->Initialize(targetLatency))
    {
        fprintf(stderr, "Failed to initialize audio capturer.\n");
        CloseHandle(pcmFile);
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }

    // Print audio parameters in JSON format immediately after initialization to stdout
    // Note we don't add any labels or explanations, just the pure JSON
    PrintAudioParameters(capturer->MixFormat());
    
    // Define buffer size to accumulate data for the interval duration
    // We'll make the buffer large to ensure it won't overflow
    const double safetyFactor = 2.0; // 2x safety factor
    const double bufferDurationInSeconds = (bufferIntervalMs / 1000.0) * safetyFactor;
    size_t bufferSize = static_cast<size_t>(capturer->SamplesPerSecond() * bufferDurationInSeconds * capturer->FrameSize());
    BYTE* captureBuffer = new BYTE[bufferSize];
    
    if (!captureBuffer)
    {
        fprintf(stderr, "Failed to allocate capture buffer.\n");
        CloseHandle(pcmFile);
        capturer->Shutdown();
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    // Start capturing - we'll only call Start once
    if (!capturer->Start(captureBuffer, bufferSize))
    {
        fprintf(stderr, "Failed to start audio capture.\n");
        delete[] captureBuffer;
        CloseHandle(pcmFile);
        capturer->Shutdown();
        capturer->Release();
        capturer = NULL;
        SafeRelease(&pDevice);
        SafeRelease(&pEnumerator);
        CoUninitialize();
        return 1;
    }
    
    fprintf(stderr, "Recording... Press Ctrl+C to stop\n");
    fprintf(stderr, "Buffer size: %zu bytes (%.3f seconds of audio)\n", bufferSize, bufferDurationInSeconds);
    
    int totalSeconds = 0;
    int captureCount = 0;
    
    // Main recording loop
    while (g_running)
    {
        // Wait for the specified interval
        Sleep(bufferIntervalMs);
        captureCount++;
        
        // Check how much data has been captured
        size_t bytesAvailable = capturer->BytesCaptured();
        
        // If we have data, write it to the file and reset capture position
        if (bytesAvailable > 0)
        {
            // Write captured data to file
            if (!WritePcmFile(pcmFile, captureBuffer, bytesAvailable))
            {
                fprintf(stderr, "\nFailed to write audio data.\n");
                break;
            }
            
            // Flush file to make sure data is written to disk for downstream consumers
            FlushFileBuffers(pcmFile);
            
            // Reset buffer for next capture without stopping the capturer
            capturer->ResetCaptureIndex();
        }
        
        // Update display every second
        int updatesPerSecond = 1000 / bufferIntervalMs;
        if (captureCount % updatesPerSecond == 0) {
            totalSeconds++;
            fprintf(stderr, "\rRecording: %d seconds", totalSeconds);
        }
    }
    
    // Now that we're done, stop the capturer
    capturer->Stop();
    
    fprintf(stderr, "\nRecording complete. Total duration: %d seconds\n", totalSeconds);
    fprintf(stderr, "Audio data saved to %s\n", outputFilePath.c_str());
    
    // Clean up
    delete[] captureBuffer;
    CloseHandle(pcmFile);
    capturer->Shutdown();
    capturer->Release();
    capturer = NULL;
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    CoUninitialize();
    
    fprintf(stderr, "Program complete.\n");
    return 0;
} 