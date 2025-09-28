#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define SCALER_PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#define SCALER_PLATFORM_MACOS
#elif defined(__linux__)
#define SCALER_PLATFORM_LINUX
#elif defined(__unix__)
#define SCALER_PLATFORM_UNIX
#else
#error "Unknown platform"
#endif

// OpenGL headers - platform specific
#ifdef SCALER_PLATFORM_WINDOWS
// Windows requires windows.h before GL headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
// Function pointer loading might be needed
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#elif defined(SCALER_PLATFORM_MACOS)
// macOS uses different OpenGL headers
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
// Linux/Unix uses standard GL headers
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// SDL headers
#include <SDL.h>
#include <SDL_opengl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <cstring>
#include <mutex>
#include <iostream>

namespace scaler::gpu {
    // RAII wrapper for OpenGL resources
    template<typename Deleter>
    class GLResource {
        private:
            GLuint id_;
            Deleter deleter_;
            bool owned_;

        public:
            GLResource()
                : id_(0), owned_(false) {
            }

            GLResource(GLuint id, Deleter del)
                : id_(id), deleter_(del), owned_(true) {
            }

            ~GLResource() { release(); }

            // Move only
            GLResource(GLResource&& other) noexcept
                : id_(other.id_), deleter_(std::move(other.deleter_)), owned_(other.owned_) {
                other.owned_ = false;
            }

            GLResource& operator=(GLResource&& other) noexcept {
                if (this != &other) {
                    release();
                    id_ = other.id_;
                    deleter_ = std::move(other.deleter_);
                    owned_ = other.owned_;
                    other.owned_ = false;
                }
                return *this;
            }

            GLResource(const GLResource&) = delete;
            GLResource& operator=(const GLResource&) = delete;

            void release() {
                if (owned_ && id_ != 0) {
                    deleter_(id_);
                    id_ = 0;
                    owned_ = false;
                }
            }

            GLuint get() const { return id_; }
            GLuint* ptr() { return &id_; }
            operator GLuint() const { return id_; }
    };

