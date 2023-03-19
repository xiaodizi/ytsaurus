#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

//===-- llvm/Support/DataTypes.h - Define fixed size types ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Due to layering constraints (Support depends on llvm-c) this is a thin
// wrapper around the implementation that lives in llvm-c, though most clients
// can/should think of this as being provided by Support for simplicity (not
// many clients are aware of their dependency on llvm-c).
//
//===----------------------------------------------------------------------===//

#include "llvm-c/DataTypes.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif