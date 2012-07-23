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

#include <unistd.h>
#include <sys/types.h>

#include "treediff.h"
#include "debug.h"
#include "util.h"
#include "scan.h"

using namespace std;

/*
 * TreeDiff
 */
TreeDiff::TreeDiff()
{
}

void
TreeDiff::diffTwoTrees(const Tree::Flat &t1, const Tree::Flat &t2)
{
    map<string, TreeEntry>::const_iterator it;

    for (it = t1.begin(); it != t1.end(); it++) {
	string path = (*it).first;
	TreeEntry entry = (*it).second;
	map<string, TreeEntry>::const_iterator it2;

	it2 = t2.find(path);
	if (it2 == t2.end()) {
	    TreeDiffEntry diffEntry;

	    // New file or directory
	    diffEntry.filepath = path;
	    if (entry.type == TreeEntry::Tree) {
		diffEntry.type = TreeDiffEntry::NewDir;
	    } else {
		diffEntry.type = TreeDiffEntry::NewFile;
		// diffEntry.newFilename = ...
	    }

	    entries.push_back(diffEntry);
	} else {
	    TreeEntry entry2 = (*it2).second;

	    if (entry.type != TreeEntry::Tree && entry2.type == TreeEntry::Tree) {
		// Replaced file with directory
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::Deleted;
		entries.push_back(diffEntry);

		diffEntry.type = TreeDiffEntry::NewFile;
		entries.push_back(diffEntry);
		// diffEntry.newFilename = ...
	    } else if (entry.type == TreeEntry::Tree &&
		       entry2.type != TreeEntry::Tree) {
		// Replaced directory with file
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::Deleted;
		entries.push_back(diffEntry);

		diffEntry.type = TreeDiffEntry::NewDir;
		entries.push_back(diffEntry);
	    } else {
		// Check for mismatch
		TreeEntry entry2 = (*it2).second;

		// XXX: This should do the right thing even if for some reason
		// the file was a small file and became a large file.  That 
		// should never happen though!
		if (entry.type != TreeEntry::Tree && entry.hash != entry2.hash) {
		    TreeDiffEntry diffEntry;

		    diffEntry.filepath = path;
		    diffEntry.type = TreeDiffEntry::Modified;
		    // diffEntry.newFilename = ...

		    entries.push_back(diffEntry);
		}
	    }
	}
    }

    for (it = t2.begin(); it != t2.end(); it++) {
	string path = (*it).first;
	TreeEntry entry = (*it).second;
	map<string, TreeEntry>::const_iterator it1;

	it1 = t1.find(path);
	if (it1 == t1.end()) {
	    TreeDiffEntry diffEntry;

	    // Deleted file or directory
	    diffEntry.filepath = path;
	    diffEntry.type = TreeDiffEntry::Deleted;

	    entries.push_back(diffEntry);
	}
    }

    return;
}

struct _scanHelperData {
    set<string> *wd_paths;
    map<string, TreeEntry> *flattened_tree;
    TreeDiff *td;

    size_t cwdLen;
    Repo *repo;
};

