#pragma once

#include <vector>
#include <array>
#ifdef DEBUG
#include <cassert>
#endif
#include <stdexcept>
namespace scaler {
    /**
     * A cache-friendly sliding window buffer for image processing algorithms.
     *
     * This class manages a circular buffer of image rows to improve cache locality
     * when accessing pixels in a neighborhood pattern (e.g., 3x3, 5x5 windows).
     *
     * @tparam PixelType The type of pixel data stored in the buffer
     */
    template<typename PixelType>
    class sliding_window_buffer {
        protected:  // Changed from private to protected for derived class access
            std::vector<std::vector<PixelType>> buffer_;
            int window_height_;      // Number of rows in the window (e.g., 3 for 3x3, 5 for 5x5)
            size_t width_;           // Width of each row (image width + padding)
            int padding_;            // Padding on each side for boundary pixels
            size_t current_y_;       // Current y position in the source image
            int buffer_offset_;      // Offset from current_y to first buffer row

            // Maps a source row index to buffer index
            [[nodiscard]] size_t row_to_buffer_index(int src_row) const noexcept {
                // Optimize modulo for positive numbers and power-of-2 sizes
                // For non-power-of-2, use single modulo for positive src_row
                int idx = src_row % window_height_;
                // Handle negative case (only happens at image boundaries)
                return static_cast<size_t>((idx < 0) ? (idx + window_height_) : idx);
            }

