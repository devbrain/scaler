#include <scaler/sdl_scalers.hh>
#include <scaler/epx_crtp.hh>
#include <vector>
#include <memory>

// Example 1: Using with SDL (simple)
void example_sdl_simple() {
    SDL_Surface* input_surface = SDL_LoadBMP("input.bmp");
    
    // Simple one-liner using convenience function
    SDL_Surface* scaled = scaleEpxSDL(input_surface);
    
    SDL_SaveBMP(scaled, "output_epx.bmp");
    SDL_DestroySurface(scaled);
    SDL_DestroySurface(input_surface);
}

// Example 2: Using with SDL (explicit CRTP)
void example_sdl_crtp() {
    SDL_Surface* input_surface = SDL_LoadBMP("input.bmp");
    
    // Create CRTP wrapper
    SDLInputImage input(input_surface);
    
    // Scale using CRTP interface
    auto output = scaleEpx<SDLInputImage, SDLOutputImage>(input);
    
    // Get SDL surface and save
    SDL_SaveBMP(output.get_surface(), "output_epx_crtp.bmp");
    
    SDL_DestroySurface(input_surface);
}

// Example 3: Custom image backend implementation
class SimpleImage : public InputImageBase<SimpleImage, uvec3>,
                    public OutputImageBase<SimpleImage, uvec3> {
public:
    SimpleImage(int w, int h) 
        : m_width(w), m_height(h), m_data(w * h) {}
    
    // Input interface
    [[nodiscard]] int width_impl() const { return m_width; }
    [[nodiscard]] int height_impl() const { return m_height; }
    [[nodiscard]] uvec3 get_pixel_impl(int x, int y) const {
        return m_data[y * m_width + x];
    }
    
    // Output interface  
    void set_pixel_impl(int x, int y, const uvec3& pixel) {
        m_data[y * m_width + x] = pixel;
    }
    
    // Constructor for scaling operations
    SimpleImage(int w, int h, const SimpleImage& template_img)
        : SimpleImage(w, h) {}
    
private:
    int m_width, m_height;
    std::vector<uvec3> m_data;
};

// Example 4: Using custom backend
void example_custom_backend() {
    // Create a simple test image
    SimpleImage input(10, 10);
    
    // Fill with test pattern
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            input.set_pixel(x, y, {
                static_cast<unsigned>(x * 25), 
                static_cast<unsigned>(y * 25), 
                128
            });
        }
    }
    
    // Scale using CRTP interface - works with any backend!
    auto output = scaleEpx<SimpleImage, SimpleImage>(input);
    
    // Output is now 20x20 scaled version
}

// Example 5: Chain multiple scalers
template<typename InputImage, typename OutputImage>
auto scale4x(const InputImage& src) -> OutputImage {
    // Scale 2x twice
    auto temp = scaleEpx<InputImage, OutputImage>(src);
    return scaleEpx<OutputImage, OutputImage>(temp);
}