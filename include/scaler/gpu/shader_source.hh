//
// Created by igor on 28/09/2025.
//

#pragma once

namespace scaler::gpu::shader_source {
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

    // Nearest neighbor fragment shader
    static constexpr const char* nearest_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;

        void main() {
            FragColor = texture(u_texture, v_texCoord);
        }
    )";

    // Bilinear fragment shader
    static constexpr const char* bilinear_fragment_shader = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        uniform vec2 u_texture_size;

        void main() {
            vec2 texel = 1.0 / u_texture_size;
            vec2 coord = v_texCoord * u_texture_size - 0.5;
            vec2 frac = fract(coord);
            coord = floor(coord) + 0.5;

            vec4 tl = texture(u_texture, (coord + vec2(0, 0)) / u_texture_size);
            vec4 tr = texture(u_texture, (coord + vec2(1, 0)) / u_texture_size);
            vec4 bl = texture(u_texture, (coord + vec2(0, 1)) / u_texture_size);
            vec4 br = texture(u_texture, (coord + vec2(1, 1)) / u_texture_size);

            vec4 top = mix(tl, tr, frac.x);
            vec4 bottom = mix(bl, br, frac.x);
            FragColor = mix(top, bottom, frac.y);
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

    // Scale3xSFX shader - Improved Scale3x algorithm by Sp00kyFox
    static constexpr const char* scale3x_sfx_fragment_shader = R"(
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
            // For now, use regular Scale3x - SFX variant would need more complex logic
            vec2 texel = 1.0 / u_texture_size;
            vec2 output_pixel = v_texCoord * u_output_size;
            vec2 src_pixel = floor(output_pixel / 3.0);
            vec2 block_pos = fract(output_pixel / 3.0);
            vec2 src_tex_coord = (src_pixel + 0.5) / u_texture_size;

            // Sample the 3x3 neighborhood
            vec4 A = sampleTexture(src_tex_coord + vec2(-texel.x, -texel.y));
            vec4 B = sampleTexture(src_tex_coord + vec2(0.0, -texel.y));
            vec4 C = sampleTexture(src_tex_coord + vec2(texel.x, -texel.y));
            vec4 D = sampleTexture(src_tex_coord + vec2(-texel.x, 0.0));
            vec4 E = sampleTexture(src_tex_coord);
            vec4 F = sampleTexture(src_tex_coord + vec2(texel.x, 0.0));
            vec4 G = sampleTexture(src_tex_coord + vec2(-texel.x, texel.y));
            vec4 H = sampleTexture(src_tex_coord + vec2(0.0, texel.y));
            vec4 I = sampleTexture(src_tex_coord + vec2(texel.x, texel.y));

            // Scale3x algorithm rules
            vec4 E0 = E, E1 = E, E2 = E, E3 = E, E4 = E, E5 = E, E6 = E, E7 = E, E8 = E;

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

            vec4 result;
            int block_x = clamp(int(block_pos.x * 3.0), 0, 2);
            int block_y = clamp(int(block_pos.y * 3.0), 0, 2);

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
}
