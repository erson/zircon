# Zircon Web Server Roadmap

## Vision
Transform Zircon from an educational project into a high-performance, production-grade web server capable of handling millions of concurrent connections with minimal latency — **on any platform**.

---

## Cross-Platform Architecture

### Supported Platforms

| Platform | Versions | Architecture |
|----------|----------|--------------|
| **Linux** | 4.x+ (basic), 5.1+ (io_uring), 5.10+ (XDP) | x86_64, ARM64 |
| **macOS** | 10.14+ (Mojave) | x86_64, ARM64 (Apple Silicon) |
| **FreeBSD** | 12.0+ | x86_64, ARM64 |
| **OpenBSD** | 6.7+ | x86_64 |
| **NetBSD** | 9.0+ | x86_64 |
| **Windows** | 10 1903+ (via IOCP) | x86_64 |

### Platform Detection Strategy

```c
// include/platform/detect.h

#if defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macOS"
#elif defined(__FreeBSD__)
    #define PLATFORM_FREEBSD 1
    #define PLATFORM_NAME "FreeBSD"
#elif defined(__OpenBSD__)
    #define PLATFORM_OPENBSD 1
    #define PLATFORM_NAME "OpenBSD"
#elif defined(__NetBSD__)
    #define PLATFORM_NETBSD 1
    #define PLATFORM_NAME "NetBSD"
#elif defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "Windows"
#else
    #define PLATFORM_UNKNOWN 1
    #define PLATFORM_NAME "Unknown"
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X86_64 1
    #define ARCH_NAME "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64 1
    #define ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
    #define ARCH_ARM32 1
    #define ARCH_NAME "ARM32"
#else
    #define ARCH_UNKNOWN 1
    #define ARCH_NAME "Unknown"
#endif
```

### Feature Capability Matrix

| Feature | Linux | macOS | FreeBSD | OpenBSD | Windows |
|---------|-------|-------|---------|---------|---------|
| io_uring | ✅ 5.1+ | ❌ | ❌ | ❌ | ❌ |
| epoll | ✅ | ❌ | ❌ | ❌ | ❌ |
| kqueue | ❌ | ✅ | ✅ | ✅ | ❌ |
| IOCP | ❌ | ❌ | ❌ | ❌ | ✅ |
| sendfile | ✅ | ✅ | ✅ | ❌ | ✅ (TransmitFile) |
| SO_REUSEPORT | ✅ | ✅ | ✅ | ✅ | ❌ |
| AF_XDP | ✅ 5.10+ | ❌ | ❌ | ❌ | ❌ |
| eBPF | ✅ 4.x+ | ❌ | ❌ | ❌ | ❌ |
| CPU affinity | ✅ | ✅ | ✅ | ❌ | ✅ |
| AVX2 | ✅ | ✅ | ✅ | ✅ | ✅ |
| NEON | ✅ ARM | ✅ ARM | ✅ ARM | ❌ | ✅ ARM |

---

## Phase 1: Foundation Optimization (v0.2.x)
**Timeline: 2-3 months | Complexity: Medium**

### 1.1 Event-Driven I/O Abstraction Layer

**Unified Interface:**
```c
// include/platform/event_loop.h

typedef enum {
    EVENT_READ   = 1 << 0,
    EVENT_WRITE  = 1 << 1,
    EVENT_ERROR  = 1 << 2,
    EVENT_HANGUP = 1 << 3
} event_flags_t;

typedef struct event_loop event_loop_t;
typedef void (*event_callback_t)(int fd, event_flags_t events, void *userdata);

// Platform-agnostic API
event_loop_t *event_loop_create(void);
void event_loop_destroy(event_loop_t *loop);
int event_loop_add(event_loop_t *loop, int fd, event_flags_t events, 
                   event_callback_t cb, void *userdata);
int event_loop_modify(event_loop_t *loop, int fd, event_flags_t events);
int event_loop_remove(event_loop_t *loop, int fd);
int event_loop_run(event_loop_t *loop, int timeout_ms);
const char *event_loop_backend_name(event_loop_t *loop);
```

**Platform Implementations:**

| Platform | Primary Backend | Fallback |
|----------|-----------------|----------|
| Linux 5.1+ | io_uring | epoll |
| Linux <5.1 | epoll | poll |
| macOS | kqueue | poll |
| FreeBSD | kqueue | poll |
| OpenBSD | kqueue | poll |
| NetBSD | kqueue | poll |
| Windows | IOCP | select |

