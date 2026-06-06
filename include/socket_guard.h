#pragma once
#include <unistd.h>

// RAII wrapper for socket file descriptors
// Automatically closes the socket when it goes out of scope
class SocketGuard {
public:
    explicit SocketGuard(int fd) : fd_(fd) {}
    ~SocketGuard() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    // Non-copyable
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    // Movable
    SocketGuard(SocketGuard&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    int get() const { return fd_; }

    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

private:
    int fd_;
};
