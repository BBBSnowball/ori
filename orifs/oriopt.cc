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

#include <cstdio>
#include <cstddef>
#include <cstring>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "oriopt.h"

#define MOUNT_ORI_OPT(t, p, v) {t, offsetof(mount_ori_config, p), v}
static struct fuse_opt mount_ori_opts[] = {
    MOUNT_ORI_OPT("--shallow", shallow, 1),
    MOUNT_ORI_OPT("--nocache", nocache, 1),
    MOUNT_ORI_OPT("--repo=%s", repo_path, 0),
    MOUNT_ORI_OPT("--clone=%s", clone_path, 0),
    { NULL, 0, 0 }, // FUSE_OPT_END: Macro incompatible with C++
};

// XXX: FUSE driver isn't ready for multithreading yet!
#undef FUSE_SINGLE_THREADED
#define FUSE_SINGLE_THREADED 1

void mount_ori_parse_opt(struct fuse_args *args, mount_ori_config *conf)
{
    memset(conf, 0, sizeof(mount_ori_config));
    fuse_opt_parse(args, conf, mount_ori_opts, 0);

#if defined(FUSE_SINGLE_THREADED) && FUSE_SINGLE_THREADED == 1
    // printf("FUSE forcing single threaded\n");
    fuse_opt_add_arg(args, "-s"); // Force single-threaded
#endif
}
