#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <cstdint>

// Forward declarations for miniaudio
struct ma_context;
struct ma_device;
struct ma_encoder;

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();
    
    // Recording
    bool StartRecording(const std::string& filepath);
    void StopRecording();
    bool IsRecording() const { return recording.load(); }
    
    // Playback
    bool StartPlayback(const std::string& filepath);
    void StopPlayback();
    bool IsPlaying() const { return playing.load(); }
    
    // Device initialization
    bool Initialize();
    void Shutdown();
    
    // Getters
    float GetPlaybackPosition() const;
    float GetPlaybackDuration() const;
    
private:
    static void RecordingCallback(ma_device* device, void* output, const void* input, unsigned int frameCount);
    static void PlaybackCallback(ma_device* device, void* output, const void* input, unsigned int frameCount);
    
    ma_context* context = nullptr;
    ma_device* recordDevice = nullptr;
    ma_device* playbackDevice = nullptr;
    ma_encoder* encoder = nullptr;
    
    std::atomic<bool> recording{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> initialized{false};
    
    // Playback data
    std::vector<float> playbackBuffer;
    std::atomic<size_t> playbackPosition{0};
    float playbackDuration = 0.0f;
    uint32_t nativeChannels = 2;
    uint32_t nativeSampleRate = 48000;
    
    std::string currentRecordPath;
    std::string currentPlaybackPath;
};
