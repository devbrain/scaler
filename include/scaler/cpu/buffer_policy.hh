#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace scaler {
    // Buffer policy interface - determines how row buffers are allocated
    template<typename PixelType>
    class dynamic_buffer_policy {
        public:
            using BufferType = std::vector <PixelType>;

            explicit dynamic_buffer_policy(size_t width)
                : width_(width) {
            }

            BufferType allocate() const {
                return BufferType(width_ + 2);
            }

            PixelType* data(BufferType& buffer) {
                return buffer.data();
            }

            const PixelType* data(const BufferType& buffer) const {
                return buffer.data();
            }

        private:
            size_t width_;
    };

    // Fixed-size buffer policy for better performance
    template<typename PixelType, size_t MaxWidth = 4096>
    class fixed_buffer_policy {
        public:
            using BufferType = std::array <PixelType, MaxWidth + 2>;

            explicit fixed_buffer_policy(size_t width)
                : width_(width) {
                if (width > MaxWidth) {
                    throw std::runtime_error("Image width exceeds fixed buffer capacity");
                }
            }

            BufferType allocate() const {
                return BufferType{};
            }

            PixelType* data(BufferType& buffer) {
                return buffer.data();
            }

            const PixelType* data(const BufferType& buffer) const {
                return buffer.data();
            }

            [[nodiscard]] size_t width() const { return width_; }

        private:
            size_t width_;
    };

    // Automatic policy selection based on width
    template<typename PixelType>
    class auto_buffer_policy {
        public:
            static constexpr size_t FIXED_BUFFER_THRESHOLD = 4096;

            explicit auto_buffer_policy(size_t width)
                : width_(width) {
            }

            [[nodiscard]] bool use_fixed_buffer() const {
                return width_ <= FIXED_BUFFER_THRESHOLD;
            }

            [[nodiscard]] size_t width() const { return width_; }

        private:
            size_t width_;
    };

    // Row buffer manager using the specified policy
    template<typename PixelType, typename Policy>
    class row_buffer_manager {
        public:
            using BufferType = typename Policy::BufferType;

            explicit row_buffer_manager(size_t width)
                : policy_(width) {
                prev_row_ = policy_.allocate();
                curr_row_ = policy_.allocate();
                next_row_ = policy_.allocate();
            }

            template<typename ImageType>
            void initialize_rows(const ImageType& src, int y) {
                auto* prev = policy_.data(prev_row_);
                auto* curr = policy_.data(curr_row_);

                for (size_t x = 0; x < src.width() + 2; ++x) {
                    prev[x] = src.safe_access(static_cast <int>(x) - 1, y - 1);
                    curr[x] = src.safe_access(static_cast <int>(x) - 1, y);
                }
            }

            template<typename ImageType>
            void load_next_row(const ImageType& src, int y) {
                auto* next = policy_.data(next_row_);
                int next_y = (y < static_cast <int>(src.height()) - 1) ? y + 1 : y;

                for (size_t x = 0; x < src.width() + 2; ++x) {
                    next[x] = src.safe_access(static_cast <int>(x) - 1, next_y);
                }
            }

            void rotate_rows() {
                std::swap(prev_row_, curr_row_);
                std::swap(curr_row_, next_row_);
            }

            void get_neighborhood(int x, PixelType* w) const {
                const auto* prev = policy_.data(prev_row_);
                const auto* curr = policy_.data(curr_row_);
                const auto* next = policy_.data(next_row_);

                int idx = x + 1; // Offset by 1 for padding
                w[0] = prev[idx - 1];
                w[1] = prev[idx];
                w[2] = prev[idx + 1];
                w[3] = curr[idx - 1];
                w[4] = curr[idx];
                w[5] = curr[idx + 1];
                w[6] = next[idx - 1];
                w[7] = next[idx];
                w[8] = next[idx + 1];
            }

        private:
            Policy policy_;
            BufferType prev_row_;
            BufferType curr_row_;
            BufferType next_row_;
    };
} // namespace scaler
