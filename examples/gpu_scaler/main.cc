#include "app.hh"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        gpu_scaler_app::App app;

        if (!app.init()) {
            std::cerr << "Failed to initialize application\n";
            return 1;
        }

        app.run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}