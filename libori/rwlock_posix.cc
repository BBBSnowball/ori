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
/*
 * RWLock Class
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#include "rwlock.h"
#include "debug.h"
#include "util.h"

#define LOG_LOCKING 0

RWLock::RWLock()
{
    pthread_rwlock_init(&lockHandle, NULL);
    lockNum = gLockNum++;
}

RWLock::~RWLock()
{
    pthread_rwlock_destroy(&lockHandle);
}

RWKey::sp RWLock::readLock()
{
#if LOG_LOCKING == 1
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    LOG("%u readLock: %u", tid, lockNum);
    //Util_LogBacktrace();
#endif

    pthread_rwlock_rdlock(&lockHandle);

#if LOG_LOCKING == 1
    LOG("%u success readLock: %u", tid, lockNum);
#endif
    return RWKey::sp(new RWKey(this));
}

RWKey::sp RWLock::tryReadLock()
{
    NOT_IMPLEMENTED(false);
    if (pthread_rwlock_tryrdlock(&lockHandle) == 0) {
        return RWKey::sp(new RWKey(this));
    }
    return RWKey::sp();
}

RWKey::sp RWLock::writeLock()
{
#if LOG_LOCKING == 1
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    LOG("%u writeLock: %u", tid, lockNum);
    //Util_LogBacktrace();
#endif

    pthread_rwlock_wrlock(&lockHandle);

#if LOG_LOCKING == 1
    LOG("%u success writeLock: %u", tid, lockNum);
#endif
    return RWKey::sp(new RWKey(this));
}

RWKey::sp RWLock::tryWriteLock()
{
    NOT_IMPLEMENTED(false);
    if (pthread_rwlock_trywrlock(&lockHandle) == 0) {
        return RWKey::sp(new RWKey(this));
    }
    return RWKey::sp();
}

void RWLock::unlock()
{
    pthread_rwlock_unlock(&lockHandle);

#if LOG_LOCKING == 1
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    LOG("%u unlock: %u", tid, lockNum);
#endif
}

