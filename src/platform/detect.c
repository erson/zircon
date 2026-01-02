#include "platform/detect.h"
#include "platform/compat.h"
#include <stdio.h>
#include <string.h>

#if defined(ZIRCON_LINUX)
    #include <sys/utsname.h>
    #include <linux/version.h>
#endif

#if defined(ZIRCON_X86_64) || defined(ZIRCON_X86)
    #if defined(ZIRCON_GCC) || defined(ZIRCON_CLANG)
        #include <cpuid.h>
    #elif defined(ZIRCON_MSVC)
        #include <intrin.h>
    #endif
#endif

#if defined(ZIRCON_ARM64) && defined(ZIRCON_LINUX)
    #include <sys/auxv.h>
    #include <asm/hwcap.h>
#endif

typedef struct {
    bool has_sse42;
    bool has_avx2;
    bool has_avx512f;
    bool has_neon;
    bool has_sve;
} simd_caps_t;

typedef struct {
    bool has_io_uring;
    bool has_epoll;
    bool has_kqueue;
    bool has_iocp;
    bool has_sendfile;
    bool has_so_reuseport;
    bool has_af_xdp;
} platform_caps_t;

static simd_caps_t g_simd_caps;
static platform_caps_t g_platform_caps;
static bool g_caps_initialized = false;

#if defined(ZIRCON_X86_64) || defined(ZIRCON_X86)
static void detect_x86_simd(simd_caps_t *caps) {
    unsigned int eax, ebx, ecx, edx;

#if defined(ZIRCON_GCC) || defined(ZIRCON_CLANG)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        caps->has_sse42 = (ecx & (1 << 20)) != 0;
    }
    
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        caps->has_avx2 = (ebx & (1 << 5)) != 0;
        caps->has_avx512f = (ebx & (1 << 16)) != 0;
    }
#elif defined(ZIRCON_MSVC)
    int cpu_info[4];
    __cpuid(cpu_info, 1);
    caps->has_sse42 = (cpu_info[2] & (1 << 20)) != 0;
    
    __cpuidex(cpu_info, 7, 0);
    caps->has_avx2 = (cpu_info[1] & (1 << 5)) != 0;
    caps->has_avx512f = (cpu_info[1] & (1 << 16)) != 0;
#endif
}
#endif

#if defined(ZIRCON_ARM64)
static void detect_arm_simd(simd_caps_t *caps) {
#if defined(ZIRCON_LINUX)
    unsigned long hwcap = getauxval(AT_HWCAP);
    caps->has_neon = (hwcap & HWCAP_ASIMD) != 0;
    #ifdef HWCAP_SVE
    caps->has_sve = (hwcap & HWCAP_SVE) != 0;
    #endif
#elif defined(ZIRCON_MACOS)
    caps->has_neon = true;
    caps->has_sve = false;
#else
    caps->has_neon = true;
    caps->has_sve = false;
#endif
}
#endif

#if defined(ZIRCON_LINUX)
static void detect_linux_kernel_version(int *major, int *minor) {
    struct utsname buf;
    *major = 0;
    *minor = 0;
    
    if (uname(&buf) == 0) {
        sscanf(buf.release, "%d.%d", major, minor);
    }
}

static bool linux_kernel_at_least(int major, int minor) {
    int kmaj, kmin;
    detect_linux_kernel_version(&kmaj, &kmin);
    return (kmaj > major) || (kmaj == major && kmin >= minor);
}
#endif

static void detect_platform_caps(platform_caps_t *caps) {
    memset(caps, 0, sizeof(*caps));

#if defined(ZIRCON_LINUX)
    caps->has_epoll = true;
    caps->has_sendfile = true;
    caps->has_so_reuseport = true;
    caps->has_io_uring = linux_kernel_at_least(5, 1);
    caps->has_af_xdp = linux_kernel_at_least(5, 10);
#endif

#if defined(ZIRCON_HAS_KQUEUE)
    caps->has_kqueue = true;
#endif

#if defined(ZIRCON_MACOS) || defined(ZIRCON_FREEBSD)
    caps->has_sendfile = true;
    caps->has_so_reuseport = true;
#endif

#if defined(ZIRCON_WINDOWS)
    caps->has_iocp = true;
    caps->has_sendfile = true;
#endif
}

