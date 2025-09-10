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
protected:  // Changed from private to protected for derived class access
    std::vector<std::vector<PixelType>> buffer_;
    int window_height_;      // Number of rows in the window (e.g., 3 for 3x3, 5 for 5x5)
    int width_;              // Width of each row (image width + padding)
    int padding_;            // Padding on each side for boundary pixels
    int current_y_;          // Current y position in the source image
    int buffer_offset_;      // Offset from current_y to first buffer row
    
    // Maps a source row index to buffer index
    inline int rowToBufferIndex(int src_row) const noexcept {
        // Optimize modulo for positive numbers and power-of-2 sizes
        // For non-power-of-2, use single modulo for positive src_row
        int idx = src_row % window_height_;
        // Handle negative case (only happens at image boundaries)
        return (idx < 0) ? (idx + window_height_) : idx;
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
    inline PixelType get(int x, int y_offset) const noexcept {
        const int src_row = current_y_ + y_offset;
        const int buffer_idx = rowToBufferIndex(src_row);
        
#ifdef DEBUG
        assert(buffer_idx >= 0 && buffer_idx < window_height_);
        assert(x + padding_ >= 0 && x + padding_ < width_);
#endif
        
        return buffer_[buffer_idx][x + padding_];
    }
    
    /**
     * Get a reference to an entire row
     * @param y_offset Offset from current_y
     * @return Reference to the row vector
     */
    inline const std::vector<PixelType>& getRow(int y_offset) const noexcept {
        const int src_row = current_y_ + y_offset;
        const int buffer_idx = rowToBufferIndex(src_row);
        
#ifdef DEBUG
        assert(buffer_idx >= 0 && buffer_idx < window_height_);
#endif
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
    explicit SlidingWindow3x3(int image_width)
        : SlidingWindowBuffer<PixelType>(3, image_width, 1, -1) {}
    
    // Get all 3x3 pixels with a single row lookup per row - more efficient
    inline void get3x3(int x, PixelType& tl, PixelType& t, PixelType& tr,
                       PixelType& l, PixelType& c, PixelType& r,
                       PixelType& bl, PixelType& b, PixelType& br) const noexcept {
        const int xp = x + this->getPadding();
        const auto& topRow = this->getRow(-1);
        const auto& midRow = this->getRow(0);
        const auto& botRow = this->getRow(1);
        
        tl = topRow[xp - 1]; t = topRow[xp]; tr = topRow[xp + 1];
        l = midRow[xp - 1]; c = midRow[xp]; r = midRow[xp + 1];
        bl = botRow[xp - 1]; b = botRow[xp]; br = botRow[xp + 1];
    }
    
    // Keep individual accessors for compatibility but optimize them
    inline PixelType getTopLeft(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ - 1)][x + this->padding_ - 1];
    }
    inline PixelType getTop(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ - 1)][x + this->padding_];
    }
    inline PixelType getTopRight(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ - 1)][x + this->padding_ + 1];
    }
    inline PixelType getLeft(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_)][x + this->padding_ - 1];
    }
    inline PixelType getCenter(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_)][x + this->padding_];
    }
    inline PixelType getRight(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_)][x + this->padding_ + 1];
    }
    inline PixelType getBottomLeft(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ + 1)][x + this->padding_ - 1];
    }
    inline PixelType getBottom(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ + 1)][x + this->padding_];
    }
    inline PixelType getBottomRight(int x) const noexcept { 
        return this->buffer_[this->rowToBufferIndex(this->current_y_ + 1)][x + this->padding_ + 1];
    }
    
private:
    // Optimized modulo for size 3
    inline int rowToBufferIndex(int src_row) const noexcept {
        int idx = src_row % 3;
        return (idx < 0) ? (idx + 3) : idx;
    }
};

template<typename PixelType>
class SlidingWindow4x4 : public SlidingWindowBuffer<PixelType> {
public:
    explicit SlidingWindow4x4(int image_width)
        : SlidingWindowBuffer<PixelType>(4, image_width, 1, -1) {}
    
