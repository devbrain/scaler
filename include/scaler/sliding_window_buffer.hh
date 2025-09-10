#pragma once

#include <vector>
#include <cassert>

/**
 * A cache-friendly sliding window buffer for image processing algorithms.
 * 
 * This class manages a circular buffer of image rows to improve cache locality
 * when accessing pixels in a neighborhood pattern (e.g., 3x3, 5x5 windows).
 * 
 * @tparam PixelType The type of pixel data stored in the buffer
 */
template<typename PixelType>
class SlidingWindowBuffer {
private:
    std::vector<std::vector<PixelType>> buffer_;
    int window_height_;      // Number of rows in the window (e.g., 3 for 3x3, 5 for 5x5)
    int width_;              // Width of each row (image width + padding)
    int padding_;            // Padding on each side for boundary pixels
    int current_y_;          // Current y position in the source image
    int buffer_offset_;      // Offset from current_y to first buffer row
    
    // Maps a source row index to buffer index
    int rowToBufferIndex(int src_row) const {
        // The key insight: buffer index = (src_row % window_height_)
        // This ensures consistent mapping throughout processing
        return ((src_row % window_height_) + window_height_) % window_height_;
    }
    
public:
    /**
     * Initialize the sliding window buffer
     * @param window_height Height of the sliding window (e.g., 3 for 3x3 kernel)
     * @param image_width Width of the source image
     * @param padding Padding on each side for boundary access
     * @param buffer_offset Offset from current position (e.g., -1 for centered window)
     */
    SlidingWindowBuffer(int window_height, int image_width, int padding, int buffer_offset)
        : window_height_(window_height)
        , width_(image_width + 2 * padding)
        , padding_(padding)
        , current_y_(0)
        , buffer_offset_(buffer_offset) {
        
        buffer_.resize(window_height_);
        for (auto& row : buffer_) {
            row.resize(width_);
        }
    }
    
    /**
     * Initialize buffer with data from the source image
     * @param src Source image accessor
     * @param start_y Starting y position in the source image
     */
    template<typename ImageAccessor>
    void initialize(const ImageAccessor& src, int start_y = 0) {
        current_y_ = start_y;
        
        // Load all initial rows
        for (int i = 0; i < window_height_; ++i) {
            int src_y = start_y + buffer_offset_ + i;
            loadRow(src, src_y);
        }
    }
    
    /**
     * Advance the sliding window by one row
     * @param src Source image accessor
     */
    template<typename ImageAccessor>
    void advance(const ImageAccessor& src) {
        current_y_++;
        
        // Load the new row that enters the window
        int new_src_row = current_y_ + buffer_offset_ + window_height_ - 1;
        loadRow(src, new_src_row);
    }
    
    /**
     * Get a pixel from the buffer
     * @param x X coordinate (0-based, without padding)
     * @param y_offset Offset from current_y (e.g., -1, 0, 1)
     * @return The pixel value
     */
    PixelType get(int x, int y_offset) const {
        int src_row = current_y_ + y_offset;
        int buffer_idx = rowToBufferIndex(src_row);
        
        assert(buffer_idx >= 0 && buffer_idx < window_height_);
        assert(x + padding_ >= 0 && x + padding_ < width_);
        
        return buffer_[buffer_idx][x + padding_];
    }
    
    /**
     * Get a reference to an entire row
     * @param y_offset Offset from current_y
     * @return Reference to the row vector
     */
    const std::vector<PixelType>& getRow(int y_offset) const {
        int src_row = current_y_ + y_offset;
        int buffer_idx = rowToBufferIndex(src_row);
        
        assert(buffer_idx >= 0 && buffer_idx < window_height_);
        return buffer_[buffer_idx];
    }
    
    /**
     * Get the current y position in the source image
     */
    int getCurrentY() const { return current_y_; }
    
    /**
     * Get the padding amount
     */
    int getPadding() const { return padding_; }
    
private:
    /**
     * Load a row from the source image into the buffer
     */
    template<typename ImageAccessor>
    void loadRow(const ImageAccessor& src, int src_y) {
        int buffer_idx = rowToBufferIndex(src_y);
        
        for (int x = 0; x < width_; ++x) {
            int src_x = x - padding_;
            buffer_[buffer_idx][x] = src.safeAccess(src_x, src_y);
        }
    }
};

/**
 * Helper factory function for creating sliding window buffers
 */
template<typename PixelType>
auto makeSlidingWindowBuffer(int window_height, int image_width, int padding, int buffer_offset) {
    return SlidingWindowBuffer<PixelType>(window_height, image_width, padding, buffer_offset);
}

// Specialized versions for common window sizes
template<typename PixelType>
class SlidingWindow3x3 : public SlidingWindowBuffer<PixelType> {
public:
    SlidingWindow3x3(int image_width)
        : SlidingWindowBuffer<PixelType>(3, image_width, 1, -1) {}
    
    // Convenience accessors for 3x3 neighborhood
    PixelType getTopLeft(int x) const { return this->get(x - 1, -1); }
    PixelType getTop(int x) const { return this->get(x, -1); }
    PixelType getTopRight(int x) const { return this->get(x + 1, -1); }
    PixelType getLeft(int x) const { return this->get(x - 1, 0); }
    PixelType getCenter(int x) const { return this->get(x, 0); }
    PixelType getRight(int x) const { return this->get(x + 1, 0); }
    PixelType getBottomLeft(int x) const { return this->get(x - 1, 1); }
    PixelType getBottom(int x) const { return this->get(x, 1); }
    PixelType getBottomRight(int x) const { return this->get(x + 1, 1); }
};

template<typename PixelType>
class SlidingWindow4x4 : public SlidingWindowBuffer<PixelType> {
public:
    SlidingWindow4x4(int image_width)
        : SlidingWindowBuffer<PixelType>(4, image_width, 1, -1) {}
    
    // Get pixels in 4x4 pattern for 2xSaI (from y-1 to y+2, x-1 to x+2)
    void get4x4(int x, PixelType out[4][4]) const {
        for (int dy = -1; dy <= 2; ++dy) {
            for (int dx = -1; dx <= 2; ++dx) {
                out[dy + 1][dx + 1] = this->get(x + dx, dy);
            }
        }
    }
};

template<typename PixelType>
class SlidingWindow5x5 : public SlidingWindowBuffer<PixelType> {
public:
    SlidingWindow5x5(int image_width)
        : SlidingWindowBuffer<PixelType>(5, image_width, 2, -2) {}
    
    // Get all pixels for a 5x5 neighborhood centered at (x, current_y)
    void getNeighborhood(int x, PixelType neighborhood[5][5]) const {
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                neighborhood[dy + 2][dx + 2] = this->get(x + dx, dy);
            }
        }
    }
};