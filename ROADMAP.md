# Zircon Web Server Roadmap

> **Status: v1.2-dev** | Last revised: 2026-04-27
>
> This document reflects the **actual** state of the codebase and a realistic path forward.
> The previous roadmap was aspirational; this one is grounded in what exists.

---

## Current State: What We Actually Have

### Platform Abstraction Layer (COMPLETE)
```
include/platform/
├── detect.h       # OS/arch/compiler/endianness detection (10 platforms)
├── compat.h       # socket_t, mutex_t, nonblocking, reuseaddr, reuseport
├── event_loop.h   # Abstract event loop API (event_loop_t, event_callback_t)
├── sendfile.h     # Cross-platform zero-copy API
└── thread_pool.h  # SO_REUSEPORT multi-worker architecture

src/platform/
├── detect.c       # Runtime SIMD detection (SSE4.2/AVX2/AVX-512/NEON/SVE)
│                  # Runtime capability detection (io_uring, epoll, IOCP...)
├── event_epoll.c  # Linux epoll backend
├── event_kqueue.c # BSD/macOS kqueue backend
├── sendfile.c     # Linux/macOS/FreeBSD/Windows/fallback
├── thread_pool.c  # Per-worker event loops, CPU affinity, stats
└── thread.c       # CPU affinity (pthread/Mach/cpuset/Windows), naming
```

### Feature Matrix (Actual)

| Feature | Status | Notes |
|---------|--------|-------|
| HTTP/1.1 GET/HEAD | ✅ Done | Single-threaded async server |
| Keep-Alive | ✅ Done | Connection reuse, timeout check |
| event loop (epoll) | ✅ Done | Linux |
| event loop (kqueue) | ✅ Done | macOS, FreeBSD, OpenBSD, NetBSD |
| event loop (io_uring) | ❌ Missing | Only capability detection exists |
| event loop (IOCP) | ❌ Missing | Only capability detection exists |
| event loop (poll fallback) | ❌ Missing | Universal fallback |
| sendfile (Linux) | ✅ Done | `sendfile()` |
| sendfile (macOS) | ✅ Done | `sendfile()` BSD variant |
| sendfile (FreeBSD) | ✅ Done | `sendfile()` BSD variant |
| sendfile (Windows) | ✅ Done | `TransmitFile()` |
| sendfile (fallback) | ✅ Done | Userspace read/write loop |
| Thread pool | ✅ Done | SO_REUSEPORT, per-core event loops |
| CPU affinity | ✅ Done | Linux/macOS/FreeBSD/Windows |
| Rate limiting | ✅ Done | IP-based, mutex-protected, auto-unblock |
| Security headers | ✅ Done | CSP, HSTS, X-Frame-Options, etc. |
| MIME detection | ✅ Done | Extension + magic byte heuristic |
| ETag / caching | ✅ Done | Weak ETag, If-None-Match, Cache-Control |
| Path traversal prevention | ✅ Done | `realpath()` based |
| File type whitelist | ✅ Done | 20 extensions |
| Config file | ✅ Done | INI parser, CLI override |
| Graceful shutdown | ✅ Done | SIGTERM/SIGINT handler |
| Logger | ✅ Done | Levels, colors, file/console output |
| Multi-threaded HTTP pipeline | ✅ Done | Per-worker connection_handler, timeout checking |
| HTTPS (TLS) | ❌ Missing | Not started |
| HTTP/2 | ❌ Missing | Not started |
| eBPF/XDP | ❌ Missing | Not started |
| Directory listing | ❌ Missing | |
| Range requests | ❌ Missing | |

### Critical Gap — RESOLVED

The multi-threaded pipeline is now fully operational. Each worker thread lazily creates its own `connection_handler_t` with a dedicated `connections[]` array — no shared state, no locking needed. Timeout checking runs per-worker via the `on_periodic` callback invoked after each `event_loop_run_once` (1s interval).

---

## Phase 1: Production Readiness (v1.2 → v1.3)

**Goal**: A single reliable binary you can deploy. Multi-threaded mode works end-to-end.

### 1.1 Multi-Threaded HTTP Pipeline (P0) ✅ DONE

**Solution**: Extracted the connection state machine from `server_async.c` into `src/connection.c` as `connection_handler_t`. Each worker lazily creates its own handler instance via `on_worker_accept`, storing it in a worker-ID-indexed array. The shared `rate_limiter_t` (mutex-protected) is the only cross-worker resource.

