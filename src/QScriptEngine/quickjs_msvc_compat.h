#ifndef QUICKJS_MSVC_COMPAT_H
#define QUICKJS_MSVC_COMPAT_H

// MSVC does not support C11 _Alignas when compiling .c files.
// Map it to __declspec(align(x)) which is the MSVC equivalent.
// This header is force-included via /FI in ScriptEngine.pri,
// so it applies to all quickjs .c files without modifying upstream code.

#if defined(_MSC_VER) && !defined(__clang__)
#define _Alignas(x) __declspec(align(x))
#endif

#endif // QUICKJS_MSVC_COMPAT_H
