/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSSIGNAL_H_
#define LIBRARIES_NACL_IO_OSSIGNAL_H_

#if !defined(WIN23)
#include <signal.h>
#if defined(__GLIBC__)
typedef __sighandler_t sighandler_t;
#else
typedef _sig_func_ptr sighandler_t;
#endif
#endif

#endif