**Implementation Files:**
```
src/platform/
├── event_loop.c          # Dispatcher + runtime detection
├── event_iouring.c       # Linux io_uring backend
├── event_epoll.c         # Linux epoll backend
├── event_kqueue.c        # BSD/macOS kqueue backend
├── event_iocp.c          # Windows IOCP backend
└── event_poll.c          # Universal fallback (POSIX poll/select)
```

**Tasks:**
- [ ] Define abstract event_loop_t interface
- [ ] Implement io_uring backend (Linux 5.1+)
- [ ] Implement epoll backend (Linux)
- [ ] Implement kqueue backend (macOS/BSD)
- [ ] Implement IOCP backend (Windows)
- [ ] Implement poll/select fallback (universal)
- [ ] Runtime capability detection
- [ ] Benchmark: target 100K concurrent connections

---

### 1.2 Cross-Platform Thread Pooling

**Unified Threading Interface:**
```c
// include/platform/thread.h

typedef struct thread_pool thread_pool_t;
typedef void (*task_fn_t)(void *arg);

thread_pool_t *thread_pool_create(unsigned int num_workers);
void thread_pool_destroy(thread_pool_t *pool);
int thread_pool_submit(thread_pool_t *pool, task_fn_t fn, void *arg);
unsigned int thread_pool_optimal_size(void);

// CPU affinity (where supported)
int thread_set_affinity(pthread_t thread, unsigned int cpu);
int thread_get_cpu_count(void);
```

**Platform-Specific CPU Affinity:**

```c
// src/platform/thread_affinity.c

int thread_set_affinity(pthread_t thread, unsigned int cpu) {
#if defined(PLATFORM_LINUX)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    
#elif defined(PLATFORM_MACOS)
    thread_affinity_policy_data_t policy = { cpu };
    thread_port_t mach_thread = pthread_mach_thread_np(thread);
    return thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                            (thread_policy_t)&policy, 1);
                            
#elif defined(PLATFORM_FREEBSD)
    cpuset_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    
#elif defined(PLATFORM_WINDOWS)
    HANDLE h = pthread_gethandle(thread);
    return SetThreadAffinityMask(h, 1ULL << cpu) ? 0 : -1;
    
#else
    (void)thread; (void)cpu;
    return -1;  // Not supported
#endif
}
```

**Tasks:**
- [ ] Detect CPU count per platform
- [ ] Implement CPU affinity per platform
- [ ] Lock-free work queue (MPMC)
- [ ] SO_REUSEPORT for load balancing (where supported)
- [ ] Per-worker statistics

---

### 1.3 Cross-Platform Zero-Copy File Serving

**Unified sendfile Interface:**
```c
// include/platform/sendfile.h

typedef enum {
    SENDFILE_OK,
    SENDFILE_EAGAIN,
    SENDFILE_ERROR,
    SENDFILE_UNSUPPORTED
} sendfile_result_t;

sendfile_result_t platform_sendfile(int out_fd, int in_fd, 
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent);
bool platform_has_sendfile(void);
```

**Platform Implementations:**

```c
// src/platform/sendfile.c

sendfile_result_t platform_sendfile(int out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
#if defined(PLATFORM_LINUX)
    ssize_t ret = sendfile(out_fd, in_fd, offset, count);
    if (ret >= 0) {
        *bytes_sent = ret;
        return SENDFILE_OK;
    }
    return (errno == EAGAIN) ? SENDFILE_EAGAIN : SENDFILE_ERROR;

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_FREEBSD)
    off_t len = count;
    int ret = sendfile(in_fd, out_fd, *offset, &len, NULL, 0);
    *bytes_sent = len;
    if (ret == 0 || (ret == -1 && errno == EAGAIN)) {
        *offset += len;
        return (ret == 0) ? SENDFILE_OK : SENDFILE_EAGAIN;
    }
    return SENDFILE_ERROR;

#elif defined(PLATFORM_WINDOWS)
    HANDLE hFile = (HANDLE)_get_osfhandle(in_fd);
    SOCKET sock = (SOCKET)_get_osfhandle(out_fd);
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(*offset);
    ov.OffsetHigh = (DWORD)(*offset >> 32);
    if (TransmitFile(sock, hFile, count, 0, &ov, NULL, 0)) {
        *bytes_sent = count;
        *offset += count;
        return SENDFILE_OK;
    }
    return SENDFILE_ERROR;

#else
    // Fallback: userspace copy
    return sendfile_fallback(out_fd, in_fd, offset, count, bytes_sent);
#endif
}
```

