// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

namespace Common
{
void MemArena::GrabSHMSegment(size_t size)
{
  const std::string file_name = "/dolphin-emu." + std::to_string(getpid());
  fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd == -1)
  {
    ERROR_LOG_FMT(MEMMAP, "shm_open failed: {}", strerror(errno));
    return;
  }
  shm_unlink(file_name.c_str());
  if (ftruncate(fd, size) < 0)
    ERROR_LOG_FMT(MEMMAP, "Failed to allocate low memory space");
}

void MemArena::ReleaseSHMSegment()
{
  close(fd);
}

void* MemArena::CreateView(s64 offset, size_t size, void* base)
{
  void* retval = mmap(base, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | ((base == nullptr) ? 0 : MAP_FIXED), fd, offset);

  if (retval == MAP_FAILED)
  {
    NOTICE_LOG_FMT(MEMMAP, "mmap failed");
    return nullptr;
  }
  else
  {
    return retval;
  }
}

void MemArena::ReleaseView(void* view, size_t size)
{
  munmap(view, size);
}

u8* MemArena::FindMemoryBase()
{
#if _ARCH_32
  const size_t memory_size = 0x31000000;
#else
  const size_t memory_size = 0x400000000;
#endif

  const int flags = MAP_ANON | MAP_PRIVATE;
  void* base = mmap(nullptr, memory_size, PROT_NONE, flags, -1, 0);
  if (base == MAP_FAILED)
  {
    PanicAlertFmt("Failed to map enough memory space: {}", LastStrerrorString());
    return nullptr;
  }
  munmap(base, memory_size);
  return static_cast<u8*>(base);
}
}  // namespace Common