        public:
            /**
             * Initialize the sliding window buffer
             * @param window_height Height of the sliding window (e.g., 3 for 3x3 kernel)
             * @param image_width Width of the source image
             * @param padding Padding on each side for boundary access
             * @param buffer_offset Offset from current position (e.g., -1 for centered window)
             */
            sliding_window_buffer(int window_height, size_t image_width, int padding, int buffer_offset)
                : window_height_(window_height)
                , width_(image_width + 2 * static_cast<size_t>(padding))
                , padding_(padding)
                , current_y_(0)
                , buffer_offset_(buffer_offset) {

                buffer_.resize(static_cast<size_t>(window_height_));
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
            void initialize(const ImageAccessor& src, size_t start_y = 0) {
                current_y_ = start_y;

                // Load all initial rows
                for (int i = 0; i < window_height_; ++i) {
                    int src_y = static_cast<int>(start_y) + buffer_offset_ + i;
                    load_row(src, src_y);
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
                int new_src_row = static_cast<int>(current_y_) + buffer_offset_ + window_height_ - 1;
                load_row(src, new_src_row);
            }

            /**
             * Get a pixel from the buffer
             * @param x X coordinate (0-based, without padding)
             * @param y_offset Offset from current_y (e.g., -1, 0, 1)
             * @return The pixel value
             */
            inline PixelType get(size_t x, int y_offset) const noexcept {
                const int src_row = static_cast<int>(current_y_) + y_offset;
                const size_t buffer_idx = row_to_buffer_index(src_row);
                const size_t x_idx = x + static_cast<size_t>(padding_);

#ifdef DEBUG
                assert(buffer_idx < static_cast<size_t>(window_height_));
                assert(x_idx < width_);
#endif

                return buffer_[buffer_idx][x_idx];
            }

            /**
             * Get a reference to an entire row
             * @param y_offset Offset from current_y
             * @return Reference to the row vector
             */
            inline const std::vector<PixelType>& get_row(int y_offset) const noexcept {
                const int src_row = static_cast<int>(current_y_) + y_offset;
                const size_t buffer_idx = row_to_buffer_index(src_row);

#ifdef DEBUG
                assert(buffer_idx < static_cast<size_t>(window_height_));
#endif
                return buffer_[buffer_idx];
            }

            /**
             * Get the current y position in the source image
             */
            [[nodiscard]] size_t get_current_y() const { return current_y_; }

            /**
             * Get the padding amount
             */
            [[nodiscard]] int get_padding() const { return padding_; }

        private:
            /**
             * Load a row from the source image into the buffer
             */
            template<typename ImageAccessor>
            void load_row(const ImageAccessor& src, int src_y) {
                size_t buffer_idx = row_to_buffer_index(src_y);

                for (size_t x = 0; x < width_; ++x) {
                    int src_x = static_cast<int>(x) - padding_;
                    buffer_[buffer_idx][x] = src.safe_access(src_x, src_y);
                }
            }
    };

    /**
     * Helper factory function for creating sliding window buffers
     */
    template<typename PixelType>
    auto make_sliding_window_buffer(int window_height, size_t image_width, int padding, int buffer_offset) {
        return sliding_window_buffer<PixelType>(window_height, image_width, padding, buffer_offset);
    }

    // Specialized versions for common window sizes
    template<typename PixelType>
    class sliding_window_3x3 : public sliding_window_buffer<PixelType> {
        public:
            explicit sliding_window_3x3(size_t image_width)
                : sliding_window_buffer<PixelType>(3, image_width, 1, -1) {}

            // Get all 3x3 pixels with a single row lookup per row - more efficient
            inline void get3x3(size_t x, PixelType& tl, PixelType& t, PixelType& tr,
                               PixelType& l, PixelType& c, PixelType& r,
                               PixelType& bl, PixelType& b, PixelType& br) const noexcept {
                const size_t xp = x + static_cast<size_t>(this->get_padding());
                const auto& topRow = this->get_row(-1);
                const auto& midRow = this->get_row(0);
                const auto& botRow = this->get_row(1);

                tl = topRow[xp - 1]; t = topRow[xp]; tr = topRow[xp + 1];
                l = midRow[xp - 1]; c = midRow[xp]; r = midRow[xp + 1];
                bl = botRow[xp - 1]; b = botRow[xp]; br = botRow[xp + 1];
            }

            // Keep individual accessors for compatibility but optimize them
            inline PixelType get_top_left(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) - 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_ - 1);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_top(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) - 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_top_right(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) - 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_ + 1);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_left(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_));
                size_t col_idx = x + static_cast<size_t>(this->padding_ - 1);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_center(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_));
                size_t col_idx = x + static_cast<size_t>(this->padding_);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_right(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_));
                size_t col_idx = x + static_cast<size_t>(this->padding_ + 1);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_bottom_left(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) + 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_ - 1);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_bottom(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) + 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_);
                return this->buffer_[row_idx][col_idx];
            }
            inline PixelType get_bottom_right(size_t x) const noexcept {
                size_t row_idx = this->row_to_buffer_index(static_cast<int>(this->current_y_) + 1);
                size_t col_idx = x + static_cast<size_t>(this->padding_ + 1);
                return this->buffer_[row_idx][col_idx];
            }

            // Get all pixels for a 3x3 neighborhood centered at (x, current_y)
            inline void get_neighborhood(size_t x, PixelType neighborhood[3][3]) const noexcept {
                const size_t xp = x + static_cast<size_t>(this->get_padding());
                const auto& topRow = this->get_row(-1);
                const auto& midRow = this->get_row(0);
                const auto& botRow = this->get_row(1);

                neighborhood[0][0] = topRow[xp - 1]; neighborhood[0][1] = topRow[xp]; neighborhood[0][2] = topRow[xp + 1];
                neighborhood[1][0] = midRow[xp - 1]; neighborhood[1][1] = midRow[xp]; neighborhood[1][2] = midRow[xp + 1];
                neighborhood[2][0] = botRow[xp - 1]; neighborhood[2][1] = botRow[xp]; neighborhood[2][2] = botRow[xp + 1];
            }

        private:
            // Optimized modulo for size 3
            [[nodiscard]] size_t row_to_buffer_index(int src_row) const noexcept {
                int idx = src_row % 3;
                return static_cast<size_t>((idx < 0) ? (idx + 3) : idx);
            }
    };

    template<typename PixelType>
    class sliding_window_4x4 : public sliding_window_buffer<PixelType> {
        public:
            explicit sliding_window_4x4(size_t image_width)
                : sliding_window_buffer<PixelType>(4, image_width, 2, -1) {}

            // Get pixels in 4x4 pattern for 2xSaI (from y-1 to y+2, x-1 to x+2)
            inline void get4x4(size_t x, PixelType out[4][4]) const noexcept {
                const size_t xp = x + static_cast<size_t>(this->padding_);

                // Unroll loops and use direct buffer access with optimized modulo
                const auto& row0 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) - 1)];
                const auto& row1 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_))];
                const auto& row2 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) + 1)];
                const auto& row3 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) + 2)];

                // Load all 16 pixels with minimal index calculations
                out[0][0] = row0[xp - 1]; out[0][1] = row0[xp]; out[0][2] = row0[xp + 1]; out[0][3] = row0[xp + 2];
                out[1][0] = row1[xp - 1]; out[1][1] = row1[xp]; out[1][2] = row1[xp + 1]; out[1][3] = row1[xp + 2];
                out[2][0] = row2[xp - 1]; out[2][1] = row2[xp]; out[2][2] = row2[xp + 1]; out[2][3] = row2[xp + 2];
                out[3][0] = row3[xp - 1]; out[3][1] = row3[xp]; out[3][2] = row3[xp + 1]; out[3][3] = row3[xp + 2];
            }

        private:
            // Optimized modulo for power of 2 (size 4)
            [[nodiscard]] size_t row_to_buffer_index(int src_row) const noexcept {
                // For power of 2, we can use bitwise AND which is much faster than modulo
                return static_cast<size_t>(src_row & 3);  // Equivalent to src_row % 4 for positive numbers
            }
    };

    template<typename PixelType>
    class sliding_window_5x5 : public sliding_window_buffer<PixelType> {
        public:
            explicit sliding_window_5x5(size_t image_width)
                : sliding_window_buffer<PixelType>(5, image_width, 2, -2) {}

            // Get all pixels for a 5x5 neighborhood centered at (x, current_y)
            void get_neighborhood(size_t x, PixelType neighborhood[5][5]) const noexcept {
                const size_t xp = x + static_cast<size_t>(this->padding_);

                // Unroll loops and use direct buffer access
                const auto& row0 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) - 2)];
                const auto& row1 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) - 1)];
                const auto& row2 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_))];
                const auto& row3 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) + 1)];
                const auto& row4 = this->buffer_[row_to_buffer_index(static_cast<int>(this->current_y_) + 2)];

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
            [[nodiscard]] size_t row_to_buffer_index(int src_row) const noexcept {
                int idx = src_row % 5;
                return static_cast<size_t>((idx < 0) ? (idx + 5) : idx);
            }
    };

    /**
     * Fast sliding window for 3x3 neighborhoods using fixed-size arrays
     * Optimized for images up to 4096 pixels wide
     */
    template<typename PixelType, size_t MaxWidth = 4096>
    class fast_sliding_window_3x3 {
    private:
        static constexpr int WINDOW_HEIGHT = 3;
        static constexpr int PADDING = 1;
        
        // Fixed-size row buffers - stack allocated, no heap allocation
        alignas(64) std::array<PixelType, MaxWidth + 2> buffer_[WINDOW_HEIGHT];
        size_t width_;           // Actual image width
        size_t current_y_;       // Current y position in the source image
        
        // Maps a source row index to buffer index (optimized for 3)
        [[nodiscard]] size_t row_to_buffer_index(int src_row) const noexcept {
            // Optimized modulo for size 3
            int idx = src_row % 3;
            return static_cast<size_t>((idx < 0) ? (idx + 3) : idx);
        }
        
    public:
        explicit fast_sliding_window_3x3(size_t image_width)
            : width_(image_width), current_y_(0) {
            if (image_width > MaxWidth) {
                throw std::runtime_error("Image width exceeds fast_sliding_window_3x3 capacity");
            }
        }
        
        // Initialize buffer with first rows from the source image
        template<typename ImageType>
        void initialize(const ImageType& src, size_t start_y) {
            current_y_ = start_y;
            
            // Load all 3 rows
            for (int dy = -1; dy <= 1; ++dy) {
                auto& row = buffer_[row_to_buffer_index(static_cast<int>(start_y) + dy)];
                for (int x = -PADDING; x < static_cast<int>(width_ + PADDING); ++x) {
                    row[static_cast<size_t>(x + PADDING)] = src.safe_access(x, static_cast<int>(start_y) + dy);
                }
            }
        }
        
        // Advance to the next row
        template<typename ImageType>
        void advance(const ImageType& src) {
            current_y_++;
            
            // Load the new row (current_y + 1) into the buffer
            auto& new_row = buffer_[row_to_buffer_index(static_cast<int>(current_y_) + 1)];
            for (int x = -PADDING; x < static_cast<int>(width_ + PADDING); ++x) {
                new_row[static_cast<size_t>(x + PADDING)] = src.safe_access(x, static_cast<int>(current_y_) + 1);
            }
        }
        
        // Get a row relative to current position
        const std::array<PixelType, MaxWidth + 2>& get_row(int offset) const noexcept {
            return buffer_[row_to_buffer_index(static_cast<int>(current_y_) + offset)];
        }
        
        // Get padding amount
        [[nodiscard]] int get_padding() const noexcept { return PADDING; }
        
        // Get all 3x3 pixels with a single row lookup per row
        void get_3x3(size_t x, PixelType& tl, PixelType& t, PixelType& tr,
                    PixelType& l, PixelType& c, PixelType& r,
                    PixelType& bl, PixelType& b, PixelType& br) const noexcept {
            const size_t xp = x + static_cast<size_t>(PADDING);
            const auto& topRow = get_row(-1);
            const auto& midRow = get_row(0);
            const auto& botRow = get_row(1);
            
            tl = topRow[xp - 1]; t = topRow[xp]; tr = topRow[xp + 1];
            l = midRow[xp - 1]; c = midRow[xp]; r = midRow[xp + 1];
            bl = botRow[xp - 1]; b = botRow[xp]; br = botRow[xp + 1];
        }
        
        // Get neighborhood as array
        void get_neighborhood(size_t x, PixelType neighborhood[9]) const noexcept {
            const size_t xp = x + static_cast<size_t>(PADDING);
            const auto& topRow = get_row(-1);
            const auto& midRow = get_row(0);
            const auto& botRow = get_row(1);
            
            neighborhood[0] = topRow[xp - 1]; neighborhood[1] = topRow[xp]; neighborhood[2] = topRow[xp + 1];
            neighborhood[3] = midRow[xp - 1]; neighborhood[4] = midRow[xp]; neighborhood[5] = midRow[xp + 1];
            neighborhood[6] = botRow[xp - 1]; neighborhood[7] = botRow[xp]; neighborhood[8] = botRow[xp + 1];
        }
    };
    
    /**
     * Automatic sliding window selector - uses fast version for small images
     */
    template<typename PixelType>
    auto make_sliding_window_3x3(size_t image_width) {
        if (image_width <= 4096) {
            return fast_sliding_window_3x3<PixelType, 4096>(image_width);
        } else {
            return sliding_window_3x3<PixelType>(image_width);
        }
    }
}