**Memory Mapping:**
```c
// include/platform/mmap.h

void *platform_mmap(int fd, size_t length, off_t offset);
void platform_munmap(void *addr, size_t length);
void platform_madvise_sequential(void *addr, size_t length);
void platform_madvise_random(void *addr, size_t length);
```

**Tasks:**
- [ ] Implement sendfile for Linux, macOS, FreeBSD, Windows
- [ ] Implement userspace fallback for OpenBSD/NetBSD
- [ ] Cross-platform mmap wrapper
- [ ] LRU cache with inotify (Linux), FSEvents (macOS), kqueue (BSD)

---

## Phase 2: Protocol Evolution (v0.3.x)
**Timeline: 3-4 months | Complexity: High**

### 2.1 HTTP/2 Support (Cross-Platform)

nghttp2 is already cross-platform. Integration points:

**Tasks:**
- [ ] Integrate nghttp2 (works on all platforms)
- [ ] HPACK header compression
- [ ] Stream multiplexing
- [ ] Platform-specific TLS integration:
  - Linux: OpenSSL / BoringSSL
  - macOS: SecureTransport or OpenSSL
  - Windows: Schannel or OpenSSL
  - BSD: OpenSSL

---

### 2.2 HTTP/3 (QUIC) Support

**Cross-Platform QUIC Libraries:**

| Library | Platforms | Notes |
|---------|-----------|-------|
| quiche (Cloudflare) | All | Rust, C bindings |
| msquic (Microsoft) | All | C, best Windows support |
| ngtcp2 | All | C, pairs with nghttp3 |
| picoquic | All | C, lightweight |

**Recommended: ngtcp2 + nghttp3**
- Pure C, minimal dependencies
- Works on all target platforms
- Active development

**Tasks:**
- [ ] Integrate ngtcp2 for QUIC transport
- [ ] Integrate nghttp3 for HTTP/3 framing
- [ ] Cross-platform UDP socket handling
- [ ] Connection migration support

---

## Phase 3: Performance Acceleration (v0.4.x)
**Timeline: 4-6 months | Complexity: Very High**

### 3.1 Cross-Platform SIMD HTTP Parsing

**Architecture-Specific SIMD:**

| Architecture | Instruction Sets | Vector Width |
|--------------|------------------|--------------|
| x86_64 | SSE4.2, AVX2, AVX-512 | 128/256/512 bit |
| ARM64 | NEON, SVE | 128/variable |
| ARM32 | NEON | 128 bit |

**Runtime Detection:**
```c
// include/platform/simd.h

typedef enum {
    SIMD_NONE    = 0,
    SIMD_SSE42   = 1 << 0,   // x86
    SIMD_AVX2    = 1 << 1,   // x86
    SIMD_AVX512  = 1 << 2,   // x86
    SIMD_NEON    = 1 << 3,   // ARM
    SIMD_SVE     = 1 << 4,   // ARM64
} simd_caps_t;

simd_caps_t simd_detect_capabilities(void);
const char *simd_caps_to_string(simd_caps_t caps);
```

**x86_64 Detection:**
```c
// src/platform/simd_x86.c

#if defined(ARCH_X86_64)
#include <cpuid.h>

simd_caps_t simd_detect_capabilities(void) {
    simd_caps_t caps = SIMD_NONE;
    unsigned int eax, ebx, ecx, edx;
    
    // Check SSE4.2
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & (1 << 20)) caps |= SIMD_SSE42;
    }
    
    // Check AVX2
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 5)) caps |= SIMD_AVX2;
        if (ebx & (1 << 16)) caps |= SIMD_AVX512;
    }
    
    return caps;
}
#endif
```

**ARM64 Detection:**
```c
// src/platform/simd_arm.c

#if defined(ARCH_ARM64)
#if defined(PLATFORM_LINUX)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#elif defined(PLATFORM_MACOS)
#include <sys/sysctl.h>
#endif

simd_caps_t simd_detect_capabilities(void) {
    simd_caps_t caps = SIMD_NONE;
    
#if defined(PLATFORM_LINUX)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_ASIMD) caps |= SIMD_NEON;
    if (hwcap & HWCAP_SVE) caps |= SIMD_SVE;
    
#elif defined(PLATFORM_MACOS)
    // Apple Silicon always has NEON
    caps |= SIMD_NEON;
#endif
    
    return caps;
}
#endif
```

