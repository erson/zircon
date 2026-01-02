#ifndef PLATFORM_DETECT_H
#define PLATFORM_DETECT_H

/*
 * Platform Detection
 * Compile-time detection of OS, architecture, and compiler
 */

/* Operating System Detection */
#if defined(__linux__)
    #define ZIRCON_LINUX 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_PLATFORM_NAME "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define ZIRCON_MACOS 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_BSD_LIKE 1
    #define ZIRCON_PLATFORM_NAME "macOS"
#elif defined(__FreeBSD__)
    #define ZIRCON_FREEBSD 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_BSD_LIKE 1
    #define ZIRCON_PLATFORM_NAME "FreeBSD"
#elif defined(__OpenBSD__)
    #define ZIRCON_OPENBSD 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_BSD_LIKE 1
    #define ZIRCON_PLATFORM_NAME "OpenBSD"
#elif defined(__NetBSD__)
    #define ZIRCON_NETBSD 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_BSD_LIKE 1
    #define ZIRCON_PLATFORM_NAME "NetBSD"
#elif defined(__DragonFly__)
    #define ZIRCON_DRAGONFLY 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_BSD_LIKE 1
    #define ZIRCON_PLATFORM_NAME "DragonFlyBSD"
#elif defined(_WIN32) || defined(_WIN64)
    #define ZIRCON_WINDOWS 1
    #define ZIRCON_PLATFORM_NAME "Windows"
#elif defined(__sun) && defined(__SVR4)
    #define ZIRCON_SOLARIS 1
    #define ZIRCON_UNIX 1
    #define ZIRCON_PLATFORM_NAME "Solaris"
#else
    #define ZIRCON_UNKNOWN_OS 1
    #define ZIRCON_PLATFORM_NAME "Unknown"
#endif

/* Architecture Detection */
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define ZIRCON_X86_64 1
    #define ZIRCON_64BIT 1
    #define ZIRCON_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86) || defined(__i686__)
    #define ZIRCON_X86 1
    #define ZIRCON_32BIT 1
    #define ZIRCON_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ZIRCON_ARM64 1
    #define ZIRCON_64BIT 1
    #define ZIRCON_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
    #define ZIRCON_ARM32 1
    #define ZIRCON_32BIT 1
    #define ZIRCON_ARCH_NAME "ARM32"
#elif defined(__powerpc64__) || defined(__ppc64__)
    #define ZIRCON_PPC64 1
    #define ZIRCON_64BIT 1
    #define ZIRCON_ARCH_NAME "PPC64"
#elif defined(__riscv) && __riscv_xlen == 64
    #define ZIRCON_RISCV64 1
    #define ZIRCON_64BIT 1
    #define ZIRCON_ARCH_NAME "RISC-V 64"
#else
    #define ZIRCON_UNKNOWN_ARCH 1
    #define ZIRCON_ARCH_NAME "Unknown"
#endif

/* Compiler Detection */
#if defined(__clang__)
    #define ZIRCON_CLANG 1
    #define ZIRCON_COMPILER_NAME "Clang"
    #define ZIRCON_COMPILER_VERSION __clang_major__
#elif defined(__GNUC__)
    #define ZIRCON_GCC 1
    #define ZIRCON_COMPILER_NAME "GCC"
    #define ZIRCON_COMPILER_VERSION __GNUC__
#elif defined(_MSC_VER)
    #define ZIRCON_MSVC 1
    #define ZIRCON_COMPILER_NAME "MSVC"
    #define ZIRCON_COMPILER_VERSION _MSC_VER
#else
    #define ZIRCON_UNKNOWN_COMPILER 1
    #define ZIRCON_COMPILER_NAME "Unknown"
    #define ZIRCON_COMPILER_VERSION 0
#endif

/* Endianness Detection */
#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define ZIRCON_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define ZIRCON_BIG_ENDIAN 1
    #endif
#elif defined(ZIRCON_WINDOWS)
    #define ZIRCON_LITTLE_ENDIAN 1
#endif

/* Feature Availability - Compile Time */

/* kqueue: BSD and macOS */
#if defined(ZIRCON_BSD_LIKE)
    #define ZIRCON_HAS_KQUEUE 1
#endif

/* epoll: Linux only */
#if defined(ZIRCON_LINUX)
    #define ZIRCON_HAS_EPOLL 1
#endif

/* IOCP: Windows only */
#if defined(ZIRCON_WINDOWS)
    #define ZIRCON_HAS_IOCP 1
#endif

/* sendfile variants */
#if defined(ZIRCON_LINUX)
    #define ZIRCON_HAS_SENDFILE_LINUX 1
#elif defined(ZIRCON_MACOS) || defined(ZIRCON_FREEBSD)
    #define ZIRCON_HAS_SENDFILE_BSD 1
#elif defined(ZIRCON_WINDOWS)
    #define ZIRCON_HAS_TRANSMITFILE 1
#endif

