extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/audio_fifo.h>
}

#include "EncoderModule.hpp"
#include <agk/RecordingEngine/Log.hpp>

namespace agk {

    EncoderModule::EncoderModule() : m_fmtCtx(nullptr), m_vCodecCtx(nullptr), m_aCodecCtx(nullptr),
                                     m_vStream(nullptr), m_aStream(nullptr), m_vFrame(nullptr),
                                     m_aFrame(nullptr), m_swsCtx(nullptr), m_swrCtx(nullptr),
                                     m_isInitialized(false), m_hasAudio(false), m_audioPts(0) {}

    EncoderModule::~EncoderModule() {
        Finalize();
    }

    bool EncoderModule::Initialize(const std::string& outputFile, const VideoConfig& vConfig, const AudioConfig& aConfig, WAVEFORMATEX* audioFormat) {
        m_vConfig = vConfig;
        m_aConfig = aConfig;
        
        if (!OpenOutput(outputFile)) return false;
        if (!AddVideoStream()) return false;
        
        if (aConfig.enabled && audioFormat) {
            if (AddAudioStream(audioFormat)) {
                m_hasAudio = true;
            }
        }

        // Open output file
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&m_fmtCtx->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0) {
                AGK_CORE_ERROR("[Encoder] Could not open output file {}", outputFile);
                return false;
            }
        }

        // Write header
        if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
            AGK_CORE_ERROR("[Encoder] Could not write header");
            return false;
        }

        m_isInitialized = true;
        AGK_CORE_INFO("[Encoder] Multi-stream pipeline initialized: Video (H264) + Audio (AAC: {})", m_hasAudio ? "ON" : "OFF");
        return true;
    }

    bool EncoderModule::OpenOutput(const std::string& filename) {
        if (avformat_alloc_output_context2(&m_fmtCtx, nullptr, "matroska", filename.c_str()) < 0) {
            AGK_CORE_ERROR("[Encoder] Could not allocate output format context");
            return false;
        }
        return true;
    }

    bool EncoderModule::AddVideoStream() {
        const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
        if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) return false;

        m_vStream = avformat_new_stream(m_fmtCtx, codec);
        m_vCodecCtx = avcodec_alloc_context3(codec);
        
        m_vCodecCtx->width = m_vConfig.width;
        m_vCodecCtx->height = m_vConfig.height;
        m_vCodecCtx->time_base = {1, m_vConfig.target_fps};
        m_vCodecCtx->framerate = {m_vConfig.target_fps, 1};
        m_vCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        m_vCodecCtx->bit_rate = (int64_t)m_vConfig.bitrate_kbps * 1000;
        m_vCodecCtx->gop_size = 12;

        if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_vCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(m_vCodecCtx, codec, nullptr) < 0) return false;
        avcodec_parameters_from_context(m_vStream->codecpar, m_vCodecCtx);
        m_vStream->time_base = m_vCodecCtx->time_base;

        m_vFrame = av_frame_alloc();
        m_vFrame->format = m_vCodecCtx->pix_fmt;
        m_vFrame->width = m_vCodecCtx->width;
        m_vFrame->height = m_vCodecCtx->height;
        av_frame_get_buffer(m_vFrame, 32);

        m_swsCtx = sws_getContext(m_vConfig.width, m_vConfig.height, AV_PIX_FMT_BGRA,
                                  m_vConfig.width, m_vConfig.height, AV_PIX_FMT_YUV420P,
                                  SWS_BICUBIC, nullptr, nullptr, nullptr);
        return true;
    }

    bool EncoderModule::AddAudioStream(WAVEFORMATEX* audioFormat) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec) return false;

        m_aStream = avformat_new_stream(m_fmtCtx, codec);
        m_aCodecCtx = avcodec_alloc_context3(codec);

        m_aCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        m_aCodecCtx->bit_rate = (int64_t)m_aConfig.audio_bitrate_kbps * 1000;
        m_aCodecCtx->sample_rate = audioFormat->nSamplesPerSec;
        av_channel_layout_default(&m_aCodecCtx->ch_layout, 2);

        if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_aCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(m_aCodecCtx, codec, nullptr) < 0) return false;
        avcodec_parameters_from_context(m_aStream->codecpar, m_aCodecCtx);
        m_aStream->time_base = {1, m_aCodecCtx->sample_rate};

        AVChannelLayout inLayout;
        av_channel_layout_default(&inLayout, 2);
        
        swr_alloc_set_opts2(&m_swrCtx,
            &m_aCodecCtx->ch_layout, m_aCodecCtx->sample_fmt, m_aCodecCtx->sample_rate,
            &inLayout, AV_SAMPLE_FMT_FLT, audioFormat->nSamplesPerSec,
            0, nullptr);
        swr_init(m_swrCtx);
        
        m_aInputFrame = av_frame_alloc();
        m_aInputFrame->format = AV_SAMPLE_FMT_FLT;
        av_channel_layout_default(&m_aInputFrame->ch_layout, 2);
        
        m_aFrame = av_frame_alloc();
        m_aFrame->format = m_aCodecCtx->sample_fmt;
        av_channel_layout_copy(&m_aFrame->ch_layout, &m_aCodecCtx->ch_layout);
        m_aFrame->sample_rate = m_aCodecCtx->sample_rate;

        m_audioFifo = av_audio_fifo_alloc(m_aCodecCtx->sample_fmt, m_aCodecCtx->ch_layout.nb_channels, 1);

        return true;
    }

    bool EncoderModule::EncodeVideoFrame(const uint8_t* data, int linesize, int64_t frameIndex, int cropX, int cropY, int cropW, int cropH) {
        if (!m_isInitialized) {
            AGK_CORE_ERROR("[Encoder] Encoder not initialized");
            return false;
        }

        static bool firstFrameLogged = false;
        if (!firstFrameLogged) {
            AGK_CORE_INFO("[Encoder] Received first frame for encoding (Frame Index: {}, Linesize: {})", frameIndex, linesize);
            firstFrameLogged = true;
        }

        std::lock_guard<std::mutex> lock(m_muxMutex);

        int actualSourceWidth = m_vConfig.width;
        int actualSourceHeight = m_vConfig.height;

        // Apply Cropping Strategy (Zero-Copy Offset)
        const uint8_t* srcData = data;
        if (cropW > 0 && cropH > 0) {
            actualSourceWidth = cropW;
            actualSourceHeight = cropH;
            
            // Pointer Math: Advance source pointer to the crop origin (X, Y)
            // BGRA format = 4 bytes per pixel
            srcData = data + (cropY * linesize) + (cropX * 4);
        }

        // Validate and re-initialize the scaler if source dimensions change dynamically
        if (actualSourceWidth != m_lastSrcW || actualSourceHeight != m_lastSrcH) {
            if (m_swsCtx) {
                sws_freeContext(m_swsCtx);
                m_swsCtx = nullptr;
            }

            m_swsCtx = sws_getContext(
                actualSourceWidth, actualSourceHeight, AV_PIX_FMT_BGRA, // Source (Cropped or Not)
                m_vConfig.width, m_vConfig.height, AV_PIX_FMT_YUV420P, // Dest (Target Encoding Dimensions)
                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
            );

            if (!m_swsCtx) {
                AGK_CORE_ERROR("[Encoder] sws_getContext failed for {}x{} to {}x{}", actualSourceWidth, actualSourceHeight, m_vConfig.width, m_vConfig.height);
                return false;
            }

            m_lastSrcW = actualSourceWidth;
            m_lastSrcH = actualSourceHeight;
            AGK_CORE_INFO("[Encoder] Scaler populated for dynamic source width: {}", actualSourceWidth);
        }

        uint8_t* srcDataArray[1] = { const_cast<uint8_t*>(srcData) };
        int srcLinesize[1] = { linesize };

        int result = sws_scale(m_swsCtx, srcDataArray, srcLinesize, 0, actualSourceHeight, m_vFrame->data, m_vFrame->linesize);
        if (result < 0) {
            AGK_CORE_ERROR("[Encoder] sws_scale failed with error code: {}", result);
            return false;
        }

        m_vFrame->pts = frameIndex;

        if (avcodec_send_frame(m_vCodecCtx, m_vFrame) < 0) {
            AGK_CORE_WARN("[Encoder] Error sending frame to video encoder (Frame: {})", frameIndex);
            return false;
        }

        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(m_vCodecCtx, pkt) == 0) {
            av_packet_rescale_ts(pkt, m_vCodecCtx->time_base, m_vStream->time_base);
            pkt->stream_index = m_vStream->index;
            av_interleaved_write_frame(m_fmtCtx, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        return true;
    }

    bool EncoderModule::EncodeAudioBuffer(const uint8_t* data, uint32_t sampleCount, uint64_t timestamp) {
        if (!m_isInitialized || !m_hasAudio) return false;

        std::lock_guard<std::mutex> lock(m_muxMutex);
        
        m_aInputFrame->nb_samples = sampleCount;
        if (m_aInputFrame->data[0] == nullptr || m_aInputFrame->nb_samples > m_aInputFrame->nb_samples) {
             av_frame_get_buffer(m_aInputFrame, 0);
        }
        memcpy(m_aInputFrame->data[0], data, sampleCount * 4 * 2);

        m_aFrame->nb_samples = av_rescale_rnd(swr_get_delay(m_swrCtx, m_aCodecCtx->sample_rate) + sampleCount, m_aCodecCtx->sample_rate, m_aCodecCtx->sample_rate, AV_ROUND_UP);
        if (m_aFrame->data[0] == nullptr || m_aFrame->nb_samples < m_aFrame->nb_samples) {
            av_frame_get_buffer(m_aFrame, 0);
        }

        swr_convert(m_swrCtx, m_aFrame->data, m_aFrame->nb_samples, (const uint8_t**)m_aInputFrame->data, m_aInputFrame->nb_samples);

        av_audio_fifo_realloc(m_audioFifo, av_audio_fifo_size(m_audioFifo) + m_aFrame->nb_samples);
        av_audio_fifo_write(m_audioFifo, (void**)m_aFrame->data, m_aFrame->nb_samples);

        int targetFrameSize = m_aCodecCtx->frame_size > 0 ? m_aCodecCtx->frame_size : 1024;

        while (av_audio_fifo_size(m_audioFifo) >= targetFrameSize) {
            AVFrame* encFrame = av_frame_alloc();
            encFrame->nb_samples = targetFrameSize;
            encFrame->format = m_aCodecCtx->sample_fmt;
            av_channel_layout_copy(&encFrame->ch_layout, &m_aCodecCtx->ch_layout);
            encFrame->sample_rate = m_aCodecCtx->sample_rate;
            av_frame_get_buffer(encFrame, 0);

            av_audio_fifo_read(m_audioFifo, (void**)encFrame->data, targetFrameSize);
            
            encFrame->pts = m_audioPts;
            m_audioPts += encFrame->nb_samples;

            if (avcodec_send_frame(m_aCodecCtx, encFrame) == 0) {
                AVPacket* pkt = av_packet_alloc();
                while (avcodec_receive_packet(m_aCodecCtx, pkt) == 0) {
                    av_packet_rescale_ts(pkt, m_aCodecCtx->time_base, m_aStream->time_base);
                    pkt->stream_index = m_aStream->index;
                    av_interleaved_write_frame(m_fmtCtx, pkt);
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
            }
            av_frame_free(&encFrame);
        }
        return true;
    }

    void EncoderModule::Finalize() {
        if (!m_isInitialized) return;

        // Flush video
        avcodec_send_frame(m_vCodecCtx, nullptr);
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(m_vCodecCtx, pkt) == 0) {
            av_packet_rescale_ts(pkt, m_vCodecCtx->time_base, m_vStream->time_base);
            av_interleaved_write_frame(m_fmtCtx, pkt);
            av_packet_unref(pkt);
        }

        // Flush audio
        if (m_hasAudio) {
            avcodec_send_frame(m_aCodecCtx, nullptr);
            while (avcodec_receive_packet(m_aCodecCtx, pkt) == 0) {
                av_packet_rescale_ts(pkt, m_aCodecCtx->time_base, m_aStream->time_base);
                av_interleaved_write_frame(m_fmtCtx, pkt);
                av_packet_unref(pkt);
            }
        }
        av_packet_free(&pkt);
        av_write_trailer(m_fmtCtx);
        CloseOutput();
        m_isInitialized = false;
        AGK_CORE_INFO("[Encoder] Finalized and streams closed.");
    }

    void EncoderModule::CloseOutput() {
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        if (m_swrCtx) swr_free(&m_swrCtx);
        if (m_vFrame) av_frame_free(&m_vFrame);
        if (m_aFrame) av_frame_free(&m_aFrame);
        if (m_aInputFrame) av_frame_free(&m_aInputFrame);
        if (m_audioFifo) {
            av_audio_fifo_free(m_audioFifo);
            m_audioFifo = nullptr;
        }
        if (m_vCodecCtx) avcodec_free_context(&m_vCodecCtx);
        if (m_aCodecCtx) avcodec_free_context(&m_aCodecCtx);
        if (m_fmtCtx) {
            if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_fmtCtx->pb);
            avformat_free_context(m_fmtCtx);
        }
        m_fmtCtx = nullptr;
    }

} // namespace agk