**Function Dispatch:**
```c
// src/http/parser_simd.c

typedef size_t (*parse_headers_fn)(const char *buf, size_t len, http_header_t *out);

static parse_headers_fn select_parser(void) {
    simd_caps_t caps = simd_detect_capabilities();
    
#if defined(ARCH_X86_64)
    if (caps & SIMD_AVX512) return parse_headers_avx512;
    if (caps & SIMD_AVX2)   return parse_headers_avx2;
    if (caps & SIMD_SSE42)  return parse_headers_sse42;
#elif defined(ARCH_ARM64)
    if (caps & SIMD_SVE)    return parse_headers_sve;
    if (caps & SIMD_NEON)   return parse_headers_neon;
#endif
    
    return parse_headers_scalar;  // Fallback
}
```

**Tasks:**
- [ ] Runtime SIMD detection (x86_64, ARM64)
- [ ] SSE4.2 HTTP parser
- [ ] AVX2 HTTP parser
- [ ] NEON HTTP parser (ARM)
- [ ] Scalar fallback
- [ ] Benchmark suite

---

### 3.2 JIT-Compiled Routing (Cross-Platform)

**Cross-Platform JIT Options:**

| Library | Platforms | Complexity |
|---------|-----------|------------|
| DynASM | All (Lua required) | Medium |
| LLVM ORC JIT | All | High |
| GNU Lightning | Linux, BSD, macOS | Low |
| AsmJit | x86/x64 only | Medium |
| MIR | All | Medium |

**Recommended: MIR (Medium-level IR)**
- Pure C, no dependencies
- Supports x86_64, ARM64, ARM32
- Simpler than LLVM

**Alternative: Interpret-first, JIT later**
```c
// Start with interpreted routing
// JIT compile hot routes after N hits

typedef struct {
    char *pattern;
    route_handler_t handler;
    uint64_t hit_count;
    void *jit_code;  // NULL until JIT compiled
} route_t;
```

**Tasks:**
- [ ] Interpreted route matcher (baseline)
- [ ] DFA construction from patterns
- [ ] JIT backend for x86_64
- [ ] JIT backend for ARM64
- [ ] Hot-route detection and compilation

---

## Phase 4: Kernel Bypass (v0.5.x)
**Timeline: 6-8 months | Complexity: Expert**

### 4.1 Platform-Specific High-Performance Networking

**Linux: AF_XDP**
```c
// src/platform/net_xdp.c (Linux only)
#if defined(PLATFORM_LINUX)

#include <linux/if_xdp.h>
#include <bpf/xsk.h>

typedef struct {
    struct xsk_socket *xsk;
    struct xsk_umem *umem;
    void *buffer;
    // ...
} xdp_socket_t;

#endif
```

**macOS: Network.framework (for userspace)**
- No kernel bypass available
- Use dispatch_source for high-performance async I/O
- Consider netmap for FreeBSD VM testing

**Windows: Registered I/O (RIO)**
```c
// src/platform/net_rio.c (Windows only)
#if defined(PLATFORM_WINDOWS)

#include <mswsock.h>

typedef struct {
    RIO_CQ cq;
    RIO_RQ rq;
    PRIO_BUF buffers;
    // ...
} rio_socket_t;

#endif
```

**Abstraction Layer:**
```c
// include/platform/net_accel.h

typedef enum {
    NET_ACCEL_NONE,
    NET_ACCEL_XDP,      // Linux
    NET_ACCEL_DPDK,     // Linux
    NET_ACCEL_NETMAP,   // FreeBSD
    NET_ACCEL_RIO,      // Windows
} net_accel_type_t;

typedef struct net_accel net_accel_t;

net_accel_t *net_accel_create(net_accel_type_t preferred);
net_accel_type_t net_accel_get_type(net_accel_t *accel);
bool net_accel_available(net_accel_type_t type);
```

**Platform Availability:**

| Technology | Linux | macOS | FreeBSD | Windows |
|------------|-------|-------|---------|---------|
| AF_XDP | ✅ 5.10+ | ❌ | ❌ | ❌ |
| DPDK | ✅ | ❌ | ✅ | ❌ |
| netmap | ✅ | ❌ | ✅ | ❌ |
| RIO | ❌ | ❌ | ❌ | ✅ 8+ |
| Fallback | kqueue/epoll/IOCP | kqueue | kqueue | IOCP |

