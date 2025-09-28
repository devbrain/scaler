# Platform-Specific OpenGL Support

The GPU scaler now includes comprehensive platform-specific OpenGL header support, ensuring compatibility across Windows, macOS, and Linux/Unix systems.

## Platform Detection

The system automatically detects the build platform and includes the appropriate OpenGL headers:

### Windows
- Headers: `GL/glew.h`, `GL/gl.h`, `GL/glext.h`
- Requirements:
  - `windows.h` must be included before GL headers
  - GLEW is required for extension loading
  - `WIN32_LEAN_AND_MEAN` is defined to minimize Windows.h overhead

### macOS
- Headers: `OpenGL/gl3.h`, `OpenGL/gl3ext.h`
- Requirements:
  - Uses Apple's OpenGL framework
  - GLEW is NOT required (core OpenGL 3.3 is directly available)
  - May require forward-compatible context flag

### Linux/Unix
- Headers: `GL/glew.h`, `GL/gl.h`, `GL/glext.h`
- Requirements:
  - GLEW is required for extension loading
  - `GL_GLEXT_PROTOTYPES` is defined for function prototypes

## Build Configuration

The CMake build system automatically handles platform differences:

```cmake
# Platform-specific GLEW handling
if(NOT APPLE)
    find_package(GLEW REQUIRED)
    target_link_libraries(scaler_unittest PRIVATE GLEW::GLEW)
endif()
```

## Usage in Code

The platform detection is handled automatically through `opengl_utils.hh`:

```cpp
#include <scaler/gpu/opengl_utils.hh>

// Platform macros are now available:
// SCALER_PLATFORM_WINDOWS
// SCALER_PLATFORM_MACOS
// SCALER_PLATFORM_LINUX
// SCALER_PLATFORM_UNIX

// GLEW initialization is wrapped:
#ifndef SCALER_PLATFORM_MACOS
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // Handle error
    }
#endif
```

## Platform Information API

A utility class is provided to query platform configuration:

```cpp
#include <scaler/gpu/platform_info.hh>

using namespace scaler::gpu;

// Get platform name
std::string platform = platform_info::get_platform(); // "Linux", "Windows", "macOS"

// Check if GLEW is required
bool needs_glew = platform_info::requires_glew(); // false on macOS, true elsewhere

// Get OpenGL header path
std::string header = platform_info::get_gl_header_path();

// Get recommended context flags
int flags = platform_info::get_recommended_gl_flags();
```

## Testing

Platform configuration is automatically tested:
- Platform detection correctness
- GLEW requirement validation
- OpenGL header path verification
- Context flag recommendations

## Cross-Platform Compatibility

The implementation ensures:
1. **Header Compatibility**: Correct headers for each platform
2. **GLEW Handling**: Only initialized where needed
3. **Context Creation**: Platform-specific flags applied
4. **Build System**: CMake automatically configures dependencies

## Supported Platforms

| Platform | OpenGL Version | GLEW Required | Tested |
|----------|----------------|---------------|--------|
| Linux    | 3.3+ Core      | Yes           | ✓      |
| Windows  | 3.3+ Core      | Yes           | -      |
| macOS    | 3.3+ Core      | No            | -      |
| Unix/BSD | 3.3+ Core      | Yes           | -      |

## Future Considerations

- WebGL support through Emscripten could be added
- Vulkan backend could be implemented alongside OpenGL
- Metal backend for modern macOS/iOS could be added