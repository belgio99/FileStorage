/*
 The MIT License (MIT)

 Copyright (c) 2014 Christian Schramm <christian.h.m.schramm@gmail.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 */
#pragma once

#include <stddef.h>

#if defined(_MSC_VER)
#define shoco_restrict __restrict
#elif __GNUC__
#define shoco_restrict __restrict__
#else
#define shoco_restrict restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t shoco_compress(const char * const shoco_restrict in, size_t len, char * const shoco_restrict out, size_t bufsize);
size_t shoco_decompress(const char * const shoco_restrict in, size_t len, char * const shoco_restrict out, size_t bufsize);

#ifdef __cplusplus
}
#endif