**Tasks:**
- [ ] AF_XDP backend (Linux)
- [ ] DPDK backend (Linux, FreeBSD)
- [ ] netmap backend (FreeBSD)
- [ ] RIO backend (Windows)
- [ ] Graceful fallback chain

---

### 4.2 eBPF/XDP Request Filtering (Linux Only)

```c
// src/platform/bpf_filter.c

#if defined(PLATFORM_LINUX) && defined(HAVE_LIBBPF)

#include <bpf/libbpf.h>

typedef struct {
    struct bpf_object *obj;
    int prog_fd;
    int map_fd;  // For blocklist, rate limits
} bpf_filter_t;

bpf_filter_t *bpf_filter_load(const char *prog_path);
int bpf_filter_attach(bpf_filter_t *filter, const char *ifname);
int bpf_filter_update_blocklist(bpf_filter_t *filter, uint32_t ip, bool block);

#else

// Stub implementation for non-Linux platforms
typedef struct { int dummy; } bpf_filter_t;

static inline bpf_filter_t *bpf_filter_load(const char *p) { 
    (void)p; return NULL; 
}

#endif
```

**Cross-Platform Filtering Fallback:**
- Linux: eBPF/XDP (kernel)
- Others: Application-level filtering with optimized data structures (bloom filter, hash tables)

---

## Phase 5: Distributed Operations (v1.0.x)
**Timeline: 6-12 months | Complexity: Expert**

### 5.1 Raft Consensus (Cross-Platform)

Raft is pure algorithm — fully cross-platform.

**Networking Layer Abstraction:**
```c
// Use platform event loop for Raft RPC
// TCP or UDP transport (platform-agnostic)
```

**Tasks:**
- [ ] Raft state machine (pure C, no platform deps)
- [ ] Log storage (file-based, uses platform file APIs)
- [ ] Network transport (uses platform socket abstraction)
- [ ] Cluster membership

---

### 5.2 Zero-Downtime Hot-Patching

**Platform-Specific FD Passing:**

```c
// include/platform/fd_passing.h

// Unix (Linux, macOS, BSD): SCM_RIGHTS over Unix socket
// Windows: DuplicateHandle + named pipe

int platform_send_fd(int unix_sock, int fd_to_send);
int platform_recv_fd(int unix_sock);
```

**Implementation:**
```c
// src/platform/fd_passing_unix.c
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) || defined(PLATFORM_FREEBSD)

int platform_send_fd(int unix_sock, int fd_to_send) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));
    
    return sendmsg(unix_sock, &msg, 0);
}

#endif

// src/platform/fd_passing_win.c
#if defined(PLATFORM_WINDOWS)

int platform_send_fd(HANDLE pipe, HANDLE handle_to_send) {
    DWORD pid;
    GetNamedPipeClientProcessId(pipe, &pid);
    HANDLE target_proc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    HANDLE dup_handle;
    DuplicateHandle(GetCurrentProcess(), handle_to_send,
                   target_proc, &dup_handle, 0, FALSE, 
                   DUPLICATE_SAME_ACCESS);
    // Send dup_handle value over pipe
    WriteFile(pipe, &dup_handle, sizeof(dup_handle), NULL, NULL);
    CloseHandle(target_proc);
    return 0;
}

#endif
```

**Tasks:**
- [ ] Unix domain socket FD passing (Unix-like)
- [ ] Named pipe + DuplicateHandle (Windows)
- [ ] Connection state serialization
- [ ] Graceful shutdown with drain

---

## Directory Structure

```
zircon/
├── include/
│   ├── platform/
│   │   ├── detect.h          # Platform/arch detection macros
│   │   ├── event_loop.h      # Abstract event loop API
│   │   ├── thread.h          # Threading abstraction
│   │   ├── sendfile.h        # Zero-copy file API
│   │   ├── mmap.h            # Memory mapping API
│   │   ├── simd.h            # SIMD capability detection
│   │   ├── net_accel.h       # Accelerated networking API
│   │   └── fd_passing.h      # FD transfer API
│   ├── http/
│   │   ├── parser.h
│   │   ├── h2.h
│   │   └── h3.h
│   └── ...
├── src/
│   ├── platform/
│   │   ├── detect.c          # Runtime detection
│   │   ├── event_iouring.c   # Linux io_uring
│   │   ├── event_epoll.c     # Linux epoll
│   │   ├── event_kqueue.c    # BSD/macOS kqueue
│   │   ├── event_iocp.c      # Windows IOCP
│   │   ├── event_poll.c      # Universal fallback
│   │   ├── thread_linux.c
│   │   ├── thread_macos.c
│   │   ├── thread_bsd.c
│   │   ├── thread_windows.c
│   │   ├── sendfile.c        # Platform dispatcher
│   │   ├── simd_x86.c
│   │   ├── simd_arm.c
│   │   ├── net_xdp.c         # Linux AF_XDP
│   │   ├── net_rio.c         # Windows RIO
│   │   └── ...
│   ├── http/
│   │   ├── parser.c
│   │   ├── parser_sse42.c
│   │   ├── parser_avx2.c
│   │   ├── parser_neon.c
│   │   └── parser_scalar.c
│   └── ...
└── CMakeLists.txt            # Cross-platform build
```

