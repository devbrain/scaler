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
#include <scaler/gpu/shader_source.hh>

namespace scaler::gpu {
    namespace detail {
        // RAII wrapper for OpenGL resources
        template<typename Deleter>
        class gl_resource {
            private:
                GLuint id_;
                Deleter deleter_;
                bool owned_;

            public:
                gl_resource()
                    : id_(0), owned_(false) {
                }

                gl_resource(GLuint id, Deleter del)
                    : id_(id), deleter_(del), owned_(true) {
                }

                ~gl_resource() { release(); }

                // Move only
                gl_resource(gl_resource&& other) noexcept
                    : id_(other.id_), deleter_(std::move(other.deleter_)), owned_(other.owned_) {
                    other.owned_ = false;
                }

                gl_resource& operator=(gl_resource&& other) noexcept {
                    if (this != &other) {
                        release();
                        id_ = other.id_;
                        deleter_ = std::move(other.deleter_);
                        owned_ = other.owned_;
                        other.owned_ = false;
                    }
                    return *this;
                }

                gl_resource(const gl_resource&) = delete;
                gl_resource& operator=(const gl_resource&) = delete;

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
        inline void check_gl_error(const char* operation) {
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                throw std::runtime_error(std::string("OpenGL error in ") + operation + ": " + std::to_string(error));
            }
        }
    }

    class sdl_opengl_multi_scaler {
        public:
            // algorithm type alias for compatibility
            enum algorithm {
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
            using ScalingMethod = algorithm;

        private:
            SDL_Window* window_ = nullptr;
            SDL_GLContext gl_context_ = nullptr;
            bool owns_window_ = false; // Track whether we created and own the window
            bool owns_context_ = false; // Track whether we created and own the GL context
            GLuint framebuffer_ = 0;
            GLuint texture_ = 0;

            struct shader_program {
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
            std::unordered_map <algorithm, shader_program> shader_cache_;
            algorithm current_algorithm_ = DEFAULT;
            shader_program* current_shader_ = nullptr;

            GLuint vao_ = 0;
            GLuint vbo_ = 0;

            int window_width_ = 800;
            int window_height_ = 600;
            bool initialized_ = false;

            // Performance optimization: reusable pixel buffer
            mutable std::vector <unsigned char> pixel_buffer_;

        public:
            sdl_opengl_multi_scaler() = default;

            ~sdl_opengl_multi_scaler() {
                cleanup();
            }

            void init(const std::string& title = "SDL OpenGL Scaler",
                      int width = 800, int height = 600) {
                if (initialized_) {
                    return;
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
                    owns_window_ = true; // We created the window, so we own it

                    // Create OpenGL context
                    gl_context_ = SDL_GL_CreateContext(window_);
                    if (!gl_context_) {
                        throw std::runtime_error("OpenGL context creation failed: " + std::string(SDL_GetError()));
                    }
                    owns_context_ = true; // We created the context, so we own it

                    // Initialize OpenGL
                    init_gl();

                    // Precompile commonly used shaders for faster first-time use
                    precompile_shader(DEFAULT);
                    precompile_shader(EPX);

                    // Use default shader initially
                    use_shader_for_algorithm(DEFAULT);

                    initialized_ = true;
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
                owns_window_ = false; // We don't own the external window
                gl_context_ = SDL_GL_GetCurrentContext();
                owns_context_ = false; // We don't own the external context

                if (!gl_context_) {
                    return false;
                }

                SDL_GetWindowSize(window_, &window_width_, &window_height_);

                // Initialize OpenGL
                init_gl();

                // Precompile commonly used shaders for faster first-time use
                precompile_shader(DEFAULT);
                precompile_shader(EPX);

                // Use default shader initially
                use_shader_for_algorithm(DEFAULT);

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

                    // Note: Legacy shaders_ map removed - all shaders now cached by algorithm

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

            }

            // Get or compile shader for the given algorithm
            shader_program* get_or_compile_shader(algorithm algo) {
                std::lock_guard <std::mutex> lock(shader_mutex_);

                // Check if shader is already cached
                auto it = shader_cache_.find(algo);
                if (it != shader_cache_.end()) {
                    return &it->second;
                }

                // Compile and cache the shader
                const char* fragment_shader = get_fragment_shader_for_algorithm(algo);
                shader_program program = compile_shader_program(shader_source::vertex_shader_source, fragment_shader);

                // Cache the compiled shader
                auto [inserted_it, success] = shader_cache_.emplace(algo, program);
                return &inserted_it->second;
            }

            // Select and use shader for algorithm
            void use_shader_for_algorithm(algorithm algo) {
                shader_program* shader = get_or_compile_shader(algo);
                if (shader && shader->program != 0) {
                    current_shader_ = shader;
                    current_algorithm_ = algo;
                    glUseProgram(shader->program);
                }
            }

            // Legacy method for backward compatibility
            void use_shader(const std::string& name) {
                algorithm algo = DEFAULT;
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

                use_shader_for_algorithm(algo);
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
                upload_texture(input);

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
                                use_shader("default");
                                break;
                            case SDLK_2:
                                use_shader("epx");
                                break;
                            case SDLK_3:
                                use_shader("omniscale");
                                break;
                        }
                    }
                }
                return true;
            }

