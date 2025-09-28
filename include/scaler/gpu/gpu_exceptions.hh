#pragma once

#include <stdexcept>
#include <string>

namespace scaler::gpu {

    /**
     * Base exception for all GPU-related errors
     */
    class gpu_error : public std::runtime_error {
    public:
        explicit gpu_error(const std::string& what)
            : std::runtime_error("GPU Error: " + what) {}
    };

    /**
     * Exception for shader compilation/linking errors
     */
    class shader_error : public gpu_error {
    public:
        explicit shader_error(const std::string& what)
            : gpu_error("Shader: " + what) {}
    };

    /**
     * Exception for unsupported algorithm/scale combinations
     */
    class unsupported_operation_error : public gpu_error {
    public:
        explicit unsupported_operation_error(const std::string& what)
            : gpu_error("Unsupported: " + what) {}
    };

    /**
     * Exception for OpenGL-specific errors
     */
    class opengl_error : public gpu_error {
    public:
        explicit opengl_error(const std::string& operation, unsigned int error_code)
            : gpu_error("OpenGL in " + operation + ": error 0x" + to_hex(error_code)) {}

    private:
        static std::string to_hex(unsigned int value) {
            constexpr char hex_chars[] = "0123456789ABCDEF";
            std::string result = "0000";
            for (int i = 3; i >= 0; --i) {
                result[i] = hex_chars[value & 0xF];
                value >>= 4;
            }
            return result;
        }
    };

    /**
     * Exception for resource allocation failures
     */
    class resource_error : public gpu_error {
    public:
        explicit resource_error(const std::string& what)
            : gpu_error("Resource: " + what) {}
    };

} // namespace scaler::gpu