---

## Build System

**CMake for Cross-Platform:**
```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(zircon C)

# Detect platform
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_LINUX TRUE)
    add_definitions(-DPLATFORM_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_MACOS TRUE)
    add_definitions(-DPLATFORM_MACOS=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(PLATFORM_FREEBSD TRUE)
    add_definitions(-DPLATFORM_FREEBSD=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_WINDOWS TRUE)
    add_definitions(-DPLATFORM_WINDOWS=1)
endif()

# Detect io_uring support (Linux)
if(PLATFORM_LINUX)
    find_package(PkgConfig)
    pkg_check_modules(URING liburing)
    if(URING_FOUND)
        add_definitions(-DHAVE_IO_URING=1)
    endif()
endif()

# Detect SIMD capabilities
include(CheckCCompilerFlag)
check_c_compiler_flag("-mavx2" HAVE_AVX2)
check_c_compiler_flag("-msse4.2" HAVE_SSE42)

# Platform-specific sources
if(PLATFORM_LINUX)
    list(APPEND PLATFORM_SOURCES 
        src/platform/event_epoll.c
        src/platform/event_iouring.c
        src/platform/net_xdp.c)
elseif(PLATFORM_MACOS OR PLATFORM_FREEBSD)
    list(APPEND PLATFORM_SOURCES 
        src/platform/event_kqueue.c)
elseif(PLATFORM_WINDOWS)
    list(APPEND PLATFORM_SOURCES 
        src/platform/event_iocp.c
        src/platform/net_rio.c)
endif()
```

---

## Implementation Priority (Cross-Platform)

| Feature | P0 (All) | Linux | macOS | BSD | Windows |
|---------|----------|-------|-------|-----|---------|
| Event loop abstraction | ✅ | io_uring | kqueue | kqueue | IOCP |
| Thread pool | ✅ | ✅ | ✅ | ✅ | ✅ |
| sendfile | ✅ | ✅ | ✅ | ✅ | TransmitFile |
| HTTP/2 | ✅ | ✅ | ✅ | ✅ | ✅ |
| HTTP/3 | ✅ | ✅ | ✅ | ✅ | ✅ |
| SIMD parsing | ✅ | AVX2 | AVX2/NEON | AVX2 | AVX2 |
| Kernel bypass | ❌ | AF_XDP | N/A | netmap | RIO |
| eBPF filtering | ❌ | ✅ | N/A | N/A | N/A |
| Hot-patching | ✅ | SCM_RIGHTS | SCM_RIGHTS | SCM_RIGHTS | DuplicateHandle |

---

## Testing Matrix

```yaml
# .github/workflows/ci.yml

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-22.04, ubuntu-20.04, macos-13, macos-14, windows-2022]
        arch: [x64, arm64]
        exclude:
          - os: ubuntu-20.04
            arch: arm64
          - os: windows-2022
            arch: arm64
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build
```

---

## References

### Platform Documentation
- [Linux io_uring](https://kernel.dk/io_uring.pdf)
- [macOS kqueue](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/kqueue.2.html)
- [FreeBSD kqueue](https://www.freebsd.org/cgi/man.cgi?query=kqueue)
- [Windows IOCP](https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [Windows RIO](https://docs.microsoft.com/en-us/windows/win32/winsock/rio-networking-extensions)

### SIMD Resources
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [ARM NEON Guide](https://developer.arm.com/architectures/instruction-sets/intrinsics/)

### Libraries
- [liburing](https://github.com/axboe/liburing) - io_uring wrapper
- [nghttp2](https://nghttp2.org/) - HTTP/2
- [ngtcp2](https://github.com/ngtcp2/ngtcp2) - QUIC
- [libbpf](https://github.com/libbpf/libbpf) - eBPF
