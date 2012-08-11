/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __TUNEABLES_H__
#define __TUNEABLES_H__

#define COPYFILE_BUFSZ	(256 * 1024)
#define HASHFILE_BUFSZ	(256 * 1024)
#define COMPFILE_BUFSZ  (16 * 1024)

#define LARGEFILE_MINIMUM (1024 * 1024)

#define ENABLE_COMPRESSION 0
// How much of a payload to check for compressibility
#define COMPCHECK_BYTES 1024
// Maximum compression ratio (0.8 means compressed file is 80% size of original)
#define COMPCHECK_RATIO 0.95

// These are soft maximums ("heuristics")
// 64 MB
#define PACKFILE_MAXSIZE (1024*1024*64)
// ~20KB overhead per packfile?
#define PACKFILE_MAXOBJS (512)

//#define ORI_USE_SHA256
#define ORI_USE_SKEIN

#endif /* __TUNEABLES_H__ */

