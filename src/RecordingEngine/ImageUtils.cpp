#include "ImageUtils.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../3rdparty/stb_image_write.h"
#include <agk/RecordingEngine/Log.hpp>

namespace agk {
    bool SaveImagePNG(const std::string& filepath, int width, int height, int channels, const uint8_t* data, int stride_in_bytes) {
        int result = stbi_write_png(filepath.c_str(), width, height, channels, data, stride_in_bytes);
        if (result == 0) {
            AGK_CORE_ERROR("[ImageUtils] Failed to save PNG to {}", filepath);
            return false;
        }
        return true;
    }

    void DownscaleBGRA_480p(const uint8_t* src, int srcW, int srcH, int srcStride, std::vector<uint8_t>& dst, int& dstW, int& dstH) {
        if (srcH <= 480) {
            dstW = srcW;
            dstH = srcH;
            dst.resize(srcW * srcH * 4);
            for (int y = 0; y < srcH; ++y) {
                memcpy(dst.data() + (y * srcW * 4), src + (y * srcStride), srcW * 4);
            }
            return;
        }

        dstH = 480;
        dstW = (srcW * dstH) / srcH;
        dst.resize(dstW * dstH * 4);

        for (int y = 0; y < dstH; ++y) {
            int srcY = (y * srcH) / dstH;
            const uint8_t* srcRow = src + (srcY * srcStride);
            for (int x = 0; x < dstW; ++x) {
                int srcX = (x * srcW) / dstW;
                const uint8_t* srcPixel = srcRow + (srcX * 4);
                uint8_t* dstPixel = dst.data() + (y * dstW + x) * 4;
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }
    }
}
