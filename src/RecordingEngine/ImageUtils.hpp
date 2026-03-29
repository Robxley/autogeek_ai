#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace agk {
    /**
     * Saves raw pixel data to a PNG file.
     * @param filepath The output path (e.g. "thumbnail.png")
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param channels Number of color channels (e.g. 4 for RGBA/BGRA)
     * @param data Pointer to the raw pixel buffer
     * @param stride_in_bytes The length of a row in bytes (width * channels).
     * @return true if successful, false otherwise.
     */
    bool SaveImagePNG(const std::string& filepath, int width, int height, int channels, const uint8_t* data, int stride_in_bytes);

    /**
     * @brief Performs a fast nearest-neighbor downscale of a 4-channel BGRA/RGBA buffer to 480p max-height, keeping aspect ratio.
     */
    void DownscaleBGRA_480p(const uint8_t* src, int srcW, int srcH, int srcStride, std::vector<uint8_t>& dst, int& dstW, int& dstH);
}
