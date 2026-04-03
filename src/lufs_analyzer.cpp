#include "lufs_analyzer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#include <cmath>
#include <vector>
#include <algorithm>

// EBU R128 constants
static const double LUFS_GATE = -70.0;    // Absolute gate
static const double LUFS_REL_GATE = -10.0; // Relative gate offset
static const int BLOCK_SIZE_400MS = 19200;   // 400ms at 48kHz
static const int BLOCK_SIZE_3S = 144000;     // 3s at 48kHz

// Convert sample to LUFS (simplified - assumes 0 dBFS = 1.0)
static double SampleToLufs(double sample)
{
    if (sample == 0.0) return -HUGE_VAL;
    return 20.0 * std::log10(std::abs(sample));
}

// Calculate mean square of a block
static double CalculateMeanSquare(const std::vector<double>& samples, size_t start, size_t count)
{
    double sum = 0.0;
    size_t end = std::min(start + count, samples.size());
    for (size_t i = start; i < end; ++i)
    {
        sum += samples[i] * samples[i];
    }
    return sum / count;
}

LufsAnalysis AnalyzeLufs(const std::string& filepath)
{
    LufsAnalysis result;
    
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filepath.c_str(), nullptr, nullptr) < 0)
    {
        return result;
    }
    
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
    
    // Set up resampler to convert to float planar stereo at 48kHz
    SwrContext* swr_ctx = nullptr;
    int target_rate = 48000;
    AVChannelLayout target_layout;
    av_channel_layout_default(&target_layout, 2); // Stereo
    
    int ret = swr_alloc_set_opts2(&swr_ctx,
                                  &target_layout, AV_SAMPLE_FMT_FLTP, target_rate,
                                  &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate,
                                  0, nullptr);
    if (ret < 0 || swr_init(swr_ctx) < 0)
    {
        if (swr_ctx) swr_free(&swr_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    // Read and decode audio
    std::vector<double> samples;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* resampled = av_frame_alloc();
    
    double maxSample = 0.0;
    
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
            
            // Resample
            int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
            av_frame_make_writable(resampled);
            ret = swr_convert(swr_ctx, nullptr, 0,
                              const_cast<const uint8_t**>(frame->data), frame->nb_samples);
            
            uint8_t* output[2] = { nullptr, nullptr };
            ret = swr_convert(swr_ctx, output, out_samples,
                              nullptr, 0);
            
            // Process samples
            for (int i = 0; i < frame->nb_samples; i++)
            {
                // Average channels to get mono
                float left = reinterpret_cast<float*>(frame->data[0])[i];
                float right = (dec_ctx->ch_layout.nb_channels > 1) 
                    ? reinterpret_cast<float*>(frame->extended_data[1])[i] 
                    : left;
                float mono = (left + right) * 0.5f;
                
                samples.push_back(mono);
                maxSample = std::max(maxSample, static_cast<double>(std::abs(mono)));
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
        
        for (int i = 0; i < frame->nb_samples; i++)
        {
            float left = reinterpret_cast<float*>(frame->data[0])[i];
            float right = (dec_ctx->ch_layout.nb_channels > 1) 
                ? reinterpret_cast<float*>(frame->extended_data[1])[i] 
                : left;
            float mono = (left + right) * 0.5f;
            samples.push_back(mono);
            maxSample = std::max(maxSample, static_cast<double>(std::abs(mono)));
        }
    }
    
    // Calculate peak
    if (maxSample > 0)
    {
        result.peakDb = 20.0 * std::log10(maxSample);
    }
    
    // Calculate duration
    result.duration = static_cast<double>(samples.size()) / target_rate;
    
    // Calculate momentary loudness (400ms blocks)
    std::vector<double> momentaryBlocks;
    for (size_t i = 0; i + BLOCK_SIZE_400MS <= samples.size(); i += BLOCK_SIZE_400MS / 4) // 75% overlap
    {
        double ms = CalculateMeanSquare(samples, i, BLOCK_SIZE_400MS);
        if (ms > 0)
        {
            double lufs = -0.691 + 10.0 * std::log10(ms);
            momentaryBlocks.push_back(lufs);
        }
    }
    
    // Calculate integrated loudness with gating
    if (!momentaryBlocks.empty())
    {
        // Absolute gate
        std::vector<double> gated;
        for (double l : momentaryBlocks)
        {
            if (l > LUFS_GATE)
                gated.push_back(l);
        }
        
        if (!gated.empty())
        {
            // Calculate mean of gated blocks
            double sum = 0.0;
            for (double l : gated) sum += std::pow(10.0, l / 10.0);
            double meanPow = sum / gated.size();
            double integrated = 10.0 * std::log10(meanPow);
            
            // Relative gate
            double relativeGate = integrated + LUFS_REL_GATE;
            std::vector<double> relGated;
            for (double l : gated)
            {
                if (l > relativeGate)
                    relGated.push_back(l);
            }
            
            if (!relGated.empty())
            {
                sum = 0.0;
                for (double l : relGated) sum += std::pow(10.0, l / 10.0);
                meanPow = sum / relGated.size();
                result.integratedLufs = 10.0 * std::log10(meanPow);
            }
            else
            {
                result.integratedLufs = integrated;
            }
        }
        
        // Momentary (max of 400ms blocks)
        result.momentaryLufs = *std::max_element(momentaryBlocks.begin(), momentaryBlocks.end());
        
        // Short-term (3s sliding window)
        std::vector<double> shortTermBlocks;
        for (size_t i = 0; i + BLOCK_SIZE_3S <= samples.size(); i += BLOCK_SIZE_3S / 2)
        {
            double ms = CalculateMeanSquare(samples, i, BLOCK_SIZE_3S);
            if (ms > 0)
            {
                double lufs = -0.691 + 10.0 * std::log10(ms);
                shortTermBlocks.push_back(lufs);
            }
        }
        
        if (!shortTermBlocks.empty())
        {
            result.shortTermLufs = *std::max_element(shortTermBlocks.begin(), shortTermBlocks.end());
        }
    }
    
    result.valid = true;
    
    // Cleanup
    av_frame_free(&resampled);
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    
    return result;
}
