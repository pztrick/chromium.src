/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_SDK_UTIL_MACROS_H_
#define LIBRARIES_SDK_UTIL_MACROS_H_

/**
 * A macro to disallow the evil copy constructor and operator= functions
 * This should be used in the private: declarations for a class.
 */
#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)

/** returns the size of a member of a struct. */
#define MEMBER_SIZE(struct_name, member) sizeof(((struct_name*)0)->member)

/**
 * Macros to prevent name mangling of definitions, allowing them to be
 * referenced from C.
 */
#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
#else
# define EXTERN_C_BEGIN
# define EXTERN_C_END
#endif  /* __cplusplus */

/**
 * Macros to help force linkage of symbols that otherwise would not be
 * included.
 *
 * // In a source file that you want to force linkage (file scope):
 * FORCE_LINK_THIS(myfilename);
 *
 * // In a source file that you are sure will be linked (file scope):
 * FORCE_LINK_THAT(myfilename)
 *
 */
#define FORCE_LINK_THIS(x) int force_link_##x = 0;
#define FORCE_LINK_THAT(x) \
  void force_link_function_##x() { \
    extern int force_link_##x; \
    force_link_##x = 1; \
  }

/**
 * Macro to error out when a printf-like function is passed incorrect arguments.
 *
 * Use like this:
 * void foo(const char* fmt, ...) PRINTF_LIKE(1, 2);
 *
 * The first argument is the location of the fmt string (1-based).
 * The second argument is the location of the first argument to validate (also
 *   1-based, but can be zero if the function uses a va_list, like vprintf.)
 */
#if defined(__GNUC__)
#define PRINTF_LIKE(a, b) __attribute__ ((format(printf, a, b)))
#else
#define PRINTF_LIKE(a, b)
#endif

#endif  /* LIBRARIES_SDK_UTIL_MACROS_H_ */
