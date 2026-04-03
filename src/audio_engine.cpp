#include "audio_engine.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <algorithm>
#include <cstring>

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine()
{
    Shutdown();
}

bool AudioEngine::Initialize()
{
    if (initialized.load())
        return true;
    
    context = new ma_context();
    ma_result result = ma_context_init(nullptr, 0, nullptr, context);
    if (result != MA_SUCCESS)
    {
        delete context;
        context = nullptr;
        return false;
    }
    
    initialized.store(true);
    return true;
}

void AudioEngine::Shutdown()
{
    StopRecording();
    StopPlayback();
    
    if (context)
    {
        ma_context_uninit(context);
        delete context;
        context = nullptr;
    }
    
    initialized.store(false);
}

bool AudioEngine::StartRecording(const std::string& filepath)
{
    if (!initialized.load() || recording.load())
        return false;
    
    currentRecordPath = filepath;
    
    // Initialize encoder - mono, 48kHz, float
    encoder = new ma_encoder();
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 1, 48000);
    
    ma_result result = ma_encoder_init_file(filepath.c_str(), &config, encoder);
    if (result != MA_SUCCESS)
    {
        delete encoder;
        encoder = nullptr;
        return false;
    }
    
    // Initialize recording device
    recordDevice = new ma_device();
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = RecordingCallback;
    deviceConfig.pUserData = this;
    
    result = ma_device_init(context, &deviceConfig, recordDevice);
    if (result != MA_SUCCESS)
    {
        ma_encoder_uninit(encoder);
        delete encoder;
        encoder = nullptr;
        delete recordDevice;
        recordDevice = nullptr;
        return false;
    }
    
    recording.store(true);
    ma_device_start(recordDevice);
    
    return true;
}

void AudioEngine::StopRecording()
{
    if (!recording.load())
        return;
    
    recording.store(false);
    
    if (recordDevice)
    {
        ma_device_stop(recordDevice);
        ma_device_uninit(recordDevice);
        delete recordDevice;
        recordDevice = nullptr;
    }
    
    if (encoder)
    {
        ma_encoder_uninit(encoder);
        delete encoder;
        encoder = nullptr;
    }
}

bool AudioEngine::StartPlayback(const std::string& filepath)
{
    if (!initialized.load() || playing.load())
        return false;
    
    StopPlayback();
    currentPlaybackPath = filepath;
    
    // Load audio file using miniaudio decoder - get native format
    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(filepath.c_str(), nullptr, &decoder);
    if (result != MA_SUCCESS)
        return false;
    
    // Get file info
    ma_uint64 lengthInFrames;
    ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames);
    
    // Store original format info
    nativeChannels = decoder.outputChannels;
    nativeSampleRate = decoder.outputSampleRate;
    playbackDuration = static_cast<float>(lengthInFrames) / static_cast<float>(nativeSampleRate);
    
    // Decode entire file at native format
    playbackBuffer.resize(lengthInFrames * nativeChannels);
    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&decoder, playbackBuffer.data(), lengthInFrames, &framesRead);
    ma_decoder_uninit(&decoder);
    
    playbackPosition.store(0);
    
    // Initialize playback device at native sample rate
    playbackDevice = new ma_device();
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = nativeChannels;
    deviceConfig.sampleRate = nativeSampleRate;
    deviceConfig.dataCallback = PlaybackCallback;
    deviceConfig.pUserData = this;
    
    result = ma_device_init(context, &deviceConfig, playbackDevice);
    if (result != MA_SUCCESS)
    {
        delete playbackDevice;
        playbackDevice = nullptr;
        playbackBuffer.clear();
        return false;
    }
    
    playing.store(true);
    ma_device_start(playbackDevice);
    
    return true;
}

void AudioEngine::StopPlayback()
{
    if (!playing.load())
        return;
    
    playing.store(false);
    
    if (playbackDevice)
    {
        ma_device_stop(playbackDevice);
        ma_device_uninit(playbackDevice);
        delete playbackDevice;
        playbackDevice = nullptr;
    }
    
    playbackBuffer.clear();
    playbackPosition.store(0);
}

float AudioEngine::GetPlaybackPosition() const
{
    if (!playing.load() || playbackBuffer.empty() || nativeSampleRate == 0)
        return 0.0f;
    
    size_t pos = playbackPosition.load();
    return static_cast<float>(pos / nativeChannels) / static_cast<float>(nativeSampleRate);
}

float AudioEngine::GetPlaybackDuration() const
{
    return playbackDuration;
}

void AudioEngine::RecordingCallback(ma_device* device, void* output, const void* input, unsigned int frameCount)
{
    (void)output; // Unused for capture
    
    AudioEngine* engine = static_cast<AudioEngine*>(device->pUserData);
    if (!engine || !engine->recording.load() || !engine->encoder)
        return;
    
    const float* inputFloat = static_cast<const float*>(input);
    ma_encoder_write_pcm_frames(engine->encoder, inputFloat, frameCount, nullptr);
}

void AudioEngine::PlaybackCallback(ma_device* device, void* output, const void* input, unsigned int frameCount)
{
    (void)input; // Unused for playback
    
    AudioEngine* engine = static_cast<AudioEngine*>(device->pUserData);
    if (!engine || !engine->playing.load())
    {
        std::fill(static_cast<float*>(output), static_cast<float*>(output) + frameCount * device->playback.channels, 0.0f);
        return;
    }
    
    size_t pos = engine->playbackPosition.load();
    size_t samplesAvailable = engine->playbackBuffer.size() - pos;
    size_t samplesToCopy = std::min(static_cast<size_t>(frameCount * device->playback.channels), samplesAvailable);
    size_t framesToCopy = samplesToCopy / device->playback.channels;
    
    float* outputFloat = static_cast<float*>(output);
    
    if (engine->nativeChannels == device->playback.channels)
    {
        // Direct copy
        std::memcpy(outputFloat, engine->playbackBuffer.data() + pos, samplesToCopy * sizeof(float));
    }
    else if (engine->nativeChannels == 1 && device->playback.channels == 2)
    {
        // Mono to stereo - duplicate samples
        for (size_t i = 0; i < framesToCopy; i++)
        {
            float sample = engine->playbackBuffer[pos + i];
            outputFloat[i * 2] = sample;
            outputFloat[i * 2 + 1] = sample;
        }
    }
    else
    {
        // Other conversions - just copy what we can
        std::memcpy(outputFloat, engine->playbackBuffer.data() + pos, samplesToCopy * sizeof(float));
    }
    
    if (framesToCopy < frameCount)
    {
        std::fill(outputFloat + framesToCopy * device->playback.channels, 
                  outputFloat + frameCount * device->playback.channels, 0.0f);
        engine->playing.store(false);
    }
    
    engine->playbackPosition.fetch_add(samplesToCopy);
}