/* SO_REUSEPORT */
#if defined(ZIRCON_LINUX) || defined(ZIRCON_BSD_LIKE)
    #define ZIRCON_HAS_SO_REUSEPORT 1
#endif

/* CPU affinity */
#if defined(ZIRCON_LINUX) || defined(ZIRCON_FREEBSD)
    #define ZIRCON_HAS_CPU_AFFINITY_PTHREAD 1
#elif defined(ZIRCON_MACOS)
    #define ZIRCON_HAS_CPU_AFFINITY_MACH 1
#elif defined(ZIRCON_WINDOWS)
    #define ZIRCON_HAS_CPU_AFFINITY_WIN 1
#endif

/* SIMD - compile time hints (runtime detection required for actual use) */
#if defined(ZIRCON_X86_64) || defined(ZIRCON_X86)
    #define ZIRCON_MAY_HAVE_SSE42 1
    #define ZIRCON_MAY_HAVE_AVX2 1
    #define ZIRCON_MAY_HAVE_AVX512 1
#elif defined(ZIRCON_ARM64)
    #define ZIRCON_MAY_HAVE_NEON 1
    #define ZIRCON_MAY_HAVE_SVE 1
#elif defined(ZIRCON_ARM32)
    #define ZIRCON_MAY_HAVE_NEON 1
#endif

/* Linux-specific features (version dependent - checked at runtime) */
#if defined(ZIRCON_LINUX)
    #define ZIRCON_MAY_HAVE_IO_URING 1
    #define ZIRCON_MAY_HAVE_EPOLL 1
    #define ZIRCON_MAY_HAVE_AF_XDP 1
    #define ZIRCON_MAY_HAVE_EBPF 1
#endif

/* Compiler Attributes */
#if defined(ZIRCON_GCC) || defined(ZIRCON_CLANG)
    #define ZIRCON_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define ZIRCON_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define ZIRCON_UNUSED      __attribute__((unused))
    #define ZIRCON_NORETURN    __attribute__((noreturn))
    #define ZIRCON_PACKED      __attribute__((packed))
    #define ZIRCON_ALIGNED(n)  __attribute__((aligned(n)))
    #define ZIRCON_ALWAYS_INLINE __attribute__((always_inline)) inline
    #define ZIRCON_NOINLINE    __attribute__((noinline))
    #define ZIRCON_PURE        __attribute__((pure))
    #define ZIRCON_CONST       __attribute__((const))
    #define ZIRCON_DEPRECATED  __attribute__((deprecated))
    #define ZIRCON_RESTRICT    __restrict__
#elif defined(ZIRCON_MSVC)
    #define ZIRCON_LIKELY(x)   (x)
    #define ZIRCON_UNLIKELY(x) (x)
    #define ZIRCON_UNUSED
    #define ZIRCON_NORETURN    __declspec(noreturn)
    #define ZIRCON_PACKED
    #define ZIRCON_ALIGNED(n)  __declspec(align(n))
    #define ZIRCON_ALWAYS_INLINE __forceinline
    #define ZIRCON_NOINLINE    __declspec(noinline)
    #define ZIRCON_PURE
    #define ZIRCON_CONST
    #define ZIRCON_DEPRECATED  __declspec(deprecated)
    #define ZIRCON_RESTRICT    __restrict
#else
    #define ZIRCON_LIKELY(x)   (x)
    #define ZIRCON_UNLIKELY(x) (x)
    #define ZIRCON_UNUSED
    #define ZIRCON_NORETURN
    #define ZIRCON_PACKED
    #define ZIRCON_ALIGNED(n)
    #define ZIRCON_ALWAYS_INLINE inline
    #define ZIRCON_NOINLINE
    #define ZIRCON_PURE
    #define ZIRCON_CONST
    #define ZIRCON_DEPRECATED
    #define ZIRCON_RESTRICT
#endif

/* Thread Local Storage */
#if defined(ZIRCON_GCC) || defined(ZIRCON_CLANG)
    #define ZIRCON_THREAD_LOCAL __thread
#elif defined(ZIRCON_MSVC)
    #define ZIRCON_THREAD_LOCAL __declspec(thread)
#elif __STDC_VERSION__ >= 201112L
    #define ZIRCON_THREAD_LOCAL _Thread_local
#else
    #define ZIRCON_THREAD_LOCAL
#endif

/* Export/Import for shared libraries */
#if defined(ZIRCON_WINDOWS)
    #ifdef ZIRCON_BUILD_DLL
        #define ZIRCON_API __declspec(dllexport)
    #elif defined(ZIRCON_USE_DLL)
        #define ZIRCON_API __declspec(dllimport)
    #else
        #define ZIRCON_API
    #endif
#elif defined(ZIRCON_GCC) || defined(ZIRCON_CLANG)
    #define ZIRCON_API __attribute__((visibility("default")))
#else
    #define ZIRCON_API
#endif

#endif /* PLATFORM_DETECT_H */
