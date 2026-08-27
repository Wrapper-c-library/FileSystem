#ifndef LIBRARY_CPP_WRAPPER_FILEMANAGER_EXPORT_H
#define LIBRARY_CPP_WRAPPER_FILEMANAGER_EXPORT_H

// ===========================================================================
// Cross-platform shared-library export macros.
//
// When the library is built as a shared library (.dll on Windows, .so on
// Linux, .dylib on macOS), symbols that should be visible to consumers must
// be explicitly exported. This header provides a single FSW_API macro that
// expands to the correct platform-specific annotation:
//
//   - When compiling the library itself, define FSW_EXPORTS so that FSW_API
//     becomes __declspec(dllexport) on Windows (export) and
//     __attribute__((visibility("default"))) on GCC/Clang.
//   - When consuming the library, do NOT define FSW_EXPORTS; FSW_API becomes
//     __declspec(dllimport) on Windows (slightly faster calls) and a no-op
//     on GCC/Clang (where shared symbols are visible by default).
//
// CMake handles the FSW_EXPORTS define automatically when BUILD_SHARED_LIBS
// is ON via target_compile_definitions(library PRIVATE FSW_EXPORTS).
// ===========================================================================

#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef FSW_EXPORTS
     // Building the library — export symbols.
#    define FSW_API __declspec(dllexport)
#  else
     // Consuming the library — import symbols.
#    define FSW_API __declspec(dllimport)
#  endif
#else
#  if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__clang__)
     // On ELF / Mach-O, mark the symbol as having default visibility so it
     // is exported from the shared object even when -fvisibility=hidden is
     // used at compile time.
#    define FSW_API __attribute__((visibility("default")))
#  else
#    define FSW_API
#  endif
#endif

// FSW_LOCAL marks symbols that should never be exported. Use it on internal
// helpers to keep the shared library's exported symbol table small.
#if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__clang__)
#  define FSW_LOCAL __attribute__((visibility("hidden")))
#else
#  define FSW_LOCAL
#endif

#endif // LIBRARY_CPP_WRAPPER_FILEMANAGER_EXPORT_H
