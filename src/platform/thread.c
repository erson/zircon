/* _GNU_SOURCE must be defined before any system headers for CPU_ZERO/CPU_SET */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "platform/detect.h"
#include "platform/compat.h"
#include <stdlib.h>
#if defined(ZIRCON_LINUX)
    #include <sched.h>
    #include <unistd.h>
#endif

#if defined(ZIRCON_MACOS)
    #include <pthread.h>
    #include <mach/mach_init.h>
    #include <mach/thread_act.h>
    #include <mach/thread_policy.h>
#endif

#if defined(ZIRCON_FREEBSD)
    #include <pthread_np.h>
    #include <sys/cpuset.h>
#endif

unsigned int platform_cpu_count(void) {
#if defined(ZIRCON_LINUX) || defined(ZIRCON_BSD_LIKE)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (unsigned int)n : 1;
#elif defined(ZIRCON_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
#else
    return 1;
#endif
}

int platform_thread_set_affinity(pthread_t thread, unsigned int cpu) {
#if defined(ZIRCON_LINUX)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    
#elif defined(ZIRCON_MACOS)
    thread_affinity_policy_data_t policy = { (integer_t)cpu };
    mach_port_t mach_thread = pthread_mach_thread_np(thread);
    kern_return_t ret = thread_policy_set(mach_thread, 
                                           THREAD_AFFINITY_POLICY,
                                           (thread_policy_t)&policy, 
                                           THREAD_AFFINITY_POLICY_COUNT);
    return (ret == KERN_SUCCESS) ? 0 : -1;
    
#elif defined(ZIRCON_FREEBSD)
    cpuset_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    
#elif defined(ZIRCON_WINDOWS)
    (void)thread;
    DWORD_PTR mask = 1ULL << cpu;
    return SetThreadAffinityMask(GetCurrentThread(), mask) ? 0 : -1;
    
#else
    (void)thread;
    (void)cpu;
    return -1;
#endif
}

int platform_thread_set_name(const char *name) {
#if defined(ZIRCON_LINUX)
    return pthread_setname_np(pthread_self(), name);
    
#elif defined(ZIRCON_MACOS)
    return pthread_setname_np(name);
    
#elif defined(ZIRCON_FREEBSD)
    pthread_set_name_np(pthread_self(), name);
    return 0;
    
#elif defined(ZIRCON_WINDOWS)
    (void)name;
    return -1;
    
#else
    (void)name;
    return -1;
#endif
}

const char *platform_thread_affinity_method(void) {
#if defined(ZIRCON_LINUX)
    return "pthread_setaffinity_np";
#elif defined(ZIRCON_MACOS)
    return "thread_policy_set (Mach)";
#elif defined(ZIRCON_FREEBSD)
    return "cpuset_setaffinity";
#elif defined(ZIRCON_WINDOWS)
    return "SetThreadAffinityMask";
#else
    return "unsupported";
#endif
}
