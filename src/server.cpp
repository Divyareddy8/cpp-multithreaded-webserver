#include "server.h"
#include "http.h"
#include "logger.h"
#include "socket_guard.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ─── Static instance for signal handling ─────────────────────────────────
Server* Server::instance_ = nullptr;

// ─── Constructor / Destructor ─────────────────────────────────────────────
Server::Server(int port, int num_threads, const std::string& root_dir)
    : port_(port),
      server_fd_(-1),
      root_dir_(root_dir),
      running_(false),
      pool_(num_threads),
      cache_(32 * 1024 * 1024)   // 32 MB LRU cache
{
    instance_ = this;

    // Register signal handlers for graceful shutdown
    signal(SIGINT,  Server::signal_handler);
    signal(SIGTERM, Server::signal_handler);
    signal(SIGPIPE, SIG_IGN);   // ignore broken pipe

    LOG_INFO("Server initialised — port=" + std::to_string(port_) +
             " threads=" + std::to_string(num_threads) +
             " root=" + root_dir_);
}

Server::~Server() {
    stop();
}

// ─── Signal handler ────────────────────────────────────────────────────────
void Server::signal_handler(int sig) {
    if (instance_) {
        Logger::instance().info("\nCaught signal " + std::to_string(sig) +
                                " — initiating graceful shutdown …");
        instance_->stop();
    }
}

// ─── Socket setup ─────────────────────────────────────────────────────────
void Server::setup_socket() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));

    // Allow reuse of the address (avoids "Address already in use" on restart)
    int opt = 1;
    if (::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt() failed: " + std::string(strerror(errno)));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed: " + std::string(strerror(errno)));

    if (::listen(server_fd_, SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));

    LOG_INFO("Listening on 0.0.0.0:" + std::to_string(port_));
}

// ─── Start ────────────────────────────────────────────────────────────────
void Server::start() {
    setup_socket();
    running_.store(true);
    LOG_INFO("Server started — open http://localhost:" + std::to_string(port_) + "/");
    accept_loop();
}

// ─── Stop ─────────────────────────────────────────────────────────────────
void Server::stop() {
    if (!running_.exchange(false)) return;

    LOG_INFO("Stopping accept loop …");
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
    pool_.shutdown();
    LOG_INFO("Server stopped cleanly.");
}

// ─── Accept loop ─────────────────────────────────────────────────────────
void Server::accept_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = ::accept(server_fd_,
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &client_len);

        if (client_fd < 0) {
            if (!running_.load()) break;
            LOG_WARN("accept() returned error: " + std::string(strerror(errno)));
            continue;
        }

        // Set a receive/send timeout to guard against slow clients
        struct timeval tv{};
        tv.tv_sec  = 10;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        char ip_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
        LOG_DEBUG("Accepted connection from " + std::string(ip_buf));

        // Hand off to the thread pool — lambda captures fd by value
        pool_.enqueue([this, client_fd]() {
            handle_client(client_fd);
        });
    }
}

// ─── Handle one HTTP request on a client socket ───────────────────────────
void Server::handle_client(int client_fd) {
    SocketGuard guard(client_fd);   // RAII: closes fd when function exits

    // Read the request (up to 8KB)
    constexpr size_t BUF_SIZE = 8192;
    char buf[BUF_SIZE];
    ssize_t n = ::recv(client_fd, buf, BUF_SIZE - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    std::string raw(buf, static_cast<size_t>(n));
    HttpRequest req = HttpParser::parse(raw);

    if (!req.valid) {
        auto res = HttpParser::make_error(400);
        auto serialized = res.serialize();
        ::send(client_fd, serialized.c_str(), serialized.size(), MSG_NOSIGNAL);
        return;
    }

    LOG_INFO(req.method + " " + req.uri);

    // Only GET is supported
    if (req.method != "GET") {
        auto res = HttpParser::make_error(405);
        auto serialized = res.serialize();
        ::send(client_fd, serialized.c_str(), serialized.size(), MSG_NOSIGNAL);
        return;
    }

    // Resolve URI to a filesystem path (prevent directory traversal)
    std::string uri_path = req.uri;
    // Strip query string
    auto qpos = uri_path.find('?');
    if (qpos != std::string::npos) uri_path = uri_path.substr(0, qpos);

    // Normalize: default to index.html
    if (uri_path == "/" || uri_path.empty()) uri_path = "/index.html";

    // Guard against path traversal
    fs::path requested = fs::path(root_dir_) / uri_path.substr(1);
    fs::path canonical_root = fs::weakly_canonical(root_dir_);
    fs::path canonical_req;
    try {
        canonical_req = fs::weakly_canonical(requested);
    } catch (...) {
        auto res = HttpParser::make_error(400);
        auto s = res.serialize();
        ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
        return;
    }

    // Must be inside the root directory
    auto [root_end, req_end] = std::mismatch(
        canonical_root.begin(), canonical_root.end(), canonical_req.begin());
    if (root_end != canonical_root.end()) {
        auto res = HttpParser::make_error(403);
        auto s = res.serialize();
        ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
        return;
    }

    // ── Check LRU cache first ──
    auto cached = cache_.get(canonical_req.string());
    if (cached) {
        LOG_DEBUG("Cache HIT: " + canonical_req.string());
        auto res = HttpParser::make_response(200, cached->data, cached->content_type);
        auto s = res.serialize();
        ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
        return;
    }

    // ── Read from disk ──
    if (!fs::exists(canonical_req) || fs::is_directory(canonical_req)) {
        auto res = HttpParser::make_error(404);
        auto s = res.serialize();
        ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
        return;
    }

    std::ifstream file(canonical_req, std::ios::binary);
    if (!file) {
        auto res = HttpParser::make_error(500);
        auto s = res.serialize();
        ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    std::string mime = HttpParser::get_mime_type(canonical_req.string());

    // Store in cache
    cache_.put(canonical_req.string(), {content, mime});

    auto res = HttpParser::make_response(200, content, mime);
    auto s = res.serialize();
    ::send(client_fd, s.c_str(), s.size(), MSG_NOSIGNAL);
}
