/*
 * Copyright (c) 2012-2013 Stanford University
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <resolv.h>

#include <string>
#include <iostream>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/zeroconf.h>
#include <ori/version.h>
#include <ori/localrepo.h>

#include "evbufstream.h"
#include "httpd.h"

using namespace std;

static void
OriHttpdLogCB(int severity, const char *msg)
{
#ifdef DEBUG
    const char *sev[] = {
        "Debug", "Msg", "Warning", "Error",
    };
    LOG("%s: %s", sev[severity], msg);
#endif
}

void
OriHttpdReqHandlerCB(struct evhttp_request *req, void *arg)
{
    OriHttpd *httpd = (OriHttpd *)arg;

    httpd->entry(req);

    return;
}

OriHttpd::OriHttpd(LocalRepo &repository, uint16_t port)
    : repo(repository), httpd(NULL), base(NULL)
{
    base = event_base_new();
    event_set_log_callback(OriHttpdLogCB);

    httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, "0.0.0.0", port);

    evhttp_set_gencb(httpd, OriHttpdReqHandlerCB, this);
}

OriHttpd::~OriHttpd()
{
    evhttp_free(httpd);
    event_base_free(base);
}

void
OriHttpd::start()
{
#if !defined(WITHOUT_MDNS)
    // mDNS
    MDNS_Register(port);
#endif

    event_base_dispatch(base);
}

void
OriHttpd::entry(struct evhttp_request *req)
{
    string url = evhttp_request_get_uri(req);

    /*
     * HTTP Paths
     * /stop - Debug Only
     * /id - Repository id
     * /version - Repository version
     * /HEAD - HEAD revision
     * /index
     * /commits
     * /contains
     * /getobjs
     * /objs/...
     * /objinfo/...
     */

#ifdef DEBUG
    if (url == "/stop") {
        stop(req);
    } else
#endif
    if (url == "/id") {
        getId(req);
    } else if (url == "/version") {
        getVersion(req);
    } else if (url == "/HEAD") {
        head(req);
    } else if (url == "/index") {
        getIndex(req);
    } else if (url == "/commits") {
        getCommits(req);
    } else if (url == "/contains") {
        contains(req);
    } else if (url == "/getobjs") {
        getObjs(req);
    } else if (StrUtil_StartsWith(url, "/objs/")) {
        evhttp_send_error(req, HTTP_NOTFOUND, "File Not Found");
        return;
    } else if (StrUtil_StartsWith(url, "/objinfo/")) {
        getObjInfo(req);
    } else {
        evhttp_send_error(req, HTTP_NOTFOUND, "File Not Found");
        return;
    }
}

int
OriHttpd::authenticate(struct evhttp_request *req, struct evbuffer *buf)
{
    int n;
    char output[64];
    const char *as = evhttp_find_header(req->input_headers, "Authorization");
    if (as == NULL) {
	goto authFailed;
    }

    // Select auth type

    n = b64_pton(strchr(as,' ') + 1, (u_char *) output, 64);
    output[n] = '\0';
    if (strcmp(output, "ali:test") != 0) {
        goto authFailed;
    }
    return 0;

authFailed:
    evhttp_add_header(req->output_headers, "WWW-Authenticate",
                      "Basic realm=\"Secure Area\"");
    evhttp_send_reply(req, 401, "Authorization Required", buf);
    return -1;
}

void
OriHttpd::stop(struct evhttp_request *req)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    if (authenticate(req, buf) < 0)
    {
        return;
    }

    evbuffer_add_printf(buf, "Stopping\n");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    event_base_loopexit(base, NULL);
}

