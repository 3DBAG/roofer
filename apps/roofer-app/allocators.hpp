#pragma once
#ifdef RF_ENABLE_HEAP_TRACING
// Overrides for heap allocation counting
// Ref.: https://www.youtube.com/watch?v=sLlGEUO_EGE
namespace {
  struct HeapAllocationCounter {
    std::atomic<size_t> total_allocated = 0;
    std::atomic<size_t> total_freed = 0;
    [[nodiscard]] size_t current_usage() const {
      return total_allocated - total_freed;
    };
  };
  HeapAllocationCounter heap_allocation_counter;
}  // namespace
#endif

/*
 * Code snippet below is taken from
 * https://github.com/microsoft/mimalloc/blob/dev/include/mimalloc-new-delete.h
 * and modified to work with the roofer trace feature for heap memory usage.
 */
#if defined(IS_LINUX) || defined(IS_MACOS)
#if defined(_MSC_VER) && defined(_Ret_notnull_) && \
    defined(_Post_writable_byte_size_)
   // stay consistent with VCRT definitions
#define mi_decl_new(n) \
  mi_decl_nodiscard mi_decl_restrict _Ret_notnull_ _Post_writable_byte_size_(n)
#define mi_decl_new_nothrow(n)                                                 \
  mi_decl_nodiscard mi_decl_restrict _Ret_maybenull_ _Success_(return != NULL) \
      _Post_writable_byte_size_(n)
#else
#define mi_decl_new(n) mi_decl_nodiscard mi_decl_restrict
#define mi_decl_new_nothrow(n) mi_decl_nodiscard mi_decl_restrict
#endif

void operator delete(void* p) noexcept { mi_free(p); };
void operator delete[](void* p) noexcept { mi_free(p); };

void operator delete(void* p, const std::nothrow_t&) noexcept { mi_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { mi_free(p); }

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new(n);
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new(n);
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n,
                                          const std::nothrow_t& tag) noexcept {
  (void)(tag);
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_nothrow(n);
}
mi_decl_new_nothrow(n) void* operator new[](
    std::size_t n, const std::nothrow_t& tag) noexcept {
  (void)(tag);
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_nothrow(n);
}

#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size(p, n);
};
void operator delete[](void* p, std::size_t n) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size(p, n);
};
#endif

#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete[](void* p, std::align_val_t al) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size_aligned(p, n, static_cast<size_t>(al));
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size_aligned(p, n, static_cast<size_t>(al));
};
void operator delete(void* p, std::align_val_t al,
                     const std::nothrow_t&) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete[](void* p, std::align_val_t al,
                       const std::nothrow_t&) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned(n, static_cast<size_t>(al));
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned(n, static_cast<size_t>(al));
}
void* operator new(std::size_t n, std::align_val_t al,
                   const std::nothrow_t&) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned_nothrow(n, static_cast<size_t>(al));
}
void* operator new[](std::size_t n, std::align_val_t al,
                     const std::nothrow_t&) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned_nothrow(n, static_cast<size_t>(al));
}
#endif
#endif
/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 * Ref.:
 * https://lemire.me/blog/2022/11/10/measuring-the-memory-usage-of-your-c-program/
 */
#if defined(IS_WINDOWS)
#include <windows.h>
#include <psapi.h>

#elif defined(IS_LINUX) || defined(IS_MACOS)
#include <sys/resource.h>
#include <unistd.h>

#if defined(IS_MACOS)
#include <mach/mach.h>
#elif defined(IS_LINUX)
#include <cstdio>
#endif

#endif

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
inline size_t GetCurrentRSS() {
#if defined(IS_WINDOWS)
  /* Windows -------------------------------------------------- */
  PROCESS_MEMORY_COUNTERS info;
  GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
  return (size_t)info.WorkingSetSize;

#elif defined(IS_MACOS)
  /* OSX ------------------------------------------------------ */
  struct mach_task_basic_info info;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info,
                &infoCount) != KERN_SUCCESS)
    return (size_t)0L; /* Can't access? */
  return (size_t)info.resident_size;

#elif defined(IS_LINUX)
  /* Linux ---------------------------------------------------- */
  long rss = 0L;
  FILE* fp = NULL;
  if ((fp = fopen("/proc/self/statm", "r")) == NULL)
    return (size_t)0L; /* Can't open? */
  if (fscanf(fp, "%*s%ld", &rss) != 1) {
    fclose(fp);
    return (size_t)0L; /* Can't read? */
  }
  fclose(fp);
  return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
  /* AIX, BSD, Solaris, and Unknown OS ------------------------ */
  return (size_t)0L; /* Unsupported. */
#endif
}
