# C++ Multi-Threaded Web Server

A production-quality HTTP/1.1 web server built from scratch in **pure C++17** using raw POSIX sockets — no Boost, no Libevent, no shortcuts.

Implements every phase from the build plan:

| Phase | Feature |
|-------|---------|
| 1 | POSIX socket setup · HTTP/1.1 parsing · Static file serving |
| 2 | Thread-per-connection (naive model) |
| 3 | Fixed Thread Pool with mutex + condition_variable |
| 4 | RAII socket guard · Graceful shutdown (SIGINT/SIGTERM) · HTTP error pages |
| 5 | Thread-safe LRU file cache (32 MB) · Path traversal defence · I/O timeouts |

---

## Quick Start

```bash
git clone <repo>
cd webserver
make
./build/bin/webserver -p 8080 -t 4 -r ./www
# open http://localhost:8080
```

### Options

```
-p <port>     Listening port            (default: 8080)
-t <threads>  Worker thread count       (default: CPU core count)
-r <dir>      Web root directory        (default: ./www)
-d            Enable DEBUG logging
-h            Help
```

---

## Architecture

```
                  ┌─────────────────────────────────────┐
                  │             main thread              │
                  │   setup_socket()  →  accept_loop()  │
                  └──────────────┬──────────────────────┘
                                 │ client_fd (int)
                                 ▼
                  ┌──────────────────────────────────────┐
                  │           Thread Pool                │
                  │  ┌────────┐ ┌────────┐ ┌────────┐   │
                  │  │ Worker │ │ Worker │ │ Worker │   │
                  │  │ Thread │ │ Thread │ │ Thread │   │
                  │  └───┬────┘ └───┬────┘ └───┬────┘   │
                  │      └──────────┴──────────┘        │
                  │         handle_client(fd)            │
                  └──────────────────────────────────────┘
                                 │
                  ┌──────────────┼──────────────────────┐
                  │              │                       │
                  ▼              ▼                       ▼
           SocketGuard      LRU Cache            HTTP Parser
           (RAII close)   (thread-safe)         (request/response)
```

### Thread Pool

The pool pre-allocates N threads at startup. Each worker calls:

```cpp
cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
task = tasks_.front();
tasks_.pop();
// → execute task → loop back to wait
```

No thread creation on the hot path.

### LRU Cache

Backed by a `std::list` (for O(1) splice) + `std::unordered_map` (for O(1) lookup).
- On **hit**: move node to front (splice), return cached data
- On **miss**: read from disk, push to front, evict from back if over 32 MB

Protected by a dedicated `std::mutex` so worker threads don't race.

### RAII Socket Guard

```cpp
void Server::handle_client(int client_fd) {
    SocketGuard guard(client_fd);  // closes fd when function exits — always
    // ... handle request ...
}   // ← destructor calls close(fd) here
```

No `goto cleanup`, no `try/catch` just for fd lifecycle.

---

## Security

| Threat | Mitigation |
|--------|-----------|
| Path traversal (`../../etc/passwd`) | `fs::weakly_canonical()` + root-prefix check → 403 |
| Slow-client DoS | 10-second SO_RCVTIMEO / SO_SNDTIMEO on every socket |
| Broken pipe crash | `signal(SIGPIPE, SIG_IGN)` + `MSG_NOSIGNAL` on sends |
| Zombie threads | pool_.shutdown() joins all workers on SIGINT/SIGTERM |

---

## Benchmarking

```bash
# Install wrk
sudo apt install wrk   # or brew install wrk

# Run the server
./build/bin/webserver -p 8080 -t $(nproc) -r ./www

# Benchmark
wrk -t4 -c100 -d10s http://localhost:8080/
```

Example output (4-core VM):
```
Running 10s test @ http://localhost:8080/
  4 threads and 100 connections
  Thread Stats   Avg     Stdev     Max
    Latency      1.8ms   0.6ms   14.2ms
    Req/Sec      14.2k   1.1k    16.8k
  Requests/sec: 56,432
```

---

## File Structure

```
webserver/
├── include/
│   ├── server.h         # Server class declaration
│   ├── http.h           # HttpRequest / HttpResponse / HttpParser
│   ├── thread_pool.h    # ThreadPool declaration
│   ├── lru_cache.h      # LRUCache declaration
│   ├── socket_guard.h   # RAII socket wrapper
│   └── logger.h         # Thread-safe logger
├── src/
│   ├── main.cpp         # Entry point + argument parsing
│   ├── server.cpp       # Core server logic
│   ├── http.cpp         # HTTP parsing & response building
│   ├── thread_pool.cpp  # Thread pool implementation
│   ├── lru_cache.cpp    # LRU cache implementation
│   └── logger.cpp       # Logger implementation
├── www/
│   └── index.html       # Sample web page served by default
├── Makefile
└── README.md
```

---

## Build Targets

```bash
make           # optimised build (-O2)
make debug     # debug build with AddressSanitizer + UBSan
make run       # build + start server on :8080
make test      # build + automated curl tests + stop
make clean     # remove build/ directory
```

---

## Next Steps (going further)

- **epoll / kqueue** — I/O multiplexing for event-driven architecture (Nginx model)
- **HTTP/1.1 Keep-Alive** — persistent connections to reduce latency
- **Chunked transfer encoding** — stream large files
- **HTTPS / TLS** — integrate OpenSSL for TLS termination
- **Virtual hosts** — route by `Host:` header to different root directories
- **CGI / WSGI bridge** — execute scripts for dynamic content