void
OriHttpd::getId(struct evhttp_request *req)
{
    string repoId = repo.getUUID();
    struct evbuffer *buf;

    LOG("httpd: getid");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", repoId.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
OriHttpd::getVersion(struct evhttp_request *req)
{
    string ver = repo.getVersion();
    struct evbuffer *buf;

    LOG("httpd: getversion");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", ver.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
OriHttpd::head(struct evhttp_request *req)
{
    ObjectHash headId = repo.getHead();
    ASSERT(!headId.isEmpty());
    struct evbuffer *buf;

    LOG("httpd: gethead");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", headId.hex().c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
OriHttpd::getIndex(struct evhttp_request *req)
{
    LOG("httpd: getindex");

    evbufwstream es;

    std::set<ObjectInfo> objects = repo.listObjects();
    es.writeUInt64(objects.size());
    for (std::set<ObjectInfo>::iterator it = objects.begin();
            it != objects.end();
            it++) {
        int status = es.writeInfo(*it);
        if (status < 0) {
            LOG("couldn't write info to evbuffer!");
            evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
            return;
        }
    }

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

void
OriHttpd::getCommits(struct evhttp_request *req)
{
    LOG("httpd: getCommits");

    evbufwstream es;

    vector<Commit> commits = repo.listCommits();
    es.writeUInt32(commits.size());
    for (size_t i = 0; i < commits.size(); i++) {
        std::string blob = commits[i].getBlob();
        if (es.writePStr(blob) < 0) {
            LOG("couldn't write pstr to evbuffer");
            evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
            return;
        }
    }

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

void
OriHttpd::contains(struct evhttp_request *req)
{
    // Get object hashes
    evbuffer *buf = evhttp_request_get_input_buffer(req);
    evbufstream in(buf);
    evbufwstream out;

    uint32_t numObjs = in.readUInt32();
    fprintf(stderr, "Transmitting %u objects\n", numObjs);
    string rval = "";
    for (size_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);

        if (repo.hasObject(hash))
            rval += "P"; // Present
        else
            rval += "N"; // Not Present
        // XXX: Remote later show other states
    }

    out.write(rval.data(), rval.size());

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", out.buf());
}

void
OriHttpd::getObjs(struct evhttp_request *req)
{
    // Get object hashes
    evbuffer *buf = evhttp_request_get_input_buffer(req);
    evbufstream in(buf);

    uint32_t numObjs = in.readUInt32();
    fprintf(stderr, "Transmitting %u objects\n", numObjs);
    std::vector<ObjectHash> objs;
    for (size_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);
        objs.push_back(hash);
    }


    // Transmit
    evbufwstream out;
    repo.transmit(&out, objs);

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", out.buf());
}

void
OriHttpd::getObjInfo(struct evhttp_request *req)
{
    string url = evhttp_request_get_uri(req);
    string sObjId;
    ASSERT(url.substr(0, 9) == "/objinfo/");
    
    sObjId = url.substr(9);
    if (sObjId.size() != 64) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
        return;
    }
    
    LOG("httpd: getobjinfo %s", sObjId.c_str());

    Object::sp obj = repo.getObject(ObjectHash::fromHex(sObjId));
    if (obj == NULL) {
        evhttp_send_reply(req, HTTP_NOTFOUND, "Object Not Found", evbuffer_new());
        return;
    }

    ObjectInfo objInfo = obj->getInfo();

    // Transmit
    evbufwstream es;
    es.writeInfo(objInfo);

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

// void
// OriHttpd::pushObj(struct evhttp_request *req, void *arg)
// {
//     const char *cl = evhttp_find_header(req->input_headers, "Content-Length");
//     uint32_t length;
//     struct evbuffer *body = evhttp_request_get_input_buffer(req);
//     struct evbuffer *outbuf;
//     unsigned char objId[20];
//     void *buf;
// 
//     outbuf = evbuffer_new();
//     if (outbuf == NULL) {
//         LOG("couldn't allocate evbuffer!");
//         evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
//         return;
//     }
// 
//     // Make sure it makes sense
//     if (cl == NULL) {
//         evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
//         return;
//     }
// 
//     /*
//     Dump Headers
// 
//     struct evkeyvalq *kv = evhttp_request_get_input_headers(req);
//     struct evkeyval *iter;
//     TAILQ_FOREACH(iter, kv, next) {
//         printf("%s: %s\n", iter->key, iter->value);
//     }
//     */
// 
//     length = atoi(cl);
//     printf("Push: %d bytes, got %zd bytes\n", length, evbuffer_get_length(body));
// 
//     buf = malloc(length);
//     evbuffer_remove(body, buf, length);
//     // XXX: Add object
//     free(buf);
// 
//     evbuffer_add_printf(outbuf, "%02x%02x%02x%02x"
//                         "%02x%02x%02x%02x%02x%02x%02x%02x"
//                         "%02x%02x%02x%02x%02x%02x%02x%02x\n",
//                         objId[0], objId[1], objId[2], objId[3],
//                         objId[4], objId[5], objId[6], objId[7],
//                         objId[8], objId[9], objId[10], objId[11],
//                         objId[12], objId[13], objId[14], objId[15],
//                         objId[16], objId[17], objId[18], objId[19]);
//     evhttp_send_reply(req, HTTP_OK, "OK", outbuf);
// }
 