**Completed tasks:**
- [x] Extract `connection_t`, state machine, `process_request()`, `send_error_response()` from `server_async.c` into `src/connection.c`
- [x] Thread-safe rate limiter (already mutex-protected, verified)
- [x] Wire `connection_handler_accept` into `main.c:on_worker_accept`
- [x] Per-worker connection tracking (each worker has its own `connection_handler_t` with dedicated `connections[]` array)
- [x] Per-worker timeout checking via `on_periodic` callback in thread_pool worker loop
- [x] Keep-Alive, security headers, sendfile, ETag all work in multi-threaded mode (shared connection.c code)
- [ ] Benchmark: single-threaded vs multi-threaded parity
### 1.2 Test Hardening

- [ ] Add integration test that actually starts server in multi-threaded mode and hits it with concurrent requests
- [ ] Verify all security headers appear in multi-threaded responses
- [ ] Test rate limiting under concurrent load
- [ ] Valgrind clean in both modes

### 1.3 Error Handling Audit

- [ ] Every `malloc` has error path (audit all call sites)
- [ ] File descriptor leak check (especially keep-alive edge cases)
- [ ] `strncpy` bounds review — replace with `snprintf`/`strlcpy` where appropriate

---

## Phase 2: Missing Backends (v1.4)

**Goal**: Every platform the detection layer knows about actually builds and runs.

### 2.1 io_uring Backend (Linux 5.1+)

- [ ] Implement `src/platform/event_iouring.c`
- [ ] Use `liburing` or raw syscall interface (prefer raw for zero-dependency)
- [ ] Benchmark vs epoll: target 2x throughput for small files
- [ ] Runtime fallback: io_uring → epoll → poll

### 2.2 IOCP Backend (Windows)

- [ ] Implement `src/platform/event_iocp.c`
- [ ] Windows build system (CMake or nmake Makefile)
- [ ] `TransmitFile` is already implemented; needs IOCP event integration
- [ ] CI: cross-compile with MinGW or test on Windows runner

### 2.3 Poll Fallback (Universal)

- [ ] Implement `src/platform/event_poll.c`
- [ ] Single-threaded `poll()` loop for platforms without epoll/kqueue
- [ ] This makes Zircon run on Solaris, older kernels, etc.

---

## Phase 3: Protocol & Security (v1.5 → v1.6)

### 3.1 TLS Support

- [ ] Integrate `libtls` (LibreSSL) or `mbedtls` (lightweight)
- [ ] SNI support
- [ ] Automatic redirect HTTP → HTTPS (configurable)
- [ ] HSTS header already exists; verify with real TLS

### 3.2 HTTP Range Requests

- [ ] Parse `Range: bytes=N-M` header
- [ ] 206 Partial Content with `Content-Range`
- [ ] Multi-range support (`bytes=0-1023,2048-4095`)

### 3.3 Directory Listings

- [ ] Generate HTML directory index
- [ ] Sorting by name/size/date
- [ ] Configurable on/off

---

## Phase 4: Performance (v1.7 → v1.8)

### 4.1 HTTP/2

- [ ] Integrate `nghttp2` library
- [ ] ALPN negotiation (requires TLS first)
- [ ] Server push for critical assets
- [ ] HPACK header compression

### 4.2 Memory-Mapped File Cache

- [ ] `mmap()` hot files, keep in LRU cache
- [ ] Eviction: file size + last access heuristic
- [ ] `madvise(MADV_SEQUENTIAL)` for streaming files, `MADV_RANDOM` for small
- [ ] Invalidate on file change (inotify/FSEvents/kqueue where available)

### 4.3 SIMD-Accelerated HTTP Parsing

- [ ] SSE4.2 string operations for header parsing
- [ ] AVX2 for MIME type lookup
- [ ] Runtime dispatch; fallback to scalar on older CPUs

---

## Phase 5: Advanced (v2.0+)

### 5.1 CGI / FastCGI / Reverse Proxy

- [ ] CGI gateway (fork + exec)
- [ ] FastCGI pass-through to backend
- [ ] Reverse proxy mode with upstream health checks

### 5.2 eBPF / XDP (Linux)

- [ ] XDP-based packet filtering for DDoS mitigation
- [ ] eBPF for connection tracking at kernel level

### 5.3 QUIC / HTTP/3

- [ ] Protocol-level groundwork (post-HTTP/2, requires TLS 1.3)

---

## Non-Goals (Explicitly Out of Scope)

- WebSocket support (use a reverse proxy for real-time)
- Dynamic scripting (PHP, Lua, etc.)
- Plugin system
- Configuration GUI
- Cloud-specific integrations

---

## Version History

| Version | Date | Highlights |
|---------|------|------------|
| **v1.2-dev** | Current | Platform abstraction, thread pool, sendfile across 4 OS |
| **v1.1.0** | 2025-03-30 | MIME detection, ETag, improved tests |
| **v1.0.0** | Initial | Basic HTTP server, rate limiting, path traversal prevention |
