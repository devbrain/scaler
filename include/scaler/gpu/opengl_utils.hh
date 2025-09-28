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

// Platform-specific OpenGL headers
#ifdef SCALER_PLATFORM_WINDOWS
    // Windows requires windows.h before GL headers
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <GL/glew.h>  // GLEW handles extension loading on Windows
#elif defined(SCALER_PLATFORM_MACOS)
    // macOS uses different OpenGL headers and doesn't need GLEW for core functionality
    #include <OpenGL/gl3.h>
    #include <OpenGL/gl3ext.h>
    // Define GLEW stubs for macOS to maintain API compatibility
    #ifndef GLEW_OK
        #define GLEW_OK 0
        #define glewInit() (0)  // Always succeeds on macOS
        #define glewGetErrorString(x) "No error"
    #endif
#else
    // Linux/Unix uses standard GL headers with GLEW
    #include <GL/glew.h>
#endif

#include <string>
#include <stdexcept>
#include <functional>
#include <cstring>  // For strstr

namespace scaler::gpu::detail {

    /**
     * RAII wrapper for OpenGL resources
     * Automatically manages the lifecycle of OpenGL objects
     */
    template<typename Deleter>
    class gl_resource {
    private:
        GLuint id_;
        Deleter deleter_;
        bool owned_;

    public:
        gl_resource() : id_(0), owned_(false) {}

        gl_resource(GLuint id, Deleter del)
            : id_(id), deleter_(del), owned_(true) {}

        ~gl_resource() {
            release();
        }

        // Move only - no copying
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

        // Delete copy constructor and copy assignment
        gl_resource(const gl_resource&) = delete;
        gl_resource& operator=(const gl_resource&) = delete;

        void release() {
            if (owned_ && id_ != 0) {
                deleter_(id_);
                id_ = 0;
                owned_ = false;
            }
        }

        void reset(GLuint new_id = 0) {
            release();
            if (new_id != 0) {
                id_ = new_id;
                owned_ = true;
            }
        }

        GLuint get() const { return id_; }
        GLuint* ptr() { return &id_; }
        operator GLuint() const { return id_; }
        bool is_valid() const { return id_ != 0; }

        // Release ownership without deleting
        GLuint release_ownership() {
            owned_ = false;
            return id_;
        }
    };

    // Specific resource types with appropriate deleters
    using texture_resource = gl_resource<std::function<void(GLuint)>>;
    using framebuffer_resource = gl_resource<std::function<void(GLuint)>>;
    using shader_resource = gl_resource<std::function<void(GLuint)>>;
    using program_resource = gl_resource<std::function<void(GLuint)>>;
    using buffer_resource = gl_resource<std::function<void(GLuint)>>;
    using vertex_array_resource = gl_resource<std::function<void(GLuint)>>;

    // Factory functions for creating resources
    inline texture_resource make_texture() {
        GLuint id;
        glGenTextures(1, &id);
        return texture_resource(id, [](GLuint id) { glDeleteTextures(1, &id); });
    }

    inline framebuffer_resource make_framebuffer() {
        GLuint id;
        glGenFramebuffers(1, &id);
        return framebuffer_resource(id, [](GLuint id) { glDeleteFramebuffers(1, &id); });
    }

    inline shader_resource make_shader(GLenum type) {
        GLuint id = glCreateShader(type);
        return shader_resource(id, [](GLuint id) { glDeleteShader(id); });
    }

    inline program_resource make_program() {
        GLuint id = glCreateProgram();
        return program_resource(id, [](GLuint id) { glDeleteProgram(id); });
    }

    inline buffer_resource make_buffer() {
        GLuint id;
        glGenBuffers(1, &id);
        return buffer_resource(id, [](GLuint id) { glDeleteBuffers(1, &id); });
    }

    inline vertex_array_resource make_vertex_array() {
        GLuint id;
        glGenVertexArrays(1, &id);
        return vertex_array_resource(id, [](GLuint id) { glDeleteVertexArrays(1, &id); });
    }

    /**
     * Check for OpenGL errors and throw exception if found
     */
    inline void check_gl_error(const char* operation) {
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::string error_str;
            switch (error) {
                case GL_INVALID_ENUM:
                    error_str = "GL_INVALID_ENUM";
                    break;
                case GL_INVALID_VALUE:
                    error_str = "GL_INVALID_VALUE";
                    break;
                case GL_INVALID_OPERATION:
                    error_str = "GL_INVALID_OPERATION";
                    break;
                case GL_INVALID_FRAMEBUFFER_OPERATION:
                    error_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
                    break;
                case GL_OUT_OF_MEMORY:
                    error_str = "GL_OUT_OF_MEMORY";
                    break;
                default:
                    error_str = "Unknown error " + std::to_string(error);
            }
            throw std::runtime_error(std::string("OpenGL error in ") + operation + ": " + error_str);
        }
    }

    /**
     * RAII OpenGL error checker - checks for errors in destructor
     */
    class scoped_gl_error_check {
    private:
        const char* operation_;

    public:
        explicit scoped_gl_error_check(const char* operation) : operation_(operation) {
            // Clear any existing errors
            while (glGetError() != GL_NO_ERROR) {}
        }

        ~scoped_gl_error_check() {
            try {
                check_gl_error(operation_);
            } catch (const std::exception& e) {
                // Log error but don't throw from destructor
                // In production, you'd use a proper logging system
            }
        }
    };

    /**
     * Scoped OpenGL state management
     */
    template<GLenum Cap>
    class scoped_gl_enable {
    private:
        bool was_enabled_;

    public:
        scoped_gl_enable() {
            was_enabled_ = glIsEnabled(Cap);
            if (!was_enabled_) {
                glEnable(Cap);
            }
        }

        ~scoped_gl_enable() {
            if (!was_enabled_) {
                glDisable(Cap);
            }
        }
    };

    /**
     * Scoped framebuffer binding
     */
    class scoped_framebuffer_bind {
    private:
        GLint previous_fbo_;

    public:
        explicit scoped_framebuffer_bind(GLuint fbo) {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo_);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        }

        ~scoped_framebuffer_bind() {
            glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo_);
        }
    };

    /**
     * Scoped viewport
     */
    class scoped_viewport {
    private:
        GLint previous_viewport_[4];

    public:
        scoped_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
            glGetIntegerv(GL_VIEWPORT, previous_viewport_);
            glViewport(x, y, width, height);
        }

        ~scoped_viewport() {
            glViewport(previous_viewport_[0], previous_viewport_[1],
                      previous_viewport_[2], previous_viewport_[3]);
        }
    };

    /**
     * Helper to get OpenGL version info
     */
    struct gl_version_info {
        int major;
        int minor;
        bool is_es;

        static gl_version_info get() {
            gl_version_info info;
            glGetIntegerv(GL_MAJOR_VERSION, &info.major);
            glGetIntegerv(GL_MINOR_VERSION, &info.minor);

            const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            info.is_es = (version_str && strstr(version_str, "OpenGL ES") != nullptr);

            return info;
        }

        bool supports(int required_major, int required_minor) const {
            return (major > required_major) ||
                   (major == required_major && minor >= required_minor);
        }
    };

} // namespace scaler::gpu::detail