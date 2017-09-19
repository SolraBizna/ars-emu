/*
  Include this in files that should always be highly optimized, even when
  debugging. This is only really necessary in FX implementations.
*/

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("3","fast-math")
#endif
