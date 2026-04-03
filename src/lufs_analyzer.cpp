#include "lufs_analyzer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#include <cmath>
#include <vector>
#include <algorithm>

// EBU R128 constants
static const double LUFS_GATE = -70.0;
static const double LUFS_REL_GATE = -10.0;

LufsAnalysis AnalyzeLufs(const std::string& filepath)
{
    LufsAnalysis result;
    
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filepath.c_str(), nullptr, nullptr) < 0)
        return result;
    
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
    {
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    // Find audio stream
    int audio_stream_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (audio_stream_idx < 0)
    {
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    AVStream* stream = fmt_ctx->streams[audio_stream_idx];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder)
    {
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(dec_ctx, stream->codecpar);
    
    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0)
    {
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    // Read and decode audio directly (no resampling - handle formats natively)
    std::vector<double> samples;
    samples.reserve(48000 * 10); // Reserve 10 seconds at 48kHz
    
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    double maxSample = 0.0;
    int ret;
    
    while (av_read_frame(fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index != audio_stream_idx)
        {
            av_packet_unref(packet);
            continue;
        }
        
        ret = avcodec_send_packet(dec_ctx, packet);
        av_packet_unref(packet);
        
        while (ret >= 0)
        {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                goto cleanup;
            
            // Process samples based on format
            int numChannels = dec_ctx->ch_layout.nb_channels;
            
            if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT || 
                dec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
            {
                // Float format
                for (int i = 0; i < frame->nb_samples; i++)
                {
                    float sample = 0.0f;
                    if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT)
                    {
                        // Interleaved
                        const float* data = reinterpret_cast<const float*>(frame->data[0]);
                        for (int ch = 0; ch < numChannels; ch++)
                            sample += data[i * numChannels + ch];
                        sample /= numChannels;
                    }
                    else
                    {
                        // Planar - average channels
                        for (int ch = 0; ch < numChannels && ch < 2; ch++)
                        {
                            if (frame->data[ch])
                                sample += reinterpret_cast<const float*>(frame->data[ch])[i];
                        }
                        sample /= std::max(1, std::min(numChannels, 2));
                    }
                    samples.push_back(sample);
                    maxSample = std::max(maxSample, static_cast<double>(std::abs(sample)));
                }
            }
            else if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_S16 ||
                     dec_ctx->sample_fmt == AV_SAMPLE_FMT_S16P)
            {
                // 16-bit signed int
                for (int i = 0; i < frame->nb_samples; i++)
                {
                    float sample = 0.0f;
                    if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_S16)
                    {
                        const int16_t* data = reinterpret_cast<const int16_t*>(frame->data[0]);
                        for (int ch = 0; ch < numChannels; ch++)
                            sample += data[i * numChannels + ch] / 32768.0f;
                        sample /= numChannels;
                    }
                    else
                    {
                        for (int ch = 0; ch < numChannels && ch < 2; ch++)
                        {
                            if (frame->data[ch])
                                sample += reinterpret_cast<const int16_t*>(frame->data[ch])[i] / 32768.0f;
                        }
                        sample /= std::max(1, std::min(numChannels, 2));
                    }
                    samples.push_back(sample);
                    maxSample = std::max(maxSample, static_cast<double>(std::abs(sample)));
                }
            }
            else if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_S32 ||
                     dec_ctx->sample_fmt == AV_SAMPLE_FMT_S32P)
            {
                // 32-bit signed int
                for (int i = 0; i < frame->nb_samples; i++)
                {
                    float sample = 0.0f;
                    if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_S32)
                    {
                        const int32_t* data = reinterpret_cast<const int32_t*>(frame->data[0]);
                        for (int ch = 0; ch < numChannels; ch++)
                            sample += data[i * numChannels + ch] / 2147483648.0f;
                        sample /= numChannels;
                    }
                    else
                    {
                        for (int ch = 0; ch < numChannels && ch < 2; ch++)
                        {
                            if (frame->data[ch])
                                sample += reinterpret_cast<const int32_t*>(frame->data[ch])[i] / 2147483648.0f;
                        }
                        sample /= std::max(1, std::min(numChannels, 2));
                    }
                    samples.push_back(sample);
                    maxSample = std::max(maxSample, static_cast<double>(std::abs(sample)));
                }
            }
        }
        ret = 0;
    }
    
    // Flush decoder
    avcodec_send_packet(dec_ctx, nullptr);
    while (true)
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;
        
        int numChannels = dec_ctx->ch_layout.nb_channels;
        
        // Handle remaining frames (simplified - just process float for now)
        if (dec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT)
        {
            for (int i = 0; i < frame->nb_samples; i++)
            {
                const float* data = reinterpret_cast<const float*>(frame->data[0]);
                float sample = 0.0f;
                for (int ch = 0; ch < numChannels; ch++)
                    sample += data[i * numChannels + ch];
                sample /= numChannels;
                samples.push_back(sample);
                maxSample = std::max(maxSample, static_cast<double>(std::abs(sample)));
            }
        }
    }
    
