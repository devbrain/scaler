#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>

// Include all algorithm implementations
#include <scaler/cpu/epx.hh>
#include <scaler/cpu/eagle.hh>
#include <scaler/cpu/scale3x.hh>
#include <scaler/cpu/scale2x_sfx.hh>
#include <scaler/cpu/scale3x_sfx.hh>
#include <scaler/cpu/2xsai.hh>
#include <scaler/cpu/hq2x.hh>
#include <scaler/cpu/hq3x.hh>
#include <scaler/cpu/aascale.hh>
#include <scaler/cpu/xbr.hh>
#include <scaler/cpu/omniscale.hh>
#include <scaler/cpu/bilinear.hh>
#include <scaler/cpu/trilinear.hh>

namespace scaler {

// Scale-independent algorithm names
enum class Algorithm {
    // Simple scalers
    Nearest,        // Nearest neighbor (any scale)
    Bilinear,       // Bilinear interpolation (any scale)
    Trilinear,      // Trilinear interpolation with mipmapping (any scale)

    // Classic pixel art scalers
    EPX,            // Eric's Pixel Expansion (2x only)
    Eagle,          // Eagle algorithm (2x only)
    Scale,          // AdvMAME Scale (2x, 3x, 4x)
    ScaleSFX,       // Sp00kyFox variant (2x, 3x)
    Super2xSaI,     // 2xSaI interpolation (2x only)

    // High quality family
    HQ,             // High Quality (2x, 3x, 4x)

    // Anti-aliased scaling
    AAScale,        // Anti-aliased Scale (2x, 4x)

    // Advanced algorithms
    xBR,            // Hyllian's xBR (2x, 3x, 4x)
    xBRZ,           // Zenju's xBRZ variant (2x-6x) - TODO: add 5x, 6x support

    // Resolution independent
    OmniScale,      // Any scale factor

    // Aliases for backward compatibility
    AdvMAME = Scale,
};

// Exception for unsupported scale factors
class UnsupportedScaleException : public std::runtime_error {
public:
    UnsupportedScaleException(Algorithm algo, float requested,
                             const std::vector<float>& supported)
        : std::runtime_error(makeMessage(algo, requested, supported))
        , algorithm(algo)
        , requested_scale(requested)
        , supported_scales(supported) {}

    Algorithm algorithm;
    float requested_scale;
    std::vector<float> supported_scales;

private:
    static std::string makeMessage(Algorithm algo, float requested,
                                  const std::vector<float>& supported);
};

// Algorithm information and capabilities
class ScalerCapabilities {
public:
    struct AlgorithmInfo {
        std::string name;
        std::string description;
        std::vector<float> supported_scales;  // Empty = any scale
        bool arbitrary_scale;
        float min_scale = 1.0f;
        float max_scale = 8.0f;
    };

    static const AlgorithmInfo& getInfo(Algorithm algo) {
        static const std::map<Algorithm, AlgorithmInfo> db = getAlgorithmDatabase();
        auto it = db.find(algo);
        if (it != db.end()) {
            return it->second;
        }

        // Handle specific aliases - none needed since we removed separate enum values
        throw std::invalid_argument("Unknown algorithm");
    }

    static std::string getAlgorithmName(Algorithm algo) {
        return getInfo(algo).name;
    }

    static std::vector<float> getSupportedScales(Algorithm algo) {
        return getInfo(algo).supported_scales;
    }

    static bool supportsArbitraryScale(Algorithm algo) {
        return getInfo(algo).arbitrary_scale;
    }

    static bool isScaleSupported(Algorithm algo, float scale) {
        const auto& info = getInfo(algo);
        if (info.arbitrary_scale) {
            return scale >= info.min_scale && scale <= info.max_scale;
        }
        const auto& supported = getSupportedScales(algo);
        return std::find(supported.begin(), supported.end(), scale) != supported.end();
    }

    static std::vector<Algorithm> getAllAlgorithms() {
        return {
            Algorithm::Nearest,
            Algorithm::Bilinear,
            Algorithm::Trilinear,
            Algorithm::EPX,
            Algorithm::Eagle,
            Algorithm::Scale,
            Algorithm::ScaleSFX,
            Algorithm::Super2xSaI,
            Algorithm::HQ,
            Algorithm::AAScale,
            Algorithm::xBR,
            Algorithm::OmniScale
        };
    }

