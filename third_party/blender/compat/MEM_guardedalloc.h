/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Final Effect 3D Contributors (compatibility shim, not upstream Blender code) */

/*
 * Compatibility shim for the guarded-allocator entry points that the vendored blenlib subset
 * needs (BLI_allocator.hh). Upstream these live in source/blender/memutils/MEM_guardedalloc.cc
 * with a guarded/debug allocator on top; here the same call signatures are provided over
 * aligned C++ allocation so the vendored allocator works stand-alone. Replace this file with
 * the real memutils module if the full guarded allocator (leak/overflow tracking) is wanted.
 */
#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

inline constexpr std::size_t MEM_MIN_CPP_ALIGNMENT = alignof(std::max_align_t);

[[nodiscard]] inline void *MEM_mallocN_aligned(std::size_t len, std::size_t alignment, const char * /*str*/)
{
  if (alignment < sizeof(void *)) {
    alignment = sizeof(void *);
  }
  if (len == 0) {
    len = 1;
  }
  const std::size_t rounded = ((len + alignment - 1) / alignment) * alignment;
  void *ptr = std::aligned_alloc(alignment, rounded);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

[[nodiscard]] inline void *MEM_new_uninitialized_aligned(std::size_t size,
                                                        std::size_t alignment,
                                                        const char *str)
{
  return MEM_mallocN_aligned(size, alignment, str);
}

[[nodiscard]] inline void *MEM_new_uninitialized(std::size_t size, const char *str)
{
  return MEM_mallocN_aligned(size, MEM_MIN_CPP_ALIGNMENT, str);
}

[[nodiscard]] inline void *MEM_new_zeroed(std::size_t size, const char *str)
{
  void *ptr = MEM_mallocN_aligned(size, MEM_MIN_CPP_ALIGNMENT, str);
  std::memset(ptr, 0, size);
  return ptr;
}

inline void MEM_delete_void(void *ptr)
{
  std::free(ptr);
}