            // Cache management methods
            void clear_shader_cache() {
                std::lock_guard <std::mutex> lock(shader_mutex_);
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
            void precompile_shader(algorithm algo) {
                get_or_compile_shader(algo);
            }

            // Precompile all shaders for faster runtime switching
            void precompile_all_shaders() {
                const algorithm algorithms[] = {
                    EPX, Scale2x, Scale2xSFX, Scale3x, Eagle,
                    TwoXSaI, AAScale2x, AAScale4x, Scale4x, OmniScale,
                    AdvMAME2x, AdvMAME3x, DEFAULT
                };

                for (algorithm algo : algorithms) {
                    try {
                        precompile_shader(algo);
                    } catch (const std::exception& e) {
                        // Log error but continue with other shaders
                        std::cerr << "Warning: Failed to precompile shader for algorithm "
                            << static_cast <int>(algo) << ": " << e.what() << std::endl;
                    }
                }
            }

            // Get cache statistics
            size_t get_cached_shader_count() const {
                std::lock_guard <std::mutex> lock(shader_mutex_);
                return shader_cache_.size();
            }

            // Check if a shader is cached
            bool is_shader_cached(algorithm algo) const {
                std::lock_guard <std::mutex> lock(shader_mutex_);
                return shader_cache_.find(algo) != shader_cache_.end();
            }

            // Scale SDL_Surface and return result as new SDL_Surface
            SDL_Surface* scale_surface(SDL_Surface* input, float scale_factor, algorithm method = DEFAULT) {
                if (!initialized_ || !input) {
                    return nullptr;
                }

                // Use cached shader for the algorithm
                use_shader_for_algorithm(method);

                // Use appropriate types to avoid conversions
                auto src_width = static_cast <size_t>(input->w > 0 ? input->w : 0);
                auto src_height = static_cast <size_t>(input->h > 0 ? input->h : 0);
                auto dst_width = static_cast <size_t>(static_cast <float>(src_width) * scale_factor);
                auto dst_height = static_cast <size_t>(static_cast <float>(src_height) * scale_factor);

                // Create framebuffer for offscreen rendering with RAII
                GLuint fbo = 0, render_texture = 0;
                glGenFramebuffers(1, &fbo);
                detail::check_gl_error("glGenFramebuffers");

                // RAII for framebuffer
                auto fbo_deleter = [](GLuint id) { if (id) glDeleteFramebuffers(1, &id); };
                detail::gl_resource <decltype(fbo_deleter)> fbo_guard(fbo, fbo_deleter);

                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                detail::check_gl_error("glBindFramebuffer");

                // Create texture for rendering
                glGenTextures(1, &render_texture);
                detail::check_gl_error("glGenTextures");

                // RAII for texture
                auto tex_deleter = [](GLuint id) { if (id) glDeleteTextures(1, &id); };
                detail::gl_resource <decltype(tex_deleter)> tex_guard(render_texture, tex_deleter);

                glBindTexture(GL_TEXTURE_2D, render_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                             static_cast <GLsizei>(dst_width),
                             static_cast <GLsizei>(dst_height),
                             0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                detail::check_gl_error("glTexImage2D");

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture, 0);
                detail::check_gl_error("glFramebufferTexture2D");

                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    return nullptr;
                }

                SDL_Surface* result = nullptr;
                try {
                    // Upload input surface to texture
                    upload_sdl_surface(input);

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
                    detail::check_gl_error("glDrawArrays");

                    // Read pixels from framebuffer
                    glPixelStorei(GL_PACK_ALIGNMENT, 4);

                    // Reuse pixel buffer for better performance
                    size_t required_size = dst_width * dst_height * 4;
                    if (pixel_buffer_.size() < required_size) {
                        pixel_buffer_.resize(required_size);
                    }

                    glReadPixels(0, 0, static_cast <GLsizei>(dst_width), static_cast <GLsizei>(dst_height),
                                 GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer_.data());
                    detail::check_gl_error("glReadPixels");

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
                        auto* dst_pixels = static_cast <unsigned char*>(result->pixels);
                        const size_t row_size = dst_width * 4;

                        for (size_t y = 0; y < dst_height; ++y) {
                            size_t src_row = (dst_height - 1 - y) * row_size;
                            size_t dst_offset = y * static_cast <size_t>(result->pitch);
                            // Use actual pitch, not calculated

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
            static const char* get_fragment_shader_for_algorithm(algorithm algo) {
                switch (algo) {
                    case EPX:
                    case AdvMAME2x:
                        return shader_source::epx_fragment_shader;
                    case Scale2x:
                        return shader_source::scale2x_fragment_shader;
                    case Scale2xSFX:
                        return shader_source::scale2x_sfx_fragment_shader;
                    case Scale3x:
                    case AdvMAME3x:
                        return shader_source::scale3x_fragment_shader;
                    case Eagle:
                        return shader_source::eagle_fragment_shader;
                    case TwoXSaI:
                        return shader_source::twoxsai_fragment_shader;
                    case AAScale2x:
                        return shader_source::aascale2x_fragment_shader;
                    case AAScale4x:
                        return shader_source::aascale4x_fragment_shader;
                    case Scale4x:
                        return shader_source::scale4x_fragment_shader;
                    case OmniScale:
                        return shader_source::omniscale_fragment_shader;
                    case DEFAULT:
                    default:
                        return shader_source::default_fragment_shader;
                }
            }

            // Compile shader program and return shader_program struct
            static shader_program compile_shader_program(const char* vertex_src, const char* fragment_src) {
                shader_program program;

                GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);
                GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

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
                    std::vector <char> infoLog(static_cast <size_t>(logLength > 0 ? logLength : 512));
                    glGetProgramInfoLog(program.program, static_cast <GLsizei>(infoLog.size()), nullptr,
                                        infoLog.data());
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

            void init_gl() {
                // Setup vertex data
                float vertices[] = {
                    // positions   // texture coords
                    -1.0f, 1.0f, 0.0f, 0.0f,
                    -1.0f, -1.0f, 0.0f, 1.0f,
                    1.0f, 1.0f, 1.0f, 0.0f,
                    1.0f, -1.0f, 1.0f, 1.0f,
                };

                glGenVertexArrays(1, &vao_);
                detail::check_gl_error("glGenVertexArrays");
                if (vao_ == 0) {
                    throw std::runtime_error("Failed to generate vertex array");
                }

                glGenBuffers(1, &vbo_);
                detail::check_gl_error("glGenBuffers");
                if (vbo_ == 0) {
                    glDeleteVertexArrays(1, &vao_);
                    throw std::runtime_error("Failed to generate vertex buffer");
                }

                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                detail::check_gl_error("glBufferData");

                // Position attribute
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast <void*>(nullptr));
                glEnableVertexAttribArray(0);
                detail::check_gl_error("glVertexAttribPointer position");

                // TexCoord attribute
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                      reinterpret_cast <void*>(2 * sizeof(float)));
                glEnableVertexAttribArray(1);
                detail::check_gl_error("glVertexAttribPointer texcoord");

                glBindVertexArray(0);

                // Create texture
                glGenTextures(1, &texture_);
                detail::check_gl_error("glGenTextures");
                if (texture_ == 0) {
                    throw std::runtime_error("Failed to generate texture");
                }

                glBindTexture(GL_TEXTURE_2D, texture_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                detail::check_gl_error("glTexParameteri");
            }

            static GLuint compile_shader(GLenum type, const char* source) {
                GLuint shader = glCreateShader(type);
                if (shader == 0) {
                    throw std::runtime_error("Failed to create shader");
                }
                detail::check_gl_error("glCreateShader");

                glShaderSource(shader, 1, &source, nullptr);
                glCompileShader(shader);
                detail::check_gl_error("glCompileShader");

                GLint success;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    // Get the actual log length for proper buffer allocation
                    GLint logLength = 0;
                    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

                    std::vector <char> infoLog(static_cast <size_t>(logLength > 0 ? logLength : 512));
                    glGetShaderInfoLog(shader, static_cast <GLsizei>(infoLog.size()), nullptr, infoLog.data());

                    glDeleteShader(shader); // Clean up the failed shader

                    std::string shaderType = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
                    throw std::runtime_error(
                        shaderType + " shader compilation failed: " + std::string(infoLog.data()));
                }

                return shader;
            }

            GLuint create_shader_program(const char* vertex_src, const char* fragment_src) {
                GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);
                GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

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
            void upload_texture(const InputImage& input) {
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

            void upload_sdl_surface(SDL_Surface* surface) const {
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
