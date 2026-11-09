/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Final Effect 3D Contributors (compatibility shim, not upstream Blender code) */

/*
 * Compatibility shim for the assert-reporting hooks declared by Blender's BLI_assert.h.
 * Upstream these are defined in blenlib's assert/system module together with the full
 * backtrace machinery; here they report to stderr and abort, which keeps the vendored
 * math subset stand-alone and self-hosting.
 */
#include <cstdio>
#include <cstdlib>

void _BLI_assert_print_pos(const char *file, int line, const char *function, const char *id)
{
  std::fprintf(stderr, "blenlib assert failed: %s at %s:%d\n", id, file, line);
  (void)function;
}

void _BLI_assert_print_extra(const char *str)
{
  std::fprintf(stderr, "  extra: %s\n", str);
}

void _BLI_assert_print_backtrace(void)
{
}

void _BLI_assert_unreachable_print(const char *file, int line, const char *function)
{
  std::fprintf(stderr, "blenlib unreachable reached at %s:%d\n", file, line);
  (void)function;
  std::abort();
}

void _BLI_assert_abort(void)
{
  std::abort();
}
