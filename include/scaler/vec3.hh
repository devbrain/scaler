//
// Created by igor on 3/9/25.
//

#pragma once

#include <cmath>

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
bool operator ==(const vec3 <T>& a, const vec3 <T>& b) {
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

template<typename T>
bool operator !=(const vec3 <T>& a, const vec3 <T>& b) {
    return !((a.x == b.x) && (a.y == b.y) && (a.z == b.z));  // Fixed: Now correctly returns true when vectors are different
}

template<typename T>
vec3<T> operator - (const vec3<T>& a, const vec3<T>& b) {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}

template<typename T>
vec3<T> abs(const vec3<T>& a) {
    return {std::abs(a.x), std::abs(a.y), std::abs(a.z)};
}

using uvec3 = vec3 <unsigned int>;
using ivec3 = vec3 <int>;

template<typename T, typename U>
T mix(T const& x, T const& y, U const& a) {
    return static_cast <T>(static_cast <U>(x) * (static_cast <U>(1) - a) + static_cast <U>(y) * a);
}

template<typename T, typename U>
vec3 <T> mix(vec3 <T> const& x, vec3 <T> const& y, U const& a) {
    return {mix(x.x, y.x, a), mix(x.y, y.y, a), mix(x.z, y.z, a)};
}


