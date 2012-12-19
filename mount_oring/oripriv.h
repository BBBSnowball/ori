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

#ifndef __ORIPRIV_H__
#define __ORIPRIV_H__

class OriFile
{
    OriFile() { }
    ~OriFile() { }
};

typedef enum OriFileType
{
    FILETYPE_NULL,
    FILETYPE_COMMITTED,
    FILETYPE_TEMPORARY,
    FILETYPE_REMOTE,
} OriFileType;

#define ORIPRIVID_INVALID 0
typedef uint64_t OriPrivId;

class OriFileInfo
{
public:
    OriFileInfo() {
        memset(&statInfo, 0, sizeof(statInfo));
        type = FILETYPE_NULL;
        id = 0;
    }
    ~OriFileInfo() { }
    bool isDir() { return (statInfo.st_mode & S_IFDIR) == S_IFDIR; }
    struct stat statInfo;
    ObjectHash hash;
    OriFileType type;
    OriPrivId id;
};

class OriDir
{
public:
    typedef std::map<std::string, OriPrivId>::iterator iterator;
    OriDir() { }
    ~OriDir() { }
    void add(const std::string &name, OriPrivId id)
    {
        entries[name] = id;
        setDirty();
    }
    void remove(const std::string &name)
    {
        entries.erase(name);
        setDirty();
    }
    bool isEmpty() { return entries.size() == 0; }
    void setDirty() { dirty = true; }
    void clrDirty() { dirty = true; }
    bool isDirty() { return dirty; }
    iterator begin() { return entries.begin(); }
    iterator end() { return entries.end(); }
private:
    bool dirty;
    std::map<std::string, OriPrivId> entries;
};

class OriPriv
{
public:
    OriPriv();
    ~OriPriv();
    OriPrivId generateId();
    OriFileInfo* getFileInfo(const std::string &path);
    OriFileInfo* addDir(const std::string &path);
    void rmDir(const std::string &path);
    OriDir& getDir(const std::string &path);
private:
    OriPrivId nextId;
    std::map<OriPrivId, OriDir*> dirs;
    std::map<std::string, OriFileInfo*> paths;
};

OriPriv *GetOriPriv();

#endif /* __ORIPRIV_H__ */