static void detect_simd_caps(simd_caps_t *caps) {
    memset(caps, 0, sizeof(*caps));
    
#if defined(ZIRCON_X86_64) || defined(ZIRCON_X86)
    detect_x86_simd(caps);
#elif defined(ZIRCON_ARM64)
    detect_arm_simd(caps);
#elif defined(ZIRCON_ARM32)
    caps->has_neon = true;
#endif
}

void platform_detect_init(void) {
    if (g_caps_initialized) return;
    
    detect_simd_caps(&g_simd_caps);
    detect_platform_caps(&g_platform_caps);
    g_caps_initialized = true;
}

bool platform_has_sse42(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_simd_caps.has_sse42;
}

bool platform_has_avx2(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_simd_caps.has_avx2;
}

bool platform_has_avx512(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_simd_caps.has_avx512f;
}

bool platform_has_neon(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_simd_caps.has_neon;
}

bool platform_has_sve(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_simd_caps.has_sve;
}

bool platform_has_io_uring(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_io_uring;
}

bool platform_has_epoll(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_epoll;
}

bool platform_has_kqueue(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_kqueue;
}

bool platform_has_iocp(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_iocp;
}

bool platform_has_sendfile(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_sendfile;
}

bool platform_has_so_reuseport(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_so_reuseport;
}

bool platform_has_af_xdp(void) {
    if (!g_caps_initialized) platform_detect_init();
    return g_platform_caps.has_af_xdp;
}

const char *platform_get_event_backend_name(void) {
    if (!g_caps_initialized) platform_detect_init();
    
    if (g_platform_caps.has_io_uring) return "io_uring";
    if (g_platform_caps.has_epoll) return "epoll";
    if (g_platform_caps.has_kqueue) return "kqueue";
    if (g_platform_caps.has_iocp) return "IOCP";
    return "poll";
}

void platform_print_info(void) {
    if (!g_caps_initialized) platform_detect_init();
    
    printf("Zircon Platform Information\n");
    printf("===========================\n");
    printf("OS:           %s\n", ZIRCON_PLATFORM_NAME);
    printf("Architecture: %s\n", ZIRCON_ARCH_NAME);
    printf("Compiler:     %s\n", ZIRCON_COMPILER_NAME);
    printf("\n");
    
    printf("Event Backend: %s\n", platform_get_event_backend_name());
    printf("\n");
    
    printf("I/O Features:\n");
    printf("  io_uring:     %s\n", g_platform_caps.has_io_uring ? "yes" : "no");
    printf("  epoll:        %s\n", g_platform_caps.has_epoll ? "yes" : "no");
    printf("  kqueue:       %s\n", g_platform_caps.has_kqueue ? "yes" : "no");
    printf("  IOCP:         %s\n", g_platform_caps.has_iocp ? "yes" : "no");
    printf("  sendfile:     %s\n", g_platform_caps.has_sendfile ? "yes" : "no");
    printf("  SO_REUSEPORT: %s\n", g_platform_caps.has_so_reuseport ? "yes" : "no");
    printf("  AF_XDP:       %s\n", g_platform_caps.has_af_xdp ? "yes" : "no");
    printf("\n");
    
    printf("SIMD Capabilities:\n");
#if defined(ZIRCON_X86_64) || defined(ZIRCON_X86)
    printf("  SSE4.2:  %s\n", g_simd_caps.has_sse42 ? "yes" : "no");
    printf("  AVX2:    %s\n", g_simd_caps.has_avx2 ? "yes" : "no");
    printf("  AVX-512: %s\n", g_simd_caps.has_avx512f ? "yes" : "no");
#elif defined(ZIRCON_ARM64) || defined(ZIRCON_ARM32)
    printf("  NEON: %s\n", g_simd_caps.has_neon ? "yes" : "no");
    printf("  SVE:  %s\n", g_simd_caps.has_sve ? "yes" : "no");
#else
    printf("  (no SIMD detection for this architecture)\n");
#endif
}
