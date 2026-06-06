#pragma once
#include <string>
#include <atomic>
#include "thread_pool.h"
#include "lru_cache.h"

class Server {
public:
    Server(int port, int num_threads, const std::string& root_dir);
    ~Server();

    void start();
    void stop();

private:
    int port_;
    int server_fd_;
    std::string root_dir_;
    std::atomic<bool> running_;
    ThreadPool pool_;
    LRUCache cache_;

    void setup_socket();
    void accept_loop();
    void handle_client(int client_fd);
    static void signal_handler(int sig);
    static Server* instance_;
};
