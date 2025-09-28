/**
 * Example: SDL GPU Scaler with Preallocated Textures
 *
 * Demonstrates the game engine pattern where textures are preallocated
 * once during initialization and reused during the render loop.
 */

#include <scaler/gpu/sdl/sdl_texture_adapter.hh>
#include <SDL.h>
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create window with OpenGL context
    SDL_Window* window = SDL_CreateWindow(
        "SDL GPU Scaler Example",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create OpenGL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE
    );

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    try {
        // Create GPU scaler adapter
        scaler::gpu::sdl_texture_adapter gpu_scaler(renderer);

        // ========================================
        // INITIALIZATION PHASE - Allocate Once
        // ========================================

        // Create a low-resolution game texture (160x120)
        SDL_Texture* game_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            160, 120
        );

        // Draw something to the game texture (in a real game, this would be your game render)
        SDL_SetRenderTarget(renderer, game_texture);
        SDL_SetRenderDrawColor(renderer, 64, 128, 255, 255);
        SDL_RenderClear(renderer);

        // Draw a simple pattern
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = {40, 30, 80, 60};
        SDL_RenderFillRect(renderer, &rect);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect2 = {60, 45, 40, 30};
        SDL_RenderFillRect(renderer, &rect2);

        // Reset render target
        SDL_SetRenderTarget(renderer, nullptr);

        // Calculate output dimensions for 4x scaling with EPX algorithm
        auto output_dims = scaler::gpu::sdl_texture_adapter::get_output_size(
            game_texture,
            scaler::algorithm::EPX,
            2.0f  // EPX only supports 2x
        );

        std::cout << "Input: 160x120, Output: "
                  << output_dims.width << "x" << output_dims.height << std::endl;

        // PREALLOCATE the upscaled texture (this happens ONCE)
        SDL_Texture* upscaled_texture = scaler::gpu::sdl_texture_adapter::create_output_texture(
            renderer,
            output_dims.width,
            output_dims.height
        );

        // Optionally precompile shaders for faster first frame
        gpu_scaler.precompile_shaders();

        // ========================================
        // RENDER LOOP - Reuse Preallocated Textures
        // ========================================

        bool quit = false;
        SDL_Event e;
        int frame_count = 0;
        Uint32 start_time = SDL_GetTicks();

        while (!quit) {
            // Handle events
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        quit = true;
                    }
                }
            }

            // In a real game, you would update and render to game_texture here
            // For this example, we'll just animate the background color
            SDL_SetRenderTarget(renderer, game_texture);
            int color = (SDL_GetTicks() / 10) % 256;
            SDL_SetRenderDrawColor(renderer, color/2, color/3, color, 255);
            SDL_RenderClear(renderer);

            // Redraw our pattern
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &rect2);
            SDL_SetRenderTarget(renderer, nullptr);

            // SCALE THE TEXTURE (reusing preallocated textures)
            if (!gpu_scaler.scale_texture(
                    game_texture,      // Low-res game render
                    upscaled_texture,  // Preallocated upscaled texture
                    scaler::algorithm::EPX)) {
                std::cerr << "Scaling failed: " << SDL_GetError() << std::endl;
            }

            // Clear screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Render the upscaled texture to screen
            SDL_RenderCopy(renderer, upscaled_texture, nullptr, nullptr);

            // Present
            SDL_RenderPresent(renderer);

            frame_count++;
        }

        // Calculate and display FPS
        Uint32 end_time = SDL_GetTicks();
        float seconds = (end_time - start_time) / 1000.0f;
        float fps = frame_count / seconds;
        std::cout << "Average FPS: " << fps << std::endl;

        // ========================================
        // CLEANUP - Destroy textures
        // ========================================
        SDL_DestroyTexture(upscaled_texture);
        SDL_DestroyTexture(game_texture);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Cleanup SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

/**
 * Example 2: Batch Processing Pattern
 *
 * For games with multiple layers or sprites that need scaling:
 *
 * // During initialization
 * std::vector<SDL_Texture*> sprite_textures = load_sprites();
 * std::vector<SDL_Texture*> scaled_sprites = gpu_scaler.scale_batch(
 *     sprite_textures,
 *     scaler::algorithm::Scale3x,
 *     3.0f
 * );
 *
 * // During gameplay - just use the pre-scaled textures
 * SDL_RenderCopy(renderer, scaled_sprites[sprite_index], nullptr, &dest_rect);
 */

/**
 * Example 3: Different Algorithms
 *
 * // Check what's available
 * if (scaler::gpu::sdl_texture_adapter::is_algorithm_available(scaler::algorithm::OmniScale)) {
 *     // OmniScale supports arbitrary scaling on GPU!
 *     auto dims = scaler::gpu::sdl_texture_adapter::get_output_size(
 *         input_texture,
 *         scaler::algorithm::OmniScale,
 *         2.5f  // Any scale factor!
 *     );
 * }
 */