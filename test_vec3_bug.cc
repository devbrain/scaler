#include <iostream>
#include <scaler/vec3.hh>

int main() {
    uvec3 a{10, 20, 30};
    uvec3 b{10, 20, 30};  // Same as a
    uvec3 c{10, 20, 31};  // Different from a
    
    std::cout << "Testing vec3 operator!= bug:\n";
    std::cout << "a = {" << a.x << ", " << a.y << ", " << a.z << "}\n";
    std::cout << "b = {" << b.x << ", " << b.y << ", " << b.z << "}\n";
    std::cout << "c = {" << c.x << ", " << c.y << ", " << c.z << "}\n\n";
    
    std::cout << "a == b: " << (a == b ? "true" : "false") << " (expected: true)\n";
    std::cout << "a != b: " << (a != b ? "true" : "false") << " (expected: false, but returns TRUE due to bug!)\n";
    std::cout << "a == c: " << (a == c ? "true" : "false") << " (expected: false)\n";
    std::cout << "a != c: " << (a != c ? "true" : "false") << " (expected: true, but returns FALSE due to bug!)\n";
    
    return 0;
}