int _diffToDirHelper(void *arg, const char *path)
{
    string fullPath = path;
    _scanHelperData *sd = (_scanHelperData *)arg;

    string relPath = fullPath.substr(sd->cwdLen);
    sd->wd_paths->insert(relPath);

    TreeDiffEntry diffEntry;
    diffEntry.filepath = relPath;

    map<string, TreeEntry>::iterator it = sd->flattened_tree->find(relPath);
    if (it == sd->flattened_tree->end()) {
        // New file/dir
        if (Util_IsDirectory(fullPath)) {
            diffEntry.type = TreeDiffEntry::NewDir;
        }
        else {
            diffEntry.type = TreeDiffEntry::NewFile;
            diffEntry.newFilename = fullPath;
        }
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    // Potentially modified file/dir
    const TreeEntry &te = (*it).second;
    if (Util_IsDirectory(fullPath)) {
        if (te.type != TreeEntry::Tree) {
            // File replaced by dir
            diffEntry.type = TreeDiffEntry::Deleted;
            sd->td->entries.push_back(diffEntry);
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->entries.push_back(diffEntry);
        }
        return 0;
    }

    if (te.type == TreeEntry::Tree) {
        // Dir replaced by file
        diffEntry.type = TreeDiffEntry::Deleted;
        sd->td->entries.push_back(diffEntry);
        diffEntry.type = TreeDiffEntry::NewFile;
        diffEntry.newFilename = fullPath;
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    // Check if file is modified
    // TODO: heuristic (mtime, filesize) for determining whether a file has been modified
    std::string newHash = Util_HashFile(fullPath);
    bool modified = false;
    if (te.type == TreeEntry::Blob) {
        modified = newHash != te.hash;
    }
    else if (te.type == TreeEntry::LargeBlob) {
        modified = newHash != te.largeHash;
    }

    if (modified) {
        diffEntry.type = TreeDiffEntry::Modified;
        diffEntry.newFilename = fullPath;
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    return 0;
}

void
TreeDiff::diffToDir(Tree src, const std::string &dir, Repo *r)
{
    set<string> wd_paths;
    Tree::Flat flattened_tree = src.flattened(r);
    /*for (Tree::Flat::iterator it = flattened_tree.begin();
            it != flattened_tree.end();
            it++) {
        printf("%s\n", (*it).first.c_str());
    }*/

    size_t dir_size = dir.size();
    if (dir[dir_size-1] == '/')
        dir_size--;

    _scanHelperData sd = {
        &wd_paths,
        &flattened_tree,
        this,
        dir_size,
        r};

    // Find additions and modifications
    Scan_RTraverse(dir.c_str(), &sd, _diffToDirHelper);

    // Find deletions
    for (map<string, TreeEntry>::iterator it = flattened_tree.begin();
            it != flattened_tree.end();
            it++) {
        set<string>::iterator wd_it = wd_paths.find((*it).first);
        if (wd_it == wd_paths.end()) {
            TreeDiffEntry tde;
            tde.filepath = (*it).first;
            tde.type = TreeDiffEntry::Deleted;
            entries.push_back(tde);
        }
    }
}


Tree
TreeDiff::applyTo(Tree::Flat flat, Repo *dest_repo)
{
    for (size_t i = 0; i < entries.size(); i++) {
        const TreeDiffEntry &tde = entries[i];
        printf("Applying %c   %s\n", tde.type, tde.filepath.c_str());
        if (tde.type == TreeDiffEntry::NewFile) {
            TreeEntry te;
            pair<string, string> hashes = dest_repo->addFile(tde.newFilename);
            te.hash = hashes.first;
            te.largeHash = hashes.second;
            te.type = (hashes.second != "") ? TreeEntry::LargeBlob :
                TreeEntry::Blob;
            // TODO te.mode

            flat.insert(make_pair(tde.filepath, te));
        }
        else if (tde.type == TreeDiffEntry::NewDir) {
            TreeEntry te;
            te.type = TreeEntry::Tree;
            flat.insert(make_pair(tde.filepath, te));
        }
        else if (tde.type == TreeDiffEntry::Deleted) {
            flat.erase(tde.filepath);
        }
        else if (tde.type == TreeDiffEntry::Modified) {
            TreeEntry te;
            pair<string, string> hashes = dest_repo->addFile(tde.newFilename);
            te.hash = hashes.first;
            te.largeHash = hashes.second;
            te.type = (hashes.second != "") ? TreeEntry::LargeBlob :
                TreeEntry::Blob;
            // TODO te.mode

            flat[tde.filepath] = te;
        }
	/* else if (tde.type == TreeDiffEntry::ModifiedDiff) {
	    // TODO: apply diff
            NOT_IMPLEMENTED(false);
        } */
        else {
            assert(false);
        }
    }

    /*for (Tree::Flat::iterator it = flat.begin();
            it != flat.end();
            it++) {
        printf("%s %s\n", (*it).first.c_str(), (*it).second.hash.c_str());
    }*/

    Tree rval = Tree::unflatten(flat, dest_repo);
    dest_repo->addBlob(Object::Tree, rval.getBlob());
    return rval;
}
