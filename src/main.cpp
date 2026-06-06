#include "server.h"
#include "logger.h"

#include <iostream>
#include <string>
#include <thread>
#include <stdexcept>
#include <cstdlib>

static void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -p <port>     Port to listen on          (default: 8080)\n"
        "  -t <threads>  Worker thread count         (default: CPU cores)\n"
        "  -r <dir>      Web root directory          (default: ./www)\n"
        "  -d            Enable debug logging\n"
        "  -h            Print this help\n"
        "\n"
        "Example:\n"
        "  " << prog << " -p 8080 -t 4 -r ./www\n";
}

int main(int argc, char* argv[]) {
    int port = 8080;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 4;
    std::string root_dir = "./www";
    bool debug = false;

    // Simple argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            threads = std::stoi(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            root_dir = argv[++i];
        } else if (arg == "-d") {
            debug = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (debug) Logger::instance().set_level(LogLevel::DEBUG);

    std::cout <<
        "\n"
        "  ╔═══════════════════════════════════════╗\n"
        "  ║     C++ Multi-Threaded Web Server     ║\n"
        "  ║           Phase 1-5 Complete          ║\n"
        "  ╚═══════════════════════════════════════╝\n\n";

    try {
        Server server(port, threads, root_dir);
        server.start();
    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
