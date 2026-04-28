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

void* operator new(size_t size) {
  heap_allocation_counter.total_allocated += size;
  return malloc(size);
}
void operator delete(void* memory, size_t size) noexcept {
  heap_allocation_counter.total_freed += size;
  free(memory);
};

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
