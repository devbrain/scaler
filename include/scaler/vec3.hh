//
// Created by igor on 3/9/25.
//

#pragma once

#include <cmath>
#include <scaler/warning_macros.hh>

namespace scaler {
    template<typename T>
    struct vec3 {
        T x{0};
        T y{0};
        T z{0};

        using value_type = T;

        vec3() = default;

        vec3(T a, T b, T c)
            : x(a), y(b), z(c) {}

        template<typename U>
        vec3& operator =(vec3 <U>& other) {
            x = other.x;
            y = other.y;
            z = other.z;
            return *this;
        }

        template<typename U>
        vec3(const vec3 <U>& other)
            : x(static_cast<T>(other.x)),
              y(static_cast<T>(other.y)),
              z(static_cast<T>(other.z)) {
        }
    };

    template<typename T>
    inline bool operator ==(const vec3 <T>& a, const vec3 <T>& b) noexcept {
        // Short-circuit evaluation for early exit
        return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
    }

    template<typename T>
    inline bool operator !=(const vec3 <T>& a, const vec3 <T>& b) noexcept {
        // Short-circuit evaluation - if x differs, we know they're not equal
        return (a.x != b.x) || (a.y != b.y) || (a.z != b.z);
    }

    template<typename T>
    vec3<T> operator - (const vec3<T>& a, const vec3<T>& b) {
        return {
            static_cast<T>(a.x - b.x),
            static_cast<T>(a.y - b.y),
            static_cast<T>(a.z - b.z)
        };
    }

    template<typename T>
    vec3<T> operator + (const vec3<T>& a, const vec3<T>& b) {
        return {
            static_cast<T>(a.x + b.x),
            static_cast<T>(a.y + b.y),
            static_cast<T>(a.z + b.z)
        };
    }

    template<typename T, typename S>
    vec3<T> operator * (const vec3<T>& v, S scalar) {
        SCALER_DISABLE_WARNING_PUSH
        SCALER_DISABLE_WARNING_CONVERSION
        return {
            static_cast<T>(v.x * scalar),
            static_cast<T>(v.y * scalar),
            static_cast<T>(v.z * scalar)
        };
        SCALER_DISABLE_WARNING_POP
    }

    template<typename T, typename S>
    vec3<T> operator * (S scalar, const vec3<T>& v) {
        return v * scalar;
    }

    template<typename T>
    vec3<T> abs(const vec3<T>& a) {
        return {std::abs(a.x), std::abs(a.y), std::abs(a.z)};
    }

    using uvec3 = vec3 <unsigned int>;
    using ivec3 = vec3 <int>;

    template<typename T, typename U>
    inline T mix(T const& x, T const& y, U const& a) noexcept {
        // Optimize for common cases
        if (a == static_cast<U>(0)) return x;
        if (a == static_cast<U>(1)) return y;
        return static_cast <T>(static_cast <U>(x) * (static_cast <U>(1) - a) + static_cast <U>(y) * a);
    }

    template<typename T, typename U>
    inline vec3 <T> mix(vec3 <T> const& x, vec3 <T> const& y, U const& a) noexcept {
        // Optimize for common cases
        SCALER_DISABLE_WARNING_PUSH
        SCALER_DISABLE_WARNING_FLOAT_EQUAL
        if (a == static_cast<U>(0)) return x;
        if (a == static_cast<U>(1)) return y;
        SCALER_DISABLE_WARNING_POP
        // Inline the scalar mix to avoid function call overhead
        return {
            static_cast<T>(static_cast<U>(x.x) * (static_cast<U>(1) - a) + static_cast<U>(y.x) * a),
            static_cast<T>(static_cast<U>(x.y) * (static_cast<U>(1) - a) + static_cast<U>(y.y) * a),
            static_cast<T>(static_cast<U>(x.z) * (static_cast<U>(1) - a) + static_cast<U>(y.z) * a)
        };
    }
}