cleanup:
    // Calculate peak
    if (maxSample > 0)
        result.peakDb = 20.0 * std::log10(maxSample);
    else
        result.peakDb = -96.0;
    
    // Calculate duration
    result.duration = static_cast<double>(samples.size()) / 48000.0;
    
    // Calculate LUFS using simplified method
    if (!samples.empty())
    {
        // Block size for 400ms at 48kHz
        const size_t blockSize = 19200;
        const size_t overlap = blockSize / 4;  // 75% overlap
        
        std::vector<double> blockLoudness;
        
        for (size_t i = 0; i + blockSize <= samples.size(); i += overlap)
        {
            double sum = 0.0;
            for (size_t j = i; j < i + blockSize && j < samples.size(); j++)
                sum += samples[j] * samples[j];
            
            double meanSquare = sum / blockSize;
            if (meanSquare > 0)
            {
                // EBU R128 formula: -0.691 + 10*log10(mean square)
                double lufs = -0.691 + 10.0 * std::log10(meanSquare);
                blockLoudness.push_back(lufs);
            }
        }
        
        if (!blockLoudness.empty())
        {
            // Absolute gate at -70 LUFS
            std::vector<double> gated;
            for (double l : blockLoudness)
                if (l > LUFS_GATE)
                    gated.push_back(l);
            
            if (!gated.empty())
            {
                // Calculate integrated loudness
                double sumPow = 0.0;
                for (double l : gated)
                    sumPow += std::pow(10.0, l / 10.0);
                double integrated = 10.0 * std::log10(sumPow / gated.size());
                
                // Relative gate
                double relGate = integrated + LUFS_REL_GATE;
                std::vector<double> relGated;
                for (double l : gated)
                    if (l > relGate)
                        relGated.push_back(l);
                
                if (!relGated.empty())
                {
                    sumPow = 0.0;
                    for (double l : relGated)
                        sumPow += std::pow(10.0, l / 10.0);
                    result.integratedLufs = 10.0 * std::log10(sumPow / relGated.size());
                }
                else
                {
                    result.integratedLufs = integrated;
                }
                
                result.momentaryLufs = *std::max_element(blockLoudness.begin(), blockLoudness.end());
                
                // Short-term (3s)
                const size_t shortBlockSize = 144000;
                std::vector<double> shortBlocks;
                for (size_t i = 0; i + shortBlockSize <= samples.size(); i += shortBlockSize / 2)
                {
                    double sum = 0.0;
                    for (size_t j = i; j < i + shortBlockSize && j < samples.size(); j++)
                        sum += samples[j] * samples[j];
                    double ms = sum / shortBlockSize;
                    if (ms > 0)
                        shortBlocks.push_back(-0.691 + 10.0 * std::log10(ms));
                }
                if (!shortBlocks.empty())
                    result.shortTermLufs = *std::max_element(shortBlocks.begin(), shortBlocks.end());
            }
        }
    }
    
    result.valid = true;
    
    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    
    return result;
}
