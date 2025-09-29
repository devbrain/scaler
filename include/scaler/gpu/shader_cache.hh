#pragma once

#include <scaler/gpu/opengl_utils.hh>
#include <scaler/algorithm.hh>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace scaler::gpu {

    /**
     * Compiled shader program with uniform locations
     */
    struct shader_program {
        detail::program_resource program;

        // Common uniform locations
        GLint u_texture = -1;
        GLint u_texture_size = -1;
        GLint u_output_size = -1;
        GLint u_scale = -1;

        // Additional uniforms for specific algorithms
        GLint u_time = -1;  // For animated effects
        GLint u_sharpness = -1;  // For adjustable sharpness

        bool is_valid() const {
            return program.is_valid();
        }

        void use() const {
            if (!is_valid()) {
                throw std::runtime_error("Attempting to use invalid shader program");
            }

            GLuint prog_id = program.get();
            if (prog_id == 0) {
                throw std::runtime_error("Shader program ID is 0 - invalid program");
            }

            // Clear any existing GL error
            while (glGetError() != GL_NO_ERROR) {}

            glUseProgram(prog_id);

            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                throw std::runtime_error("glUseProgram failed with program ID " +
                                       std::to_string(prog_id) +
                                       ", GL error: 0x" +
                                       std::to_string(error));
            }
        }
    };

    /**
     * Thread-safe shader compilation and caching
     */
    class shader_cache {
    private:
        mutable std::unique_ptr<std::mutex> mutex_;
        std::unordered_map<algorithm, shader_program> algo_cache_;
        std::unordered_map<std::string, shader_program> string_cache_;

        // Currently active shader
        algorithm current_algorithm_ = algorithm::Nearest;
        const shader_program* current_shader_ = nullptr;

        /**
         * Compile a shader from source
         */
        static detail::shader_resource compile_shader(GLenum type, const char* source) {
            auto shader = detail::make_shader(type);

            if (!shader.is_valid()) {
                throw std::runtime_error("Failed to create shader");
            }

            glShaderSource(shader.get(), 1, &source, nullptr);
            glCompileShader(shader.get());

            // Check compilation status
            GLint success;
            glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &success);

            if (!success) {
                GLint log_length;
                glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &log_length);

                std::vector<char> error_log(log_length > 0 ? SCALER_GLINT_TO_SIZE(log_length) : 512);
                glGetShaderInfoLog(shader.get(), static_cast<GLsizei>(error_log.size()),
                                  nullptr, error_log.data());

                std::string shader_type = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
                throw std::runtime_error("Failed to compile " + shader_type + " shader: " +
                                       std::string(error_log.data()));
            }

            return shader;
        }

        /**
         * Link shader program
         */
        static shader_program link_program(const detail::shader_resource& vertex,
                                          const detail::shader_resource& fragment) {
            shader_program result;
            result.program = detail::make_program();

            if (!result.program.is_valid()) {
                throw std::runtime_error("Failed to create shader program");
            }

            glAttachShader(result.program.get(), vertex.get());
            glAttachShader(result.program.get(), fragment.get());
            glLinkProgram(result.program.get());

            // Check link status
            GLint success;
            glGetProgramiv(result.program.get(), GL_LINK_STATUS, &success);

            if (!success) {
                GLint log_length;
                glGetProgramiv(result.program.get(), GL_INFO_LOG_LENGTH, &log_length);

                std::vector<char> error_log(log_length > 0 ? SCALER_GLINT_TO_SIZE(log_length) : 512);
                glGetProgramInfoLog(result.program.get(), static_cast<GLsizei>(error_log.size()),
                                   nullptr, error_log.data());

                throw std::runtime_error("Failed to link shader program: " +
                                       std::string(error_log.data()));
            }

            // Note: glValidateProgram is not called here because it can fail
            // if no framebuffer is bound, even if the program is valid

            // Detach shaders after linking
            glDetachShader(result.program.get(), vertex.get());
            glDetachShader(result.program.get(), fragment.get());

            // Get uniform locations
            result.u_texture = glGetUniformLocation(result.program.get(), "u_texture");
            result.u_texture_size = glGetUniformLocation(result.program.get(), "u_texture_size");
            result.u_output_size = glGetUniformLocation(result.program.get(), "u_output_size");
            result.u_scale = glGetUniformLocation(result.program.get(), "u_scale");
            result.u_time = glGetUniformLocation(result.program.get(), "u_time");
            result.u_sharpness = glGetUniformLocation(result.program.get(), "u_sharpness");

            return result;
        }

    public:
        shader_cache() : mutex_(std::make_unique<std::mutex>()) {}

        // Move constructor
        shader_cache(shader_cache&& other) noexcept
            : mutex_(std::make_unique<std::mutex>())
            , algo_cache_(std::move(other.algo_cache_))
            , string_cache_(std::move(other.string_cache_))
            , current_algorithm_(other.current_algorithm_)
            , current_shader_(nullptr) {}

        // Move assignment
        shader_cache& operator=(shader_cache&& other) noexcept {
            if (this != &other) {
                algo_cache_ = std::move(other.algo_cache_);
                string_cache_ = std::move(other.string_cache_);
                current_algorithm_ = other.current_algorithm_;
                current_shader_ = nullptr;
            }
            return *this;
        }

        // Delete copy operations
        shader_cache(const shader_cache&) = delete;
        shader_cache& operator=(const shader_cache&) = delete;

        /**
         * Compile shader program from source
         */
        shader_program compile(const char* vertex_source, const char* fragment_source) {
            detail::scoped_gl_error_check error_check("shader_cache::compile");

            auto vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
            auto fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);

            return link_program(vertex, fragment);
        }

        /**
         * Get or compile shader with string key
         */
        const shader_program& get_or_compile(const std::string& key,
                                            const char* vertex_source,
                                            const char* fragment_source) {
            std::lock_guard<std::mutex> lock(*mutex_);

            // Check if already cached
            auto it = string_cache_.find(key);
            if (it != string_cache_.end()) {
                return it->second;
            }

            // Compile and cache
            shader_program program = compile(vertex_source, fragment_source);
            auto [inserted_it, success] = string_cache_.emplace(key, std::move(program));
            return inserted_it->second;
        }

        /**
         * Get or compile shader for algorithm
         */
        shader_program* get_or_compile(algorithm algo,
                                      const char* vertex_source,
                                      const char* fragment_source) {
            std::lock_guard<std::mutex> lock(*mutex_);

            // Check if already cached
            auto it = algo_cache_.find(algo);
            if (it != algo_cache_.end()) {
                return &it->second;
            }

            // Compile and cache
            try {
                shader_program program = compile(vertex_source, fragment_source);
                auto [inserted_it, success] = algo_cache_.emplace(algo, std::move(program));
                return &inserted_it->second;
            } catch (const std::exception& e) {
                // Log error and return nullptr
                // In production, use proper logging
                return nullptr;
            }
        }

        /**
         * Use shader for algorithm
         */
        bool use(algorithm algo) {
            std::lock_guard<std::mutex> lock(*mutex_);

            auto it = algo_cache_.find(algo);
            if (it != algo_cache_.end() && it->second.is_valid()) {
                it->second.use();
                current_algorithm_ = algo;
                current_shader_ = &it->second;
                return true;
            }

            return false;
        }

        /**
         * Get current active shader
         */
        const shader_program* get_current() const {
            std::lock_guard<std::mutex> lock(*mutex_);
            return current_shader_;
        }

        /**
         * Get current algorithm
         */
        algorithm get_current_algorithm() const {
            std::lock_guard<std::mutex> lock(*mutex_);
            return current_algorithm_;
        }

        /**
         * Clear all cached shaders
         */
        void clear() {
            std::lock_guard<std::mutex> lock(*mutex_);
            algo_cache_.clear();
            string_cache_.clear();
            current_shader_ = nullptr;
            current_algorithm_ = algorithm::Nearest;
        }

        /**
         * Precompile shader for faster first use
         */
        void precompile(algorithm algo,
                       const char* vertex_source,
                       const char* fragment_source) {
            get_or_compile(algo, vertex_source, fragment_source);
        }

        /**
         * Check if shader is cached
         */
        bool is_cached(algorithm algo) const {
            std::lock_guard<std::mutex> lock(*mutex_);
            return algo_cache_.find(algo) != algo_cache_.end();
        }

        /**
         * Get number of cached shaders
         */
        size_t size() const {
            std::lock_guard<std::mutex> lock(*mutex_);
            return algo_cache_.size() + string_cache_.size();
        }

        /**
         * Set uniform values for current shader
         */
        void set_uniforms(GLsizei input_width, GLsizei input_height,
                         GLsizei output_width, GLsizei output_height,
                         float scale_factor) {
            const shader_program* shader = get_current();
            if (!shader) return;

            if (shader->u_texture >= 0) {
                glUniform1i(shader->u_texture, 0);  // Texture unit 0
            }

            if (shader->u_texture_size >= 0) {
                glUniform2f(shader->u_texture_size,
                           static_cast<float>(input_width),
                           static_cast<float>(input_height));
            }

            if (shader->u_output_size >= 0) {
                glUniform2f(shader->u_output_size,
                           static_cast<float>(output_width),
                           static_cast<float>(output_height));
            }

            if (shader->u_scale >= 0) {
                glUniform1f(shader->u_scale, scale_factor);
            }
        }

        /**
         * Set optional uniform values
         */
        void set_optional_uniform(const char* name, float value) {
            const shader_program* shader = get_current();
            if (!shader || !shader->is_valid()) return;

            GLint location = glGetUniformLocation(shader->program.get(), name);
            if (location >= 0) {
                glUniform1f(location, value);
            }
        }

        void set_optional_uniform(const char* name, int value) {
            const shader_program* shader = get_current();
            if (!shader || !shader->is_valid()) return;

            GLint location = glGetUniformLocation(shader->program.get(), name);
            if (location >= 0) {
                glUniform1i(location, value);
            }
        }
    };

} // namespace scaler::gpu