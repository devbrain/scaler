#include <doctest/doctest.h>
#include <scaler/gpu/platform_info.hh>
#include <iostream>

using namespace scaler::gpu;

TEST_CASE("Platform Configuration") {
    SUBCASE("Platform Detection") {
        std::string platform = platform_info::get_platform();
        INFO("Detected platform: " << platform);

        // We should detect one of the known platforms
        CHECK((platform == "Windows" ||
               platform == "macOS" ||
               platform == "Linux" ||
               platform == "Unix"));

        // On current system, we expect Linux
        #ifdef __linux__
        CHECK(platform == "Linux");
        #endif
    }

    SUBCASE("GLEW Requirements") {
        bool needs_glew = platform_info::requires_glew();
        INFO("GLEW required: " << (needs_glew ? "yes" : "no"));

        #ifdef __APPLE__
        CHECK(needs_glew == false);
        #else
        CHECK(needs_glew == true);
        #endif
    }

    SUBCASE("OpenGL Header Path") {
        std::string header = platform_info::get_gl_header_path();
        INFO("GL header path: " << header);
        CHECK(!header.empty());

        // Verify header path matches platform
        #ifdef __linux__
        CHECK(header.find("Linux/Unix") != std::string::npos);
        #endif
    }

    SUBCASE("OpenGL Support") {
        CHECK(platform_info::supports_gl33_core() == true);

        int flags = platform_info::get_recommended_gl_flags();
        #ifdef __APPLE__
        CHECK(flags == SDL_GL_CONTEXT_FORWARD_COMPATIBLE);
        #else
        CHECK(flags == 0);
        #endif
    }
}