    static std::vector<Algorithm> getAlgorithmsForScale(float scale) {
        std::vector<Algorithm> result;
        for (Algorithm algo : getAllAlgorithms()) {
            if (isScaleSupported(algo, scale)) {
                result.push_back(algo);
            }
        }
        return result;
    }

private:
    static const std::map<Algorithm, AlgorithmInfo>& getAlgorithmDatabase() {
        static const std::map<Algorithm, AlgorithmInfo> db = {
            {Algorithm::Nearest, {
                "Nearest", "Nearest neighbor sampling - fastest, pixelated",
                {}, true, 0.1f, 10.0f
            }},
            {Algorithm::Bilinear, {
                "Bilinear", "Bilinear interpolation - smooth but blurry",
                {}, true, 0.1f, 10.0f
            }},
            {Algorithm::Trilinear, {
                "Trilinear", "Trilinear interpolation with mipmapping - better for downscaling",
                {}, true, 0.1f, 10.0f
            }},
            {Algorithm::EPX, {
                "EPX", "Eric's Pixel Expansion - good for pixel art",
                {2.0f}, false
            }},
            {Algorithm::Eagle, {
                "Eagle", "Eagle algorithm - smooth diagonal lines",
                {2.0f}, false
            }},
            {Algorithm::Scale, {
                "Scale", "AdvMAME Scale2x/3x/4x - sharp pixel art scaling",
                {2.0f, 3.0f, 4.0f}, false
            }},
            {Algorithm::ScaleSFX, {
                "ScaleSFX", "Sp00kyFox's improved Scale2x/3x - better edge handling",
                {2.0f, 3.0f}, false
            }},
            {Algorithm::Super2xSaI, {
                "Super2xSaI", "Super 2xSaI - smooth interpolation for pixel art",
                {2.0f}, false
            }},
            {Algorithm::HQ, {
                "HQ", "High Quality 2x/3x/4x - excellent for low-res games",
                {2.0f, 3.0f, 4.0f}, false
            }},
            {Algorithm::AAScale, {
                "AAScale", "Anti-Aliased Scale - smooth edges, less pixelated",
                {2.0f, 4.0f}, false
            }},
            {Algorithm::xBR, {
                "xBR", "Hyllian's xBR - advanced edge interpolation",
                {2.0f, 3.0f, 4.0f}, false
            }},
            {Algorithm::xBRZ, {
                "xBRZ", "Zenju's xBRZ - improved xBR variant",
                {2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, false
            }},
            {Algorithm::OmniScale, {
                "OmniScale", "OmniScale - resolution-independent scaling",
                {}, true, 1.0f, 8.0f
            }},
            {Algorithm::AdvMAME, {
                "Scale", "AdvMAME Scale2x/3x/4x - sharp pixel art scaling",
                {2.0f, 3.0f, 4.0f}, false
            }},
        };
        return db;
    }
};

// Main unified scaler interface
template<typename InputImage, typename OutputImage>
class UnifiedScaler {
public:
    // Main scaling method - throws UnsupportedScaleException if scale not supported
    static OutputImage scale(const InputImage& input,
                            Algorithm algo,
                            float scale_factor = 2.0f) {
        // Validate scale factor
        if (!ScalerCapabilities::isScaleSupported(algo, scale_factor)) {
            throw UnsupportedScaleException(algo, scale_factor,
                                          ScalerCapabilities::getSupportedScales(algo));
        }

        // Dispatch to appropriate implementation
        switch (algo) {
            case Algorithm::Nearest:
                return scaleNearest(input, scale_factor);

            case Algorithm::Bilinear:
                return scale_bilinear<InputImage, OutputImage>(input, scale_factor);

            case Algorithm::Trilinear:
                return scale_trilinear<InputImage, OutputImage>(input, scale_factor);

            case Algorithm::EPX:
                return scale_epx<InputImage, OutputImage>(input, 2);

            case Algorithm::Eagle:
                return scale_eagle<InputImage, OutputImage>(input, 2);

            case Algorithm::Scale:
                return dispatchScale(input, scale_factor);

            case Algorithm::ScaleSFX:
                return dispatchScaleSFX(input, scale_factor);

            case Algorithm::Super2xSaI:
                return scale_2x_sai<InputImage, OutputImage>(input, 2);

            case Algorithm::HQ:
                return dispatchHQ(input, scale_factor);

            case Algorithm::AAScale:
                return dispatchAAScale(input, scale_factor);

            case Algorithm::xBR:
                return dispatchXBR(input, scale_factor);

            case Algorithm::OmniScale:
                if (scale_factor == 2.0f) {
                    return scale_omni_scale_2x<InputImage, OutputImage>(input, 2);
                } else if (scale_factor == 3.0f) {
                    return scale_omni_scale_3x<InputImage, OutputImage>(input, 3);
                } else {
                    // For other scales, use repeated application or nearest neighbor
                    // This is a temporary solution
                    auto temp = scale_omni_scale_2x<InputImage, OutputImage>(input, 2);
                    float remaining_scale = scale_factor / 2.0f;
                    return scaleNearest(temp, remaining_scale);
                }

            default:
                throw std::runtime_error("Algorithm not implemented yet");
        }
    }


private:
    // Dispatch helpers for multi-scale algorithms
    static OutputImage dispatchScale(const InputImage& input, float scale_factor) {
        int scale = static_cast<int>(scale_factor);
        switch (scale) {
            case 2:
                return scale_adv_mame<InputImage, OutputImage>(input, 2);
            case 3:
                return scale_scale_3x<InputImage, OutputImage>(input, 3);
            case 4:
                // Scale4x is Scale2x applied twice
                auto temp = scale_adv_mame<InputImage, OutputImage>(input, 2);
                return scale_adv_mame<decltype(temp), OutputImage>(temp, 2);
        }
        throw std::logic_error("Invalid scale factor for Scale algorithm");
    }