    // Get pixels in 4x4 pattern for 2xSaI (from y-1 to y+2, x-1 to x+2)
    inline void get4x4(int x, PixelType out[4][4]) const noexcept {
        const int xp = x + this->padding_;
        
        // Unroll loops and use direct buffer access with optimized modulo
        const auto& row0 = this->buffer_[rowToBufferIndex(this->current_y_ - 1)];
        const auto& row1 = this->buffer_[rowToBufferIndex(this->current_y_)];
        const auto& row2 = this->buffer_[rowToBufferIndex(this->current_y_ + 1)];
        const auto& row3 = this->buffer_[rowToBufferIndex(this->current_y_ + 2)];
        
        // Load all 16 pixels with minimal index calculations
        out[0][0] = row0[xp - 1]; out[0][1] = row0[xp]; out[0][2] = row0[xp + 1]; out[0][3] = row0[xp + 2];
        out[1][0] = row1[xp - 1]; out[1][1] = row1[xp]; out[1][2] = row1[xp + 1]; out[1][3] = row1[xp + 2];
        out[2][0] = row2[xp - 1]; out[2][1] = row2[xp]; out[2][2] = row2[xp + 1]; out[2][3] = row2[xp + 2];
        out[3][0] = row3[xp - 1]; out[3][1] = row3[xp]; out[3][2] = row3[xp + 1]; out[3][3] = row3[xp + 2];
    }
    
private:
    // Optimized modulo for power of 2 (size 4)
    inline int rowToBufferIndex(int src_row) const noexcept {
        // For power of 2, we can use bitwise AND which is much faster than modulo
        return src_row & 3;  // Equivalent to src_row % 4 for positive numbers
    }
};

template<typename PixelType>
class SlidingWindow5x5 : public SlidingWindowBuffer<PixelType> {
public:
    explicit SlidingWindow5x5(int image_width)
        : SlidingWindowBuffer<PixelType>(5, image_width, 2, -2) {}
    
    // Get all pixels for a 5x5 neighborhood centered at (x, current_y)
    inline void getNeighborhood(int x, PixelType neighborhood[5][5]) const noexcept {
        const int xp = x + this->padding_;
        
        // Unroll loops and use direct buffer access
        const auto& row0 = this->buffer_[rowToBufferIndex(this->current_y_ - 2)];
        const auto& row1 = this->buffer_[rowToBufferIndex(this->current_y_ - 1)];
        const auto& row2 = this->buffer_[rowToBufferIndex(this->current_y_)];
        const auto& row3 = this->buffer_[rowToBufferIndex(this->current_y_ + 1)];
        const auto& row4 = this->buffer_[rowToBufferIndex(this->current_y_ + 2)];
        
        // Load all 25 pixels with minimal index calculations
        neighborhood[0][0] = row0[xp - 2]; neighborhood[0][1] = row0[xp - 1]; 
        neighborhood[0][2] = row0[xp]; neighborhood[0][3] = row0[xp + 1]; neighborhood[0][4] = row0[xp + 2];
        
        neighborhood[1][0] = row1[xp - 2]; neighborhood[1][1] = row1[xp - 1]; 
        neighborhood[1][2] = row1[xp]; neighborhood[1][3] = row1[xp + 1]; neighborhood[1][4] = row1[xp + 2];
        
        neighborhood[2][0] = row2[xp - 2]; neighborhood[2][1] = row2[xp - 1]; 
        neighborhood[2][2] = row2[xp]; neighborhood[2][3] = row2[xp + 1]; neighborhood[2][4] = row2[xp + 2];
        
        neighborhood[3][0] = row3[xp - 2]; neighborhood[3][1] = row3[xp - 1]; 
        neighborhood[3][2] = row3[xp]; neighborhood[3][3] = row3[xp + 1]; neighborhood[3][4] = row3[xp + 2];
        
        neighborhood[4][0] = row4[xp - 2]; neighborhood[4][1] = row4[xp - 1]; 
        neighborhood[4][2] = row4[xp]; neighborhood[4][3] = row4[xp + 1]; neighborhood[4][4] = row4[xp + 2];
    }
    
private:
    // Optimized modulo for size 5
    inline int rowToBufferIndex(int src_row) const noexcept {
        int idx = src_row % 5;
        return (idx < 0) ? (idx + 5) : idx;
    }
};