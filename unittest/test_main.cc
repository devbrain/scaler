// Main test runner for musac unit tests
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <cstdlib>

int main(int argc, char** argv) {
    // Set SDL to use dummy audio driver for testing
    // This avoids needing actual audio hardware and prevents the null backend issues
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    
    // Also set video driver to dummy to avoid any GUI dependencies
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    
    // Run the tests
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    
    int res = context.run();
    
    if(context.shouldExit()) {
        return res;
    }
    
    return res;
}