    static OutputImage dispatchScaleSFX(const InputImage& input, float scale_factor) {
        int scale = static_cast<int>(scale_factor);
        switch (scale) {
            case 2:
                return scale_scale_2x_sfx<InputImage, OutputImage>(input, 2);
            case 3:
                return scale_scale_3x_sfx<InputImage, OutputImage>(input, 3);
            default:
                throw std::logic_error("Invalid scale factor for ScaleSFX algorithm");
        }
    }

    static OutputImage dispatchHQ(const InputImage& input, float scale_factor) {
        int scale = static_cast<int>(scale_factor);
        switch (scale) {
            case 2:
                return scale_hq2x<InputImage, OutputImage>(input, 2);
            case 3:
                return scale_hq_3x<InputImage, OutputImage>(input);
            case 4:
                // HQ4x would go here when implemented
                // For now, use HQ2x twice
                auto temp = scale_hq2x<InputImage, OutputImage>(input, 2);
                return scale_hq2x<decltype(temp), OutputImage>(temp, 2);
        }
        throw std::logic_error("Invalid scale factor for HQ algorithm");
    }

    static OutputImage dispatchAAScale(const InputImage& input, float scale_factor) {
        int scale = static_cast<int>(scale_factor);
        switch (scale) {
            case 2:
                return scale_aa_scale_2x<InputImage, OutputImage>(input, 2);
            case 4:
                return scale_aa_scale_4x<InputImage, OutputImage>(input, 4);
            default:
                throw std::logic_error("Invalid scale factor for AAScale algorithm");
        }
    }

    static OutputImage dispatchXBR(const InputImage& input, float scale_factor) {
        // XBR currently only supports 2x, for 3x and 4x we can apply it multiple times
        int scale = static_cast<int>(scale_factor);
        switch (scale) {
            case 2:
                return scale_xbr<InputImage, OutputImage>(input, 2);
            case 3:
                // Apply 2x then scale up with nearest neighbor to 3x
                // This is a temporary solution until native 3x is implemented
                {
                    auto temp = scale_xbr<InputImage, OutputImage>(input, 2);
                    return scaleNearest(temp, 1.5f);
                }
            case 4:
                // Apply 2x twice for 4x
                {
                    auto temp = scale_xbr<InputImage, OutputImage>(input, 2);
                    return scale_xbr<decltype(temp), OutputImage>(temp, 2);
                }
            default:
                throw std::logic_error("Invalid scale factor for xBR algorithm");
        }
    }

    // Simple nearest neighbor scaling (for any scale factor)
    template<typename AnyInput>
    static OutputImage scaleNearest(const AnyInput& input, float scale_factor) {
        auto out_width = static_cast<size_t>(input.width() * scale_factor);
        auto out_height = static_cast<size_t>(input.height() * scale_factor);

        OutputImage output(out_width, out_height, input);

        for (size_t y = 0; y < out_height; ++y) {
            auto src_y = static_cast<size_t>(y / scale_factor);
            for (size_t x = 0; x < out_width; ++x) {
                auto src_x = static_cast<size_t>(x / scale_factor);
                output.set_pixel(x, y, input.get_pixel(src_x, src_y));
            }
        }

        return output;
    }
};

// Convenience typedef for common use case
template<typename InputImage, typename OutputImage>
using Scaler = UnifiedScaler<InputImage, OutputImage>;

// Implementation of UnsupportedScaleException::makeMessage
// Must be defined after ScalerCapabilities
inline std::string UnsupportedScaleException::makeMessage(
    Algorithm algo, float requested, const std::vector<float>& supported) {

    std::stringstream ss;
    ss << ScalerCapabilities::getAlgorithmName(algo)
       << " algorithm doesn't support " << requested << "x scaling. ";

    if (supported.empty()) {
        ss << "This algorithm supports arbitrary scaling.";
    } else {
        ss << "Supported scales: ";
        for (size_t i = 0; i < supported.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << supported[i] << "x";
        }
    }
    return ss.str();
}

} // namespace scaler