    // Helper to check OpenGL errors
    inline void checkGLError(const char* operation) {
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            throw std::runtime_error(std::string("OpenGL error in ") + operation + ": " + std::to_string(error));
        }
    }

    class SDLOpenGLMultiScaler {
        public:
            // Algorithm type alias for compatibility
            enum Algorithm {
                DEFAULT,
                EPX,
                Scale2x,
                Scale2xSFX,
                Scale3x,
                Scale3xSFX,
                Scale4x,
                Eagle,
                TwoXSaI,
                HQ2x,
                HQ3x,
                XBR,
                OmniScale,
                AAScale2x,
                AAScale4x,
                AdvMAME2x,
                AdvMAME3x,
                BILINEAR
            };

            // Keep ScalingMethod for backward compatibility
            using ScalingMethod = Algorithm;

        private:
            SDL_Window* window_ = nullptr;
            SDL_GLContext gl_context_ = nullptr;
            bool owns_window_ = false;  // Track whether we created and own the window
            bool owns_context_ = false; // Track whether we created and own the GL context
            GLuint framebuffer_ = 0;
            GLuint texture_ = 0;

            struct ShaderProgram {
                GLuint program = 0;
                GLint u_texture = -1;
                GLint u_texture_size = -1;
                GLint u_output_size = -1;
                GLint u_scale = -1;
            };

            // Thread safety: This class is NOT thread-safe.
            // All methods must be called from the same thread that created the OpenGL context.
            // If multi-threaded access is needed, external synchronization is required.
            mutable std::mutex shader_mutex_; // Protects shader_cache_ and current_shader_

            // Shader cache - maps algorithm to compiled shader program
            std::unordered_map<Algorithm, ShaderProgram> shader_cache_;
            Algorithm current_algorithm_ = DEFAULT;
            ShaderProgram* current_shader_ = nullptr;

            GLuint vao_ = 0;
            GLuint vbo_ = 0;

            int window_width_ = 800;
            int window_height_ = 600;
            bool initialized_ = false;

            // Performance optimization: reusable pixel buffer
            mutable std::vector <unsigned char> pixel_buffer_;

            // Vertex shader - common for all scaling algorithms
            static constexpr const char* vertex_shader_source = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec2 texCoord;
        out vec2 v_texCoord;

        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            v_texCoord = texCoord;
        }
    )";

            // Default passthrough fragment shader
            static constexpr const char* default_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;

        void main() {
            FragColor = texture(u_texture, v_texCoord);
        }
    )";

            // Scale2x shader - the original algorithm
            static constexpr const char* scale2x_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;
            vec2 block_pos = fract(output_pixel / 2.0);
            vec2 src_tex_coord = v_texCoord;

            // Sample the 3x3 neighborhood
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 E = sampleTexture(src_tex_coord);
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));

            vec4 result;

            // Scale2x rules (simpler than EPX)
            if (B != H && D != F) {
                if (block_pos.x < 0.5) {
                    result = (block_pos.y < 0.5) ? (D == B ? D : E) : (D == H ? D : E);
                } else {
                    result = (block_pos.y < 0.5) ? (B == F ? F : E) : (H == F ? F : E);
                }
            } else {
                result = E;
            }

            FragColor = result;
        }
    )";

            // EPX shader
            static constexpr const char* epx_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        bool threeOrMoreIdentical(vec4 a, vec4 b, vec4 c, vec4 d) {
            int equal_pairs = 0;
            if (a == b) equal_pairs++;
            if (a == c) equal_pairs++;
            if (a == d) equal_pairs++;
            if (b == c) equal_pairs++;
            if (b == d) equal_pairs++;
            if (c == d) equal_pairs++;
            return equal_pairs >= 3;
        }

        vec4 sampleTexture(vec2 pos) {
            // Clamp to texture bounds to handle edge cases
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;

            // Get the output pixel position
            vec2 output_pixel = v_texCoord * u_output_size;

            // Get source pixel coordinates (which input pixel are we upscaling?)
            vec2 src_pixel = floor(output_pixel / 2.0);

            // Get the position within the 2x2 output block (0-1 range)
            vec2 block_pos = fract(output_pixel / 2.0);

            // Calculate the texture coordinate for the center of the source pixel
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 3x3 neighborhood with clamping for edges
            vec4 P = sampleTexture(src_tex_coord);  // center (original_pixel)
            vec4 A = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));  // top
            vec4 B = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));   // right
            vec4 C = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));  // left
            vec4 D = sampleTexture(src_tex_coord + vec2(0.0, texel.y));   // bottom

            // Initial assignments - all quadrants start with center pixel
            vec4 one = P;    // top-left
            vec4 two = P;    // top-right
            vec4 three = P;  // bottom-left
            vec4 four = P;   // bottom-right

            // EPX interpolation rules (matching CPU implementation exactly)
            if (C == A) one = A;
            if (A == B) two = B;
            if (D == C) three = C;
            if (B == D) four = D;

            // If 3 or more neighbors are identical, use original pixel
            if (threeOrMoreIdentical(A, B, C, D)) {
                one = two = three = four = P;
            }

            // Select the appropriate quadrant based on position within 2x2 block
            vec4 result;
            if (block_pos.x < 0.5) {
                if (block_pos.y < 0.5) {
                    result = one;    // top-left
                } else {
                    result = three;  // bottom-left
                }
            } else {
                if (block_pos.y < 0.5) {
                    result = two;    // top-right
                } else {
                    result = four;   // bottom-right
                }
            }

            FragColor = result;
        }
    )";

            // Scale4x shader - Standard Scale4x (Scale2x applied twice without AA)
            static constexpr const char* scale4x_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        vec4 scale2x_pixel(vec4 E, vec4 B, vec4 D, vec4 F, vec4 H, int quadrant) {
            vec4 result = E;
            if (B != H && D != F) {
                if (quadrant == 0) result = (D == B) ? D : E;  // top-left
                else if (quadrant == 1) result = (B == F) ? F : E;  // top-right
                else if (quadrant == 2) result = (D == H) ? D : E;  // bottom-left
                else if (quadrant == 3) result = (H == F) ? F : E;  // bottom-right
            }
            return result;
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;

            // For 4x scaling, we need to determine which original pixel and which sub-pixel
            vec2 src_pixel = floor(output_pixel / 4.0);
            vec2 pos_in_4x4 = fract(output_pixel / 4.0) * 4.0;

            // First, determine which 2x2 block within the 4x4 we're in
            int block_2x2 = int(pos_in_4x4.x / 2.0) + int(pos_in_4x4.y / 2.0) * 2;

            // Position within that 2x2 block
            vec2 pos_in_2x2 = fract(pos_in_4x4 / 2.0) * 2.0;
            int final_quad = int(pos_in_2x2.x) + int(pos_in_2x2.y) * 2;

            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // First pass: Scale2x on original pixels to get intermediate 2x2
            vec4 E = sampleTexture(src_tex_coord);
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));

            // Get the intermediate 2x2 pixel
            vec4 intermediate = scale2x_pixel(E, B, D, F, H, block_2x2);

            // Now we need to get the neighbors of this intermediate pixel for second pass
            // This is complex because we need to consider neighbors from adjacent original pixels
            vec4 int_B, int_D, int_F, int_H;

            // Top neighbor
            if (block_2x2 == 0 || block_2x2 == 1) {
                // Top edge of original pixel, need pixel from above
                vec4 BB = sampleTexture(src_tex_coord + vec2(0.0, -2.0 * texel.y));
                vec4 BA = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
                vec4 BC = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
                int_B = scale2x_pixel(B, BB, BA, BC, E, (block_2x2 == 0) ? 2 : 3);
            } else {
                int_B = scale2x_pixel(E, B, D, F, H, block_2x2 - 2);
            }

            // Left neighbor
            if (block_2x2 == 0 || block_2x2 == 2) {
                // Left edge of original pixel, need pixel from left
                vec4 DD = sampleTexture(src_tex_coord + vec2(-2.0 * texel.x, 0.0));
                vec4 DA = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
                vec4 DG = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
                int_D = scale2x_pixel(D, DA, DD, E, DG, (block_2x2 == 0) ? 1 : 3);
            } else {
                int_D = scale2x_pixel(E, B, D, F, H, block_2x2 - 1);
            }

            // Right neighbor
            if (block_2x2 == 1 || block_2x2 == 3) {
                // Right edge of original pixel, need pixel from right
                vec4 FF = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, 0.0));
                vec4 FC = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
                vec4 FI = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));
                int_F = scale2x_pixel(F, FC, E, FF, FI, (block_2x2 == 1) ? 0 : 2);
            } else {
                int_F = scale2x_pixel(E, B, D, F, H, block_2x2 + 1);
            }

            // Bottom neighbor
            if (block_2x2 == 2 || block_2x2 == 3) {
                // Bottom edge of original pixel, need pixel from below
                vec4 HH = sampleTexture(src_tex_coord + vec2(0.0, 2.0 * texel.y));
                vec4 HG = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
                vec4 HI = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));
                int_H = scale2x_pixel(H, E, HG, HI, HH, (block_2x2 == 2) ? 0 : 1);
            } else {
                int_H = scale2x_pixel(E, B, D, F, H, block_2x2 + 2);
            }

            // Second pass: Scale2x on intermediate pixels
            vec4 result = scale2x_pixel(intermediate, int_B, int_D, int_F, int_H, final_quad);

            // NO anti-aliasing for Scale4x (unlike AAScale4x)

            FragColor = result;
        }
    )";

            // AAScale4x shader - Anti-aliased Scale4x (Scale2x applied twice with AA)
            static constexpr const char* aascale4x_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        vec4 scale2x_pixel(vec4 E, vec4 B, vec4 D, vec4 F, vec4 H, int quadrant) {
            vec4 result = E;
            if (B != H && D != F) {
                if (quadrant == 0) result = (D == B) ? D : E;  // top-left
                else if (quadrant == 1) result = (B == F) ? F : E;  // top-right
                else if (quadrant == 2) result = (D == H) ? D : E;  // bottom-left
                else if (quadrant == 3) result = (H == F) ? F : E;  // bottom-right
            }
            return result;
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;

            // For 4x scaling, we need to determine which original pixel and which sub-pixel
            vec2 src_pixel = floor(output_pixel / 4.0);
            vec2 pos_in_4x4 = fract(output_pixel / 4.0) * 4.0;

            // First, determine which 2x2 block within the 4x4 we're in
            int block_2x2 = int(pos_in_4x4.x / 2.0) + int(pos_in_4x4.y / 2.0) * 2;

            // Position within that 2x2 block
            vec2 pos_in_2x2 = fract(pos_in_4x4 / 2.0) * 2.0;
            int final_quad = int(pos_in_2x2.x) + int(pos_in_2x2.y) * 2;

            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // First pass: Scale2x on original pixels to get intermediate 2x2
            vec4 E = sampleTexture(src_tex_coord);
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));

            // Get the intermediate 2x2 pixel
            vec4 intermediate = scale2x_pixel(E, B, D, F, H, block_2x2);

            // Now we need to get the neighbors of this intermediate pixel for second pass
            // This is complex because we need to consider neighbors from adjacent original pixels
            vec4 int_B, int_D, int_F, int_H;

            // Top neighbor
            if (block_2x2 == 0 || block_2x2 == 1) {
                // Top edge of original pixel, need pixel from above
                vec4 BB = sampleTexture(src_tex_coord + vec2(0.0, -2.0 * texel.y));
                vec4 BA = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
                vec4 BC = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
                int_B = scale2x_pixel(B, BB, BA, BC, E, (block_2x2 == 0) ? 2 : 3);
            } else {
                int_B = scale2x_pixel(E, B, D, F, H, block_2x2 - 2);
            }

            // Left neighbor
            if (block_2x2 == 0 || block_2x2 == 2) {
                // Left edge of original pixel, need pixel from left
                vec4 DD = sampleTexture(src_tex_coord + vec2(-2.0 * texel.x, 0.0));
                vec4 DA = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
                vec4 DG = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
                int_D = scale2x_pixel(D, DA, DD, E, DG, (block_2x2 == 0) ? 1 : 3);
            } else {
                int_D = scale2x_pixel(E, B, D, F, H, block_2x2 - 1);
            }

            // Right neighbor
            if (block_2x2 == 1 || block_2x2 == 3) {
                // Right edge of original pixel, need pixel from right
                vec4 FF = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, 0.0));
                vec4 FC = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
                vec4 FI = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));
                int_F = scale2x_pixel(F, FC, E, FF, FI, (block_2x2 == 1) ? 0 : 2);
            } else {
                int_F = scale2x_pixel(E, B, D, F, H, block_2x2 + 1);
            }

            // Bottom neighbor
            if (block_2x2 == 2 || block_2x2 == 3) {
                // Bottom edge of original pixel, need pixel from below
                vec4 HH = sampleTexture(src_tex_coord + vec2(0.0, 2.0 * texel.y));
                vec4 HG = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
                vec4 HI = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));
                int_H = scale2x_pixel(H, E, HG, HI, HH, (block_2x2 == 2) ? 0 : 1);
            } else {
                int_H = scale2x_pixel(E, B, D, F, H, block_2x2 + 2);
            }

            // Second pass: Scale2x on intermediate pixels
            vec4 result = scale2x_pixel(intermediate, int_B, int_D, int_F, int_H, final_quad);

            // Anti-aliasing: blend with intermediate pixel (50% mix)
            result = mix(result, intermediate, 0.5);

            FragColor = result;
        }
    )";

            // AAScale2x shader - Anti-aliased Scale2x
            static constexpr const char* aascale2x_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;
            vec2 src_pixel = floor(output_pixel / 2.0);
            vec2 block_pos = fract(output_pixel / 2.0);
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 3x3 neighborhood (only need cross pattern for Scale2x)
            vec4 E = sampleTexture(src_tex_coord);  // center
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));  // top
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));  // left
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));   // right
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));   // bottom

            // Calculate Scale2x pixels
            vec4 E0 = E;  // Top-left
            vec4 E1 = E;  // Top-right
            vec4 E2 = E;  // Bottom-left
            vec4 E3 = E;  // Bottom-right

            // Scale2x algorithm rules
            if (B != H && D != F) {
                E0 = (D == B) ? D : E;
                E1 = (B == F) ? F : E;
                E2 = (D == H) ? D : E;
                E3 = (H == F) ? F : E;
            }

            // Anti-aliasing: blend with original pixel (50% mix)
            E0 = mix(E0, E, 0.5);
            E1 = mix(E1, E, 0.5);
            E2 = mix(E2, E, 0.5);
            E3 = mix(E3, E, 0.5);

            // Select the appropriate quadrant
            vec4 result;
            if (block_pos.x < 0.5) {
                if (block_pos.y < 0.5) {
                    result = E0;  // top-left
                } else {
                    result = E2;  // bottom-left
                }
            } else {
                if (block_pos.y < 0.5) {
                    result = E1;  // top-right
                } else {
                    result = E3;  // bottom-right
                }
            }

            FragColor = result;
        }
    )";

            // 2xSaI shader
            static constexpr const char* twoxsai_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        int majorityMatch(vec4 A, vec4 B, vec4 C, vec4 D) {
            int x = 0, y = 0, r = 0;
            if (A == C) x += 1; else if (B == C) y += 1;
            if (A == D) x += 1; else if (B == D) y += 1;
            if (x <= 1) r -= 1;
            if (y <= 1) r += 1;
            return r;
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;
            vec2 src_pixel = floor(output_pixel / 2.0);
            vec2 block_pos = fract(output_pixel / 2.0);
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 4x4 neighborhood
            vec4 I = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
            vec4 E = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
            vec4 J = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, -texel.y));

            vec4 G = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 A = sampleTexture(src_tex_coord);  // center
            vec4 B = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));
            vec4 K = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, 0.0));

            vec4 H = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
            vec4 C = sampleTexture(src_tex_coord + vec2(0.0, texel.y));
            vec4 D = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));
            vec4 L = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, texel.y));

            vec4 M = sampleTexture(src_tex_coord + vec2(-texel.x, 2.0 * texel.y));
            vec4 N = sampleTexture(src_tex_coord + vec2(0.0, 2.0 * texel.y));
            vec4 O = sampleTexture(src_tex_coord + vec2(texel.x, 2.0 * texel.y));

            vec4 right_interp, bottom_interp, bottom_right_interp;

            if (A == D && B != C) {
                if ((A == E && B == L) || (A == C && A == F && B != E && B == J)) {
                    right_interp = A;
                } else {
                    right_interp = mix(A, B, 0.5);
                }

                if ((A == G && C == O) || (A == B && A == H && G != C && C == M)) {
                    bottom_interp = A;
                } else {
                    bottom_interp = A;
                }

                bottom_right_interp = A;
            } else if (A != D && B == C) {
                if ((B == F && A == H) || (B == E && B == D && A != F && A == I)) {
                    right_interp = B;
                } else {
                    right_interp = mix(A, B, 0.5);
                }

                if ((C == H && A == F) || (C == G && C == D && A != H && A == I)) {
                    bottom_interp = C;
                } else {
                    bottom_interp = mix(A, C, 0.5);
                }

                bottom_right_interp = B;
            } else if (A == D && B == C) {
                if (A == B) {
                    right_interp = bottom_interp = bottom_right_interp = A;
                } else {
                    right_interp = mix(A, B, 0.5);
                    bottom_interp = mix(A, C, 0.5);

                    int majority_accumulator = 0;
                    majority_accumulator += majorityMatch(B, A, G, E);
                    majority_accumulator += majorityMatch(B, A, K, F);
                    majority_accumulator += majorityMatch(B, A, H, N);
                    majority_accumulator += majorityMatch(B, A, L, O);

                    if (majority_accumulator > 0) {
                        bottom_right_interp = A;
                    } else if (majority_accumulator < 0) {
                        bottom_right_interp = B;
                    } else {
                        // Bilinear interpolation
                        bottom_right_interp = mix(mix(A, B, 0.5), mix(C, D, 0.5), 0.5);
                    }
                }
            } else {
                // Bilinear interpolation
                bottom_right_interp = mix(mix(A, B, 0.5), mix(C, D, 0.5), 0.5);

                if (A == C && A == F && B != E && B == J) {
                    right_interp = A;
                } else if (B == E && B == D && A != F && A == I) {
                    right_interp = B;
                } else {
                    right_interp = mix(A, B, 0.5);
                }

                if (A == B && A == H && G != C && C == M) {
                    bottom_interp = A;
                } else if (C == G && C == D && A != H && A == I) {
                    bottom_interp = C;
                } else {
                    bottom_interp = mix(A, C, 0.5);
                }
            }

            // Select the appropriate quadrant
            vec4 result;
            if (block_pos.x < 0.5) {
                if (block_pos.y < 0.5) {
                    result = A;  // top-left
                } else {
                    result = bottom_interp;  // bottom-left
                }
            } else {
                if (block_pos.y < 0.5) {
                    result = right_interp;  // top-right
                } else {
                    result = bottom_right_interp;  // bottom-right
                }
            }

            FragColor = result;
        }
    )";

            // Scale3x shader
            static constexpr const char* scale3x_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            // Clamp to texture bounds to handle edge cases
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;

            // Get the output pixel position
            vec2 output_pixel = v_texCoord * u_output_size;

            // Get source pixel coordinates (which input pixel are we upscaling?)
            vec2 src_pixel = floor(output_pixel / 3.0);

            // Get the position within the 3x3 output block (0-1 range)
            vec2 block_pos = fract(output_pixel / 3.0);

            // Calculate the texture coordinate for the center of the source pixel
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 3x3 neighborhood
            vec4 A = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 C = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));

            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 E = sampleTexture(src_tex_coord);  // center
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));

            vec4 G = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));
            vec4 I = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));

            // Scale3x algorithm rules
            vec4 E0 = E;
            vec4 E1 = E;
            vec4 E2 = E;
            vec4 E3 = E;
            vec4 E4 = E;
            vec4 E5 = E;
            vec4 E6 = E;
            vec4 E7 = E;
            vec4 E8 = E;

            if (B != H && D != F) {
                E0 = (D == B) ? D : E;
                E1 = ((D == B && E != C) || (B == F && E != A)) ? B : E;
                E2 = (B == F) ? F : E;
                E3 = ((D == B && E != G) || (D == H && E != A)) ? D : E;
                E4 = E;
                E5 = ((B == F && E != I) || (H == F && E != C)) ? F : E;
                E6 = (D == H) ? D : E;
                E7 = ((D == H && E != I) || (H == F && E != G)) ? H : E;
                E8 = (H == F) ? F : E;
            }

            // Select the appropriate pixel from the 3x3 output block
            vec4 result;

            // Determine which of the 9 output pixels we are
            int block_x = int(block_pos.x * 3.0);
            int block_y = int(block_pos.y * 3.0);

            // Clamp to 0-2 range to handle edge cases
            block_x = clamp(block_x, 0, 2);
            block_y = clamp(block_y, 0, 2);

            // Map to the corresponding E0-E8 value
            if (block_y == 0) {
                if (block_x == 0) result = E0;
                else if (block_x == 1) result = E1;
                else result = E2;
            } else if (block_y == 1) {
                if (block_x == 0) result = E3;
                else if (block_x == 1) result = E4;
                else result = E5;
            } else {
                if (block_x == 0) result = E6;
                else if (block_x == 1) result = E7;
                else result = E8;
            }

            FragColor = result;
        }
    )";

            // Eagle shader
            static constexpr const char* eagle_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            // Clamp to texture bounds to handle edge cases
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;

            // Get the output pixel position
            vec2 output_pixel = v_texCoord * u_output_size;

            // Get source pixel coordinates (which input pixel are we upscaling?)
            vec2 src_pixel = floor(output_pixel / 2.0);

            // Get the position within the 2x2 output block (0-1 range)
            vec2 block_pos = fract(output_pixel / 2.0);

            // Calculate the texture coordinate for the center of the source pixel
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 3x3 neighborhood
            vec4 top_left     = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
            vec4 top          = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 top_right    = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));

            vec4 left         = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 center       = sampleTexture(src_tex_coord);  // original_pixel
            vec4 right        = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));

            vec4 bottom_left  = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
            vec4 bottom       = sampleTexture(src_tex_coord + vec2(0.0, texel.y));
            vec4 bottom_right = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));

            // Initial assignments - all quadrants start with center pixel
            vec4 one = center;    // top-left
            vec4 two = center;    // top-right
            vec4 three = center;  // bottom-left
            vec4 four = center;   // bottom-right

            // Eagle interpolation rules (matching CPU implementation exactly)
            // if (top_left == top && top == top_right) { one = top_left; }
            if (top_left == top && top == top_right) {
                one = top_left;
            }

            // if (top == top_right && top_right == right) { two = top_right; }
            if (top == top_right && top_right == right) {
                two = top_right;
            }

            // if (left == bottom_left && bottom_left == bottom) { three = bottom_left; }
            if (left == bottom_left && bottom_left == bottom) {
                three = bottom_left;
            }

            // if (right == bottom_right && bottom_right == bottom) { four = bottom_right; }
            if (right == bottom_right && bottom_right == bottom) {
                four = bottom_right;
            }

            // Select the appropriate quadrant based on position within 2x2 block
            vec4 result;
            if (block_pos.x < 0.5) {
                if (block_pos.y < 0.5) {
                    result = one;    // top-left
                } else {
                    result = three;  // bottom-left
                }
            } else {
                if (block_pos.y < 0.5) {
                    result = two;    // top-right
                } else {
                    result = four;   // bottom-right
                }
            }

            FragColor = result;
        }
    )";

            // Scale2xSFX shader - Improved Scale2x algorithm by Sp00kyFox
            static constexpr const char* scale2x_sfx_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        vec4 sampleTexture(vec2 pos) {
            // Clamp to texture bounds to handle edge cases
            vec2 clamped = clamp(pos, vec2(0.0), vec2(1.0));
            return texture(u_texture, clamped);
        }

        void main() {
            vec2 texel = 1.0 / u_texture_size;

            // Get the output pixel position
            vec2 output_pixel = v_texCoord * u_output_size;

            // Get source pixel coordinates (which input pixel are we upscaling?)
            vec2 src_pixel = floor(output_pixel / 2.0);

            // Get the position within the 2x2 output block (0-1 range)
            vec2 block_pos = fract(output_pixel / 2.0);

            // Calculate the texture coordinate for the center of the source pixel
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 5x5 neighborhood
            vec4 J = sampleTexture(src_tex_coord + vec2(0.0, -2.0 * texel.y));  // y-2, x

            vec4 A = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));    // y-1, x-1
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));         // y-1, x
            vec4 C = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));     // y-1, x+1

            vec4 K = sampleTexture(src_tex_coord + vec2(-2.0 * texel.x, 0.0));   // y, x-2
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));         // y, x-1
            vec4 E = sampleTexture(src_tex_coord);                               // y, x (center)
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));          // y, x+1
            vec4 L = sampleTexture(src_tex_coord + vec2(2.0 * texel.x, 0.0));    // y, x+2

            vec4 G = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));     // y+1, x-1
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));          // y+1, x
            vec4 I = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));      // y+1, x+1

            vec4 M = sampleTexture(src_tex_coord + vec2(0.0, 2.0 * texel.y));    // y+2, x

            // Improved Scale2x SFX algorithm rules
            vec4 E0 = E;
            vec4 E1 = E;
            vec4 E2 = E;
            vec4 E3 = E;

            // E0 = B==D && B!=F && D!=H && (E!=A || E==C || E==G || A==J || A==K) ? D : E
            if (B == D && B != F && D != H && (E != A || E == C || E == G || A == J || A == K)) {
                E0 = D;
            }

            // E1 = B==F && B!=D && F!=H && (E!=C || E==A || E==I || C==J || C==L) ? F : E
            if (B == F && B != D && F != H && (E != C || E == A || E == I || C == J || C == L)) {
                E1 = F;
            }

            // E2 = D==H && B!=D && F!=H && (E!=G || E==A || E==I || G==K || G==M) ? D : E
            if (D == H && B != D && F != H && (E != G || E == A || E == I || G == K || G == M)) {
                E2 = D;
            }

            // E3 = F==H && B!=F && D!=H && (E!=I || E==C || E==G || I==L || I==M) ? F : E
            if (F == H && B != F && D != H && (E != I || E == C || E == G || I == L || I == M)) {
                E3 = F;
            }

            // Select the appropriate quadrant based on position within 2x2 block
            vec4 result;
            if (block_pos.x < 0.5) {
                if (block_pos.y < 0.5) {
                    result = E0;  // top-left
                } else {
                    result = E2;  // bottom-left
                }
            } else {
                if (block_pos.y < 0.5) {
                    result = E1;  // top-right
                } else {
                    result = E3;  // bottom-right
                }
            }

            FragColor = result;
        }
    )";

            // OmniScale shader - advanced pattern-based scaling from SameBoy
            static constexpr const char* omniscale_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;
        uniform vec2 u_output_size;

        // HQ colorspace conversion for pattern detection
        vec3 rgb_to_hq_colospace(vec4 rgb) {
            return vec3(0.250 * rgb.r + 0.250 * rgb.g + 0.250 * rgb.b,
                       0.250 * rgb.r - 0.000 * rgb.g - 0.250 * rgb.b,
                      -0.125 * rgb.r + 0.250 * rgb.g - 0.125 * rgb.b);
        }

        bool is_different(vec4 a, vec4 b) {
            vec3 diff = abs(rgb_to_hq_colospace(a) - rgb_to_hq_colospace(b));
            return diff.x > 0.018 || diff.y > 0.002 || diff.z > 0.005;
        }

        vec4 texture_relative(vec2 pos, vec2 offset) {
            return texture(u_texture, (floor(pos * u_texture_size) + offset + vec2(0.5, 0.5)) / u_texture_size);
        }

        #define P(m, r) ((pattern & (m)) == (r))

        void main() {
            vec2 position = v_texCoord;
            vec2 input_resolution = u_texture_size;
            vec2 output_resolution = u_output_size;

            // o = offset, the width of a pixel
            vec2 o = vec2(1, 1);

            // We always calculate the top left quarter. If we need a different quarter, we flip our co-ordinates
            // p = the position within a pixel [0...1]
            vec2 p = fract(position * input_resolution);

            if (p.x > 0.5) {
                o.x = -o.x;
                p.x = 1.0 - p.x;
            }
            if (p.y > 0.5) {
                o.y = -o.y;
                p.y = 1.0 - p.y;
            }

            vec4 w0 = texture_relative(position, vec2(-o.x, -o.y));
            vec4 w1 = texture_relative(position, vec2(   0, -o.y));
            vec4 w2 = texture_relative(position, vec2( o.x, -o.y));
            vec4 w3 = texture_relative(position, vec2(-o.x,    0));
            vec4 w4 = texture_relative(position, vec2(   0,    0));
            vec4 w5 = texture_relative(position, vec2( o.x,    0));
            vec4 w6 = texture_relative(position, vec2(-o.x,  o.y));
            vec4 w7 = texture_relative(position, vec2(   0,  o.y));
            vec4 w8 = texture_relative(position, vec2( o.x,  o.y));

            int pattern = 0;
            if (is_different(w0, w4)) pattern |= 1 << 0;
            if (is_different(w1, w4)) pattern |= 1 << 1;
            if (is_different(w2, w4)) pattern |= 1 << 2;
            if (is_different(w3, w4)) pattern |= 1 << 3;
            if (is_different(w5, w4)) pattern |= 1 << 4;
            if (is_different(w6, w4)) pattern |= 1 << 5;
            if (is_different(w7, w4)) pattern |= 1 << 6;
            if (is_different(w8, w4)) pattern |= 1 << 7;

            if ((P(0xBF,0x37) || P(0xDB,0x13)) && is_different(w1, w5)) {
                FragColor = mix(w4, w3, 0.5 - p.x);
                return;
            }
            if ((P(0xDB,0x49) || P(0xEF,0x6D)) && is_different(w7, w3)) {
                FragColor = mix(w4, w1, 0.5 - p.y);
                return;
            }
            if ((P(0x0B,0x0B) || P(0xFE,0x4A) || P(0xFE,0x1A)) && is_different(w3, w1)) {
                FragColor = w4;
                return;
            }
            if ((P(0x6F,0x2A) || P(0x5B,0x0A) || P(0xBF,0x3A) || P(0xDF,0x5A) ||
                 P(0x9F,0x8A) || P(0xCF,0x8A) || P(0xEF,0x4E) || P(0x3F,0x0E) ||
                 P(0xFB,0x5A) || P(0xBB,0x8A) || P(0x7F,0x5A) || P(0xAF,0x8A) ||
                 P(0xEB,0x8A)) && is_different(w3, w1)) {
                FragColor = mix(w4, mix(w4, w0, 0.5 - p.x), 0.5 - p.y);
                return;
            }
            if (P(0x0B,0x08)) {
                FragColor = mix(mix(w0 * 0.375 + w1 * 0.25 + w4 * 0.375, w4 * 0.5 + w1 * 0.5, p.x * 2.0), w4, p.y * 2.0);
                return;
            }
            if (P(0x0B,0x02)) {
                FragColor = mix(mix(w0 * 0.375 + w3 * 0.25 + w4 * 0.375, w4 * 0.5 + w3 * 0.5, p.y * 2.0), w4, p.x * 2.0);
                return;
            }
            if (P(0x2F,0x2F)) {
                float dist = length(p - vec2(0.5));
                float pixel_size = length(1.0 / (output_resolution / input_resolution));
                if (dist < 0.5 - pixel_size / 2.0) {
                    FragColor = w4;
                    return;
                }
                vec4 r;
                if (is_different(w0, w1) || is_different(w0, w3)) {
                    r = mix(w1, w3, p.y - p.x + 0.5);
                }
                else {
                    r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
                }

                if (dist > 0.5 + pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }
                FragColor = mix(w4, r, (dist - 0.5 + pixel_size / 2.0) / pixel_size);
                return;
            }
            if (P(0xBF,0x37) || P(0xDB,0x13)) {
                float dist = p.x - 2.0 * p.y;
                float pixel_size = length(1.0 / (output_resolution / input_resolution)) * sqrt(5.0);
                if (dist > pixel_size / 2.0) {
                    FragColor = w1;
                    return;
                }
                vec4 r = mix(w3, w4, p.x + 0.5);
                if (dist < -pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }
                FragColor = mix(r, w1, (dist + pixel_size / 2.0) / pixel_size);
                return;
            }
            if (P(0xDB,0x49) || P(0xEF,0x6D)) {
                float dist = p.y - 2.0 * p.x;
                float pixel_size = length(1.0 / (output_resolution / input_resolution)) * sqrt(5.0);
                if (p.y - 2.0 * p.x > pixel_size / 2.0) {
                    FragColor = w3;
                    return;
                }
                vec4 r = mix(w1, w4, p.x + 0.5);
                if (dist < -pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }
                FragColor = mix(r, w3, (dist + pixel_size / 2.0) / pixel_size);
                return;
            }
            if (P(0xBF,0x8F) || P(0x7E,0x0E)) {
                float dist = p.x + 2.0 * p.y;
                float pixel_size = length(1.0 / (output_resolution / input_resolution)) * sqrt(5.0);

                if (dist > 1.0 + pixel_size / 2.0) {
                    FragColor = w4;
                    return;
                }

                vec4 r;
                if (is_different(w0, w1) || is_different(w0, w3)) {
                    r = mix(w1, w3, p.y - p.x + 0.5);
                }
                else {
                    r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
                }

                if (dist < 1.0 - pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }

                FragColor = mix(r, w4, (dist + pixel_size / 2.0 - 1.0) / pixel_size);
                return;
            }

            if (P(0x7E,0x2A) || P(0xEF,0xAB)) {
                float dist = p.y + 2.0 * p.x;
                float pixel_size = length(1.0 / (output_resolution / input_resolution)) * sqrt(5.0);

                if (p.y + 2.0 * p.x > 1.0 + pixel_size / 2.0) {
                    FragColor = w4;
                    return;
                }

                vec4 r;

                if (is_different(w0, w1) || is_different(w0, w3)) {
                    r = mix(w1, w3, p.y - p.x + 0.5);
                }
                else {
                    r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
                }

                if (dist < 1.0 - pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }

                FragColor = mix(r, w4, (dist + pixel_size / 2.0 - 1.0) / pixel_size);
                return;
            }

            if (P(0x1B,0x03) || P(0x4F,0x43) || P(0x8B,0x83) || P(0x6B,0x43)) {
                FragColor = mix(w4, w3, 0.5 - p.x);
                return;
            }

            if (P(0x4B,0x09) || P(0x8B,0x89) || P(0x1F,0x19) || P(0x3B,0x19)) {
                FragColor = mix(w4, w1, 0.5 - p.y);
                return;
            }

            if (P(0xFB,0x6A) || P(0x6F,0x6E) || P(0x3F,0x3E) || P(0xFB,0xFA) ||
                P(0xDF,0xDE) || P(0xDF,0x1E)) {
                FragColor = mix(w4, w0, (1.0 - p.x - p.y) / 2.0);
                return;
            }

            if (P(0x4F,0x4B) || P(0x9F,0x1B) || P(0x2F,0x0B) ||
                P(0xBE,0x0A) || P(0xEE,0x0A) || P(0x7E,0x0A) || P(0xEB,0x4B) ||
                P(0x3B,0x1B)) {
                float dist = p.x + p.y;
                float pixel_size = length(1.0 / (output_resolution / input_resolution));

                if (dist > 0.5 + pixel_size / 2.0) {
                    FragColor = w4;
                    return;
                }

                vec4 r;
                if (is_different(w0, w1) || is_different(w0, w3)) {
                    r = mix(w1, w3, p.y - p.x + 0.5);
                }
                else {
                    r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
                }

                if (dist < 0.5 - pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }

                FragColor = mix(r, w4, (dist + pixel_size / 2.0 - 0.5) / pixel_size);
                return;
            }

            if (P(0x0B,0x01)) {
                FragColor = mix(mix(w4, w3, 0.5 - p.x), mix(w1, (w1 + w3) / 2.0, 0.5 - p.x), 0.5 - p.y);
                return;
            }

            if (P(0x0B,0x00)) {
                FragColor = mix(mix(w4, w3, 0.5 - p.x), mix(w1, w0, 0.5 - p.x), 0.5 - p.y);
                return;
            }

            float dist = p.x + p.y;
            float pixel_size = length(1.0 / (output_resolution / input_resolution));

            if (dist > 0.5 + pixel_size / 2.0) {
                FragColor = w4;
                return;
            }

            // We need more samples to "solve" this diagonal
            vec4 x0 = texture_relative(position, vec2(-o.x * 2.0, -o.y * 2.0));
            vec4 x1 = texture_relative(position, vec2(-o.x      , -o.y * 2.0));
            vec4 x2 = texture_relative(position, vec2( 0.0      , -o.y * 2.0));
            vec4 x3 = texture_relative(position, vec2( o.x      , -o.y * 2.0));
            vec4 x4 = texture_relative(position, vec2(-o.x * 2.0, -o.y      ));
            vec4 x5 = texture_relative(position, vec2(-o.x * 2.0,  0.0      ));
            vec4 x6 = texture_relative(position, vec2(-o.x * 2.0,  o.y      ));

            if (is_different(x0, w4)) pattern |= 1 << 8;
            if (is_different(x1, w4)) pattern |= 1 << 9;
            if (is_different(x2, w4)) pattern |= 1 << 10;
            if (is_different(x3, w4)) pattern |= 1 << 11;
            if (is_different(x4, w4)) pattern |= 1 << 12;
            if (is_different(x5, w4)) pattern |= 1 << 13;
            if (is_different(x6, w4)) pattern |= 1 << 14;

            int diagonal_bias = -7;
            while (pattern != 0) {
                diagonal_bias += pattern & 1;
                pattern >>= 1;
            }

            if (diagonal_bias <= 0) {
                vec4 r = mix(w1, w3, p.y - p.x + 0.5);
                if (dist < 0.5 - pixel_size / 2.0) {
                    FragColor = r;
                    return;
                }
                FragColor = mix(r, w4, (dist + pixel_size / 2.0 - 0.5) / pixel_size);
                return;
            }

            FragColor = w4;
        }
    )";

        public:
            SDLOpenGLMultiScaler() = default;

            ~SDLOpenGLMultiScaler() {
                cleanup();
            }

            bool init(const std::string& title = "SDL OpenGL Scaler",
                      int width = 800, int height = 600) {
                if (initialized_) {
                    return true;
                }

                window_width_ = width;
                window_height_ = height;

                // Initialize SDL with proper cleanup on failure
                bool sdl_initialized = false;
                if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                    throw std::runtime_error("SDL initialization failed: " + std::string(SDL_GetError()));
                }
                sdl_initialized = true;

                try {
                    // Set OpenGL attributes
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

                    // Create window
                    window_ = SDL_CreateWindow(
                        title.c_str(),
                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                        window_width_, window_height_,
                        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
                    );

                    if (!window_) {
                        throw std::runtime_error("Window creation failed: " + std::string(SDL_GetError()));
                    }
                    owns_window_ = true;  // We created the window, so we own it

                    // Create OpenGL context
                    gl_context_ = SDL_GL_CreateContext(window_);
                    if (!gl_context_) {
                        throw std::runtime_error("OpenGL context creation failed: " + std::string(SDL_GetError()));
                    }
                    owns_context_ = true;  // We created the context, so we own it

                    // Initialize OpenGL
                    initGL();

                    // Precompile commonly used shaders for faster first-time use
                    precompileShader(DEFAULT);
                    precompileShader(EPX);

                    // Use default shader initially
                    useShaderForAlgorithm(DEFAULT);

                    initialized_ = true;
                    return true;
                } catch (...) {
                    // Clean up on any failure - reset ownership flags first
                    owns_window_ = false;
                    owns_context_ = false;

                    if (gl_context_) {
                        SDL_GL_DeleteContext(gl_context_);
                        gl_context_ = nullptr;
                    }
                    if (window_) {
                        SDL_DestroyWindow(window_);
                        window_ = nullptr;
                    }
                    if (sdl_initialized) {
                        SDL_Quit();
                    }
                    throw;
                }
            }

            // Initialize with existing window and GL context
            bool initialize(SDL_Window* existing_window) {
                if (initialized_) {
                    return true;
                }

                if (!existing_window) {
                    return false;
                }

                window_ = existing_window;
                owns_window_ = false;  // We don't own the external window
                gl_context_ = SDL_GL_GetCurrentContext();
                owns_context_ = false;  // We don't own the external context

                if (!gl_context_) {
                    return false;
                }

                SDL_GetWindowSize(window_, &window_width_, &window_height_);

                // Initialize OpenGL
                initGL();

                // Precompile commonly used shaders for faster first-time use
                precompileShader(DEFAULT);
                precompileShader(EPX);

                // Use default shader initially
                useShaderForAlgorithm(DEFAULT);

                initialized_ = true;
                return true;
            }

            void cleanup() {
                if (!initialized_) {
                    return;
                }

                initialized_ = false; // Set early to prevent concurrent cleanup

                // Clean up shaders with thread safety
                {
                    std::lock_guard <std::mutex> lock(shader_mutex_);

                    // Clean up cached shaders
                    for (auto& [algo, shader] : shader_cache_) {
                        if (shader.program) {
                            glDeleteProgram(shader.program);
                            shader.program = 0; // Prevent double-delete
                        }
                    }
                    shader_cache_.clear();

                    // Note: Legacy shaders_ map removed - all shaders now cached by Algorithm

                    current_shader_ = nullptr;
                    current_algorithm_ = DEFAULT;
                }

                // Clean up OpenGL resources with safety checks
                if (vbo_) {
                    glDeleteBuffers(1, &vbo_);
                    vbo_ = 0;
                }
                if (vao_) {
                    glDeleteVertexArrays(1, &vao_);
                    vao_ = 0;
                }
                if (texture_) {
                    glDeleteTextures(1, &texture_);
                    texture_ = 0;
                }
                if (framebuffer_) {
                    glDeleteFramebuffers(1, &framebuffer_);
                    framebuffer_ = 0;
                }

                // Clean up SDL - only if we own the resources
                if (gl_context_ && owns_context_) {
                    SDL_GL_DeleteContext(gl_context_);
                }
                gl_context_ = nullptr;

                if (window_ && owns_window_) {
                    SDL_DestroyWindow(window_);
                }
                window_ = nullptr;

                // Reset ownership flags
                owns_window_ = false;
                owns_context_ = false;

                // Note: SDL_Quit() removed - should be called by application, not library
            }

            // Note: loadShader method removed - use precompileShader or automatic caching instead

            // Get or compile shader for the given algorithm
            ShaderProgram* getOrCompileShader(Algorithm algo) {
                std::lock_guard<std::mutex> lock(shader_mutex_);

                // Check if shader is already cached
                auto it = shader_cache_.find(algo);
                if (it != shader_cache_.end()) {
                    return &it->second;
                }

                // Compile and cache the shader
                const char* fragment_shader = getFragmentShaderForAlgorithm(algo);
                ShaderProgram program = compileShaderProgram(vertex_shader_source, fragment_shader);

                // Cache the compiled shader
                auto [inserted_it, success] = shader_cache_.emplace(algo, program);
                return &inserted_it->second;
            }

            // Select and use shader for algorithm
            void useShaderForAlgorithm(Algorithm algo) {
                ShaderProgram* shader = getOrCompileShader(algo);
                if (shader && shader->program != 0) {
                    current_shader_ = shader;
                    current_algorithm_ = algo;
                    glUseProgram(shader->program);
                }
            }

            // Legacy method for backward compatibility
            void useShader(const std::string& name) {
                Algorithm algo = DEFAULT;
                if (name == "epx") algo = EPX;
                else if (name == "scale2x") algo = Scale2x;
                else if (name == "scale2x_sfx") algo = Scale2xSFX;
                else if (name == "scale3x") algo = Scale3x;
                else if (name == "eagle") algo = Eagle;
                else if (name == "twoxsai") algo = TwoXSaI;
                else if (name == "aascale2x") algo = AAScale2x;
                else if (name == "aascale4x") algo = AAScale4x;
                else if (name == "scale4x") algo = Scale4x;
                else if (name == "omniscale") algo = OmniScale;
                else if (name == "default") algo = DEFAULT;

                useShaderForAlgorithm(algo);
            }

            template<typename InputImage>
            void scale(const InputImage& input, float scale_factor = 2.0f) {
                if (!initialized_) {
                    throw std::runtime_error("Scaler not initialized");
                }

                size_t src_width = input.width();
                size_t src_height = input.height();
                size_t dst_width = static_cast <size_t>(src_width * scale_factor);
                size_t dst_height = static_cast <size_t>(src_height * scale_factor);

                // Upload input image to texture
                uploadTexture(input);

                // Set uniforms
                if (current_shader_) {
                    if (current_shader_->u_texture >= 0) {
                        glUniform1i(current_shader_->u_texture, 0);
                    }
                    if (current_shader_->u_texture_size >= 0) {
                        glUniform2f(current_shader_->u_texture_size,
                                    static_cast <float>(src_width),
                                    static_cast <float>(src_height));
                    }
                    if (current_shader_->u_output_size >= 0) {
                        glUniform2f(current_shader_->u_output_size,
                                    static_cast <float>(dst_width),
                                    static_cast <float>(dst_height));
                    }
                    if (current_shader_->u_scale >= 0) {
                        glUniform1f(current_shader_->u_scale, scale_factor);
                    }
                }

                // Render
                glViewport(0, 0, window_width_, window_height_);
                glClear(GL_COLOR_BUFFER_BIT);

                glBindVertexArray(vao_);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);

                SDL_GL_SwapWindow(window_);
            }

            bool handleEvents() {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        return false;
                    }
                    if (event.type == SDL_KEYDOWN) {
                        switch (event.key.keysym.sym) {
                            case SDLK_ESCAPE:
                                return false;
                            case SDLK_1:
                                useShader("default");
                                break;
                            case SDLK_2:
                                useShader("epx");
                                break;
                            case SDLK_3:
                                useShader("omniscale");
                                break;
                        }
                    }
                }
                return true;
            }

            // Cache management methods
            void clearShaderCache() {
                std::lock_guard<std::mutex> lock(shader_mutex_);
                for (auto& [algo, shader] : shader_cache_) {
                    if (shader.program) {
                        glDeleteProgram(shader.program);
                    }
                }
                shader_cache_.clear();
                current_shader_ = nullptr;
                current_algorithm_ = DEFAULT;
            }

            // Precompile shader for an algorithm
            void precompileShader(Algorithm algo) {
                getOrCompileShader(algo);
            }

            // Precompile all shaders for faster runtime switching
            void precompileAllShaders() {
                const Algorithm algorithms[] = {
                    EPX, Scale2x, Scale2xSFX, Scale3x, Eagle,
                    TwoXSaI, AAScale2x, AAScale4x, Scale4x, OmniScale,
                    AdvMAME2x, AdvMAME3x, DEFAULT
                };

                for (Algorithm algo : algorithms) {
                    try {
                        precompileShader(algo);
                    } catch (const std::exception& e) {
                        // Log error but continue with other shaders
                        std::cerr << "Warning: Failed to precompile shader for algorithm "
                                  << static_cast<int>(algo) << ": " << e.what() << std::endl;
                    }
                }
            }

            // Get cache statistics
            size_t getCachedShaderCount() const {
                std::lock_guard<std::mutex> lock(shader_mutex_);
                return shader_cache_.size();
            }

            // Check if a shader is cached
            bool isShaderCached(Algorithm algo) const {
                std::lock_guard<std::mutex> lock(shader_mutex_);
                return shader_cache_.find(algo) != shader_cache_.end();
            }

            // Scale SDL_Surface and return result as new SDL_Surface
            SDL_Surface* scaleSurface(SDL_Surface* input, float scale_factor, Algorithm method = DEFAULT) {
                if (!initialized_ || !input) {
                    return nullptr;
                }

                // Use cached shader for the algorithm
                useShaderForAlgorithm(method);

                // Use appropriate types to avoid conversions
                size_t src_width = static_cast <size_t>(input->w > 0 ? input->w : 0);
                size_t src_height = static_cast <size_t>(input->h > 0 ? input->h : 0);
                size_t dst_width = static_cast <size_t>(static_cast <float>(src_width) * scale_factor);
                size_t dst_height = static_cast <size_t>(static_cast <float>(src_height) * scale_factor);

                // Create framebuffer for offscreen rendering with RAII
                GLuint fbo = 0, render_texture = 0;
                glGenFramebuffers(1, &fbo);
                checkGLError("glGenFramebuffers");

                // RAII for framebuffer
                auto fbo_deleter = [](GLuint id) { if (id) glDeleteFramebuffers(1, &id); };
                GLResource <decltype(fbo_deleter)> fbo_guard(fbo, fbo_deleter);

                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                checkGLError("glBindFramebuffer");

                // Create texture for rendering
                glGenTextures(1, &render_texture);
                checkGLError("glGenTextures");

                // RAII for texture
                auto tex_deleter = [](GLuint id) { if (id) glDeleteTextures(1, &id); };
                GLResource <decltype(tex_deleter)> tex_guard(render_texture, tex_deleter);

                glBindTexture(GL_TEXTURE_2D, render_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                             static_cast <GLsizei>(dst_width),
                             static_cast <GLsizei>(dst_height),
                             0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                checkGLError("glTexImage2D");

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture, 0);
                checkGLError("glFramebufferTexture2D");

                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    return nullptr;
                }

                SDL_Surface* result = nullptr;
                try {
                    // Upload input surface to texture
                    uploadSDLSurface(input);

                    // Set uniforms
                    if (current_shader_) {
                        if (current_shader_->u_texture >= 0) {
                            glUniform1i(current_shader_->u_texture, 0);
                        }
                        if (current_shader_->u_texture_size >= 0) {
                            glUniform2f(current_shader_->u_texture_size,
                                        static_cast <float>(src_width),
                                        static_cast <float>(src_height));
                        }
                        if (current_shader_->u_output_size >= 0) {
                            glUniform2f(current_shader_->u_output_size,
                                        static_cast <float>(dst_width),
                                        static_cast <float>(dst_height));
                        }
                        if (current_shader_->u_scale >= 0) {
                            glUniform1f(current_shader_->u_scale, scale_factor);
                        }
                    }

                    // Render to framebuffer
                    glViewport(0, 0, static_cast <GLsizei>(dst_width), static_cast <GLsizei>(dst_height));
                    glClear(GL_COLOR_BUFFER_BIT);

                    glBindVertexArray(vao_);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    glBindVertexArray(0);
                    checkGLError("glDrawArrays");

                    // Read pixels from framebuffer
                    glPixelStorei(GL_PACK_ALIGNMENT, 4);

                    // Reuse pixel buffer for better performance
                    size_t required_size = dst_width * dst_height * 4;
                    if (pixel_buffer_.size() < required_size) {
                        pixel_buffer_.resize(required_size);
                    }

                    glReadPixels(0, 0, static_cast <GLsizei>(dst_width), static_cast <GLsizei>(dst_height),
                                 GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer_.data());
                    checkGLError("glReadPixels");

                    // Create SDL surface from pixels
                    result = SDL_CreateRGBSurfaceWithFormat(0,
                                                            static_cast <int>(dst_width),
                                                            static_cast <int>(dst_height),
                                                            32, SDL_PIXELFORMAT_RGBA32);
                    if (result && result->pixels) {
                        // Validate SDL surface properties
                        if (result->pitch < 0 || static_cast <size_t>(result->pitch) < dst_width * 4) {
                            SDL_FreeSurface(result);
                            throw std::runtime_error("Invalid SDL surface pitch");
                        }

                        // Flip vertically (OpenGL renders upside down)
                        unsigned char* dst_pixels = static_cast <unsigned char*>(result->pixels);
                        const size_t row_size = dst_width * 4;

                        for (size_t y = 0; y < dst_height; ++y) {
                            size_t src_row = (dst_height - 1 - y) * row_size;
                            size_t dst_offset = y * static_cast<size_t>(result->pitch); // Use actual pitch, not calculated

                            // Comprehensive bounds check
                            if (src_row + row_size <= pixel_buffer_.size() &&
                                dst_offset + row_size <= static_cast <size_t>(result->h * result->pitch)) {
                                std::memcpy(dst_pixels + dst_offset, pixel_buffer_.data() + src_row, row_size);
                            } else {
                                // Handle error gracefully
                                SDL_FreeSurface(result);
                                throw std::runtime_error("Buffer overflow prevented in pixel copy");
                            }
                        }
                    }
                } catch (...) {
                    // Clean up SDL surface if created
                    if (result) {
                        SDL_FreeSurface(result);
                        result = nullptr;
                    }
                    throw;
                }

                // Cleanup is automatic via RAII
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                return result;
            }

        private:
            // Get fragment shader source for algorithm
            const char* getFragmentShaderForAlgorithm(Algorithm algo) const {
                switch (algo) {
                    case EPX:
                    case AdvMAME2x:
                        return epx_fragment_shader;
                    case Scale2x:
                        return scale2x_fragment_shader;
                    case Scale2xSFX:
                        return scale2x_sfx_fragment_shader;
                    case Scale3x:
                    case AdvMAME3x:
                        return scale3x_fragment_shader;
                    case Eagle:
                        return eagle_fragment_shader;
                    case TwoXSaI:
                        return twoxsai_fragment_shader;
                    case AAScale2x:
                        return aascale2x_fragment_shader;
                    case AAScale4x:
                        return aascale4x_fragment_shader;
                    case Scale4x:
                        return scale4x_fragment_shader;
                    case OmniScale:
                        return omniscale_fragment_shader;
                    case DEFAULT:
                    default:
                        return default_fragment_shader;
                }
            }

            // Compile shader program and return ShaderProgram struct
            ShaderProgram compileShaderProgram(const char* vertex_src, const char* fragment_src) {
                ShaderProgram program;

                GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_src);
                GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_src);

                program.program = glCreateProgram();
                glAttachShader(program.program, vertex_shader);
                glAttachShader(program.program, fragment_shader);
                glLinkProgram(program.program);

                // Check for linking errors
                GLint success;
                glGetProgramiv(program.program, GL_LINK_STATUS, &success);
                if (!success) {
                    GLint logLength;
                    glGetProgramiv(program.program, GL_INFO_LOG_LENGTH, &logLength);
                    std::vector<char> infoLog(static_cast<size_t>(logLength > 0 ? logLength : 512));
                    glGetProgramInfoLog(program.program, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
                    glDeleteProgram(program.program);
                    glDeleteShader(vertex_shader);
                    glDeleteShader(fragment_shader);
                    throw std::runtime_error(std::string("Shader program linking failed: ") + infoLog.data());
                }

                // Clean up shaders (they're linked to the program now)
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);

                // Get uniform locations
                program.u_texture = glGetUniformLocation(program.program, "u_texture");
                program.u_texture_size = glGetUniformLocation(program.program, "u_texture_size");
                program.u_output_size = glGetUniformLocation(program.program, "u_output_size");
                program.u_scale = glGetUniformLocation(program.program, "u_scale");

                return program;
            }

            void initGL() {
                // Setup vertex data
                float vertices[] = {
                    // positions   // texture coords
                    -1.0f, 1.0f, 0.0f, 0.0f,
                    -1.0f, -1.0f, 0.0f, 1.0f,
                    1.0f, 1.0f, 1.0f, 0.0f,
                    1.0f, -1.0f, 1.0f, 1.0f,
                };

                glGenVertexArrays(1, &vao_);
                checkGLError("glGenVertexArrays");
                if (vao_ == 0) {
                    throw std::runtime_error("Failed to generate vertex array");
                }

                glGenBuffers(1, &vbo_);
                checkGLError("glGenBuffers");
                if (vbo_ == 0) {
                    glDeleteVertexArrays(1, &vao_);
                    throw std::runtime_error("Failed to generate vertex buffer");
                }

                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                checkGLError("glBufferData");

                // Position attribute
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast <void*>(nullptr));
                glEnableVertexAttribArray(0);
                checkGLError("glVertexAttribPointer position");

                // TexCoord attribute
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                      reinterpret_cast <void*>(2 * sizeof(float)));
                glEnableVertexAttribArray(1);
                checkGLError("glVertexAttribPointer texcoord");

                glBindVertexArray(0);

                // Create texture
                glGenTextures(1, &texture_);
                checkGLError("glGenTextures");
                if (texture_ == 0) {
                    throw std::runtime_error("Failed to generate texture");
                }

                glBindTexture(GL_TEXTURE_2D, texture_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                checkGLError("glTexParameteri");
            }

            GLuint compileShader(GLenum type, const char* source) {
                GLuint shader = glCreateShader(type);
                if (shader == 0) {
                    throw std::runtime_error("Failed to create shader");
                }
                checkGLError("glCreateShader");

                glShaderSource(shader, 1, &source, nullptr);
                glCompileShader(shader);
                checkGLError("glCompileShader");

                GLint success;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    // Get the actual log length for proper buffer allocation
                    GLint logLength = 0;
                    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

                    std::vector <char> infoLog(static_cast<size_t>(logLength > 0 ? logLength : 512));
                    glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());

                    glDeleteShader(shader); // Clean up the failed shader

                    std::string shaderType = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
                    throw std::runtime_error(
                        shaderType + " shader compilation failed: " + std::string(infoLog.data()));
                }

                return shader;
            }

            GLuint createShaderProgram(const char* vertex_src, const char* fragment_src) {
                GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_src);
                GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_src);

                GLuint program = glCreateProgram();
                glAttachShader(program, vertex_shader);
                glAttachShader(program, fragment_shader);
                glLinkProgram(program);

                GLint success;
                glGetProgramiv(program, GL_LINK_STATUS, &success);
                if (!success) {
                    char infoLog[512];
                    glGetProgramInfoLog(program, 512, nullptr, infoLog);
                    throw std::runtime_error("Shader linking failed: " + std::string(infoLog));
                }

                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);

                return program;
            }

            template<typename InputImage>
            void uploadTexture(const InputImage& input) {
                size_t width = input.width();
                size_t height = input.height();

                // Convert image to RGBA format
                std::vector <unsigned char> pixels(width * height * 4);
                for (size_t y = 0; y < height; ++y) {
                    for (size_t x = 0; x < width; ++x) {
                        auto pixel = input.get_pixel(x, y);
                        size_t idx = (y * width + x) * 4;
                        pixels[idx] = pixel.x; // R
                        pixels[idx + 1] = pixel.y; // G
                        pixels[idx + 2] = pixel.z; // B
                        pixels[idx + 3] = 255; // A
                    }
                }

                glBindTexture(GL_TEXTURE_2D, texture_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            }

            void uploadSDLSurface(SDL_Surface* surface) {
                if (!surface) return;

                SDL_Surface* converted = nullptr;
                SDL_Surface* src = surface;

                // Convert to RGBA format if needed
                if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
                    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
                    if (converted) {
                        src = converted;
                    }
                }

                glBindTexture(GL_TEXTURE_2D, texture_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->w, src->h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, src->pixels);

                if (converted) {
                    SDL_FreeSurface(converted);
                }
            }
    };
} // namespace scaler::gpu
