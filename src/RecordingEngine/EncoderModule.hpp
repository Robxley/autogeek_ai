#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // for WAVEFORMATEX
#include <mmreg.h>
#include <agk/RecordingEngine/ConfigSystem.hpp>
#include <string>
#include <memory>
#include <vector>
#include <mutex>

// Forward declarations for FFmpeg to keep header clean
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;

namespace agk {

    /**
     * @brief High-performance multi-stream encoder (Video + Audio) using FFmpeg.
     */
    class EncoderModule {
    public:
        EncoderModule();
        ~EncoderModule();

        /**
         * @brief Initializes the encoder with both video and optional audio streams.
         * @param outputFile Path to the .mkv file.
         * @param vConfig Video configuration.
         * @param aConfig Audio configuration.
         * @param audioFormat Native format from WASAPI (optional).
         * @return True if initialization succeeded.
         */
        bool Initialize(const std::string& outputFile, const VideoConfig& vConfig, const AudioConfig& aConfig, WAVEFORMATEX* audioFormat = nullptr);

        /**
         * @brief Encodes a single BGRA video frame from the capture module.
         * @param data Pointer to raw BGRA pixels.
         * @param linesize Row pitch in bytes (stride).
         * @param frameIndex Frame sequence index.
         * @param cropX Optional cropped X offset (left margin).
         * @param cropY Optional cropped Y offset (top margin).
         * @param cropW Optional cropped width (0 = uncropped).
         * @param cropH Optional cropped height (0 = uncropped).
         */
        bool EncodeVideoFrame(const uint8_t* data, int linesize, int64_t frameIndex, int cropX = 0, int cropY = 0, int cropW = 0, int cropH = 0);
        bool EncodeAudioBuffer(const uint8_t* data, uint32_t sampleCount, uint64_t timestamp);

        /**
         * @brief Finalizes encoding, flushes all buffers, and closes the file.
         */
        void Finalize();

    private:
        bool OpenOutput(const std::string& filename);
        bool AddVideoStream();
        bool AddAudioStream(WAVEFORMATEX* audioFormat);
        void CloseOutput();

        AVFormatContext* m_fmtCtx = nullptr;
        std::mutex m_muxMutex;
        
        // Video Stream
        AVCodecContext* m_vCodecCtx = nullptr;
        AVStream* m_vStream = nullptr;
        AVFrame* m_vFrame = nullptr;
        SwsContext* m_swsCtx = nullptr;

        // Audio Stream
        AVCodecContext* m_aCodecCtx = nullptr;
        AVStream* m_aStream = nullptr;
        AVFrame* m_aFrame = nullptr;      // Output Frame (Encoded)
        AVFrame* m_aInputFrame = nullptr; // Input Frame (Raw)
        SwrContext* m_swrCtx = nullptr;
        int64_t m_audioPts = 0;

        VideoConfig m_vConfig;
        AudioConfig m_aConfig;
        int m_lastSrcW = 0;
        int m_lastSrcH = 0;
        bool m_isInitialized = false;
        bool m_hasAudio = false;
    };

} // namespace agk
