/* -*- coding: utf-8 c-basic-offset: 4 tab-width: 4 indent-tabs-mode: nil -*-
 * vi: set et ts=4 sw=4: */
/** \file */
#define {#TEMPLATE}_VERSION "0.1a"
/** \mainpage
 * See the example in the libuv book:
 *  http://docs.libuv.org/en/v1.x/guide/networking.html#tcp
 *  https://github.com/nikhilm/uvbook/blob/master/code/tcp-echo-server/main.c
 * To compile this, uncomment these Makefile lines:
 *  include Makefile.libuv.mk
 *  MAIN=src/main.libuv.c
 *
 * {#Template} 0.1a
 *
 * Just an example of using Cedro together with libuv,
 * that you can use as base for new programs.
 *
 * Modify this text to match your program, add your name to the
 * copyright statement.
 * This project template is distributed under the MIT license,
 * so you can change it to any license you want.
 *
 * It includes my customized hash table library derived from
 * [khash](http://attractivechaos.github.io/klib/#About),
 * and also Josh Baker’s [btree.c](https://github.com/tidwall/btree.c).
 *
 *   - [main](main_8c.html)
 *   - [hash-table](hash-table_8h.html)
 *   - [btree](btree_8c.html)
 *
 * TODO: put license here, for instance GPL or MIT.
 * You can get help picking one at: https://choosealicense.com/
 *
 * The C logo ([doc/logo.png](logo.png)) was made by
 * [Alberto González Palomo](https://sentido-labs.com).
 *
 * The copy of Cedro under `tools/cedro/` is under the Apache License 2.0,
 * and anyway it does not affect your code if you do not include or link it
 * into your program.
 * Here it is used only to process the source code, and the result
 * is of course not a derivative work of Cedro.
 *
 * \author {#Author}
 * \copyright {#year} {#Author}
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef __UINT8_C
#include <stdint.h>
#else
/* _SYS_INT_TYPES_H: Solaris 8, gcc 3.3.6 */
#ifndef _SYS_INT_TYPES_H
typedef unsigned char uint8_t;
#endif
#endif

#include <stdbool.h>
#include <string.h>
#define mem_eq(a, b, len)  (0 == memcmp(a, b, len))
#define str_eq(a, b)       (0 == strcmp(a, b))
#define strn_eq(a, b, len) (0 == strncmp(a, b, len))
#include <assert.h>
#include <errno.h>

/* “pthread_rwlock_t undefined on unix platform”
 * https://github.com/libuv/libuv/issues/1160
 */
#include <pthread.h>
#include <uv.h>

#pragma Cedro 1.0

#include "eprintln.h"
#define LANG(es, en) (strncmp(getenv("LANG"), "es", 2) == 0? es: en)

/** Expand to the mutable and constant variants for a `typedef`. \n
 * The default, just the type name `T`, is the constant variant
 * and the mutable variant is named `mut_T`, with corresponding \n
 * `T_p`: constant pointer to constant `T`       \n
 * `mut_T_p`: constant pointer to mutable `T`    \n
 * `mut_T_mut_p`: mutable pointer to mutable `T` \n
 * This mimics the usage in Rust, where constant bindings are the default
 * which is a good idea.
 */
#define { MUT_CONST_TYPE_VARIANTS(T)
/*  */      mut_##T,
    *       mut_##T##_mut_p,
    * const mut_##T##_p;
typedef const mut_##T T,
    *                 T##_mut_p,
    * const           T##_p;
#define }; // The semicolon here erases the one at the end of the previous line.

#include <ctl/str.h>
// Some extras for ctl/str by Alberto González Palomo https://sentido-labs.com
#include <time.h>
/** Append a date formatted as specificed. There is no default format.
 * https://en.cppreference.com/w/c/chrono/strftime
 * `"%Y-%m-%d"` ISO 8601 date. (C99 alternative: `"%F"`)
 * `"%H:%M:%S"` ISO 8601 time. (C99 alternative: `"%T"`)
 * `struct tm { .tm_year, .tm_mon, .tm_mday, .tm_hour, .tm_min, .tm_sec }`
 */
void
str_append_date(str* self, const char* format, const struct tm* time)
{
    char buffer[32];
    strftime(buffer, 32, format, time);
    buffer[sizeof(buffer)-1] = '\0';
    str_append(self, buffer);
}
/** Append an integer formatted as specified.
 * `"%d"` decimal, `"%X"`/`"%x"` hexadecimal,
 * `"%04d"` decimal padded with zeros to 4 digits.
 */
void
str_append_int(str* self, const char* format, const int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), format, value);
    buffer[sizeof(buffer)-1] = '\0';
    str_append(self, buffer);
}
/** Append a double formatted as specified.
 * `"%f"` as many digits as needed,
 * `"%04.2f"` integer part padded with zeros to 4 digits, 2 decimals.
 */
void
str_append_double(str* self, const char* format, const double value)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), format, value);
    buffer[sizeof(buffer)-1] = '\0';
    str_append(self, buffer);
}

uv_loop_t *loop;
struct sockaddr_in addr;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void
alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void
res_closed(uv_handle_t* handle)
{
    eprintln("close:  %p", (void*)handle);
    free(handle);
}

void
client_closed(uv_handle_t* client)
{
    eprintln("Freeing client %p", (void*)client);
    free(client);
}

void
res_written(uv_write_t *req, int status)
{
    // how? free(buf->base);
    if (status) {
        eprintln("Write error %s", uv_strerror(status));
    }
    free(((write_req_t*)req)->buf.base);
    ((write_req_t*)req)->buf.base = NULL;
    uv_close((uv_handle_t*)req->handle, res_closed);
    //eprintln("req: %p", (void*)req);
    free(req);
}

// https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Response_fields
#foreach { HEADERS {{                                                   \
    {Accept_CH, "Accept-CH: "},                                         \
    {Access_Control_Allow_Origin, "Access-Control-Allow-Origin: "},     \
    {Access_Control_Allow_Credentials, "Access-Control-Allow-Credentials: "}, \
    {Access_Control_Expose_Headers, "Access-Control-Expose-Headers: "}, \
    {Access_Control_Max_Age, "Access-Control-Max-Age: "},               \
    {Access_Control_Allow_Methods, "Access-Control-Allow-Methods: "},   \
    {Access_Control_Allow_Headers, "Access-Control-Allow-Headers: "},   \
    {Accept_Patch, "Accept-Patch: "},                                   \
    {Accept_Ranges, "Accept-Ranges: "},                                 \
    {Age, "Age: "},                                                     \
    {Allow, "Allow: "},                                                 \
    {Alt_Svc, "Alt-Svc: "},                                             \
    {Cache_Control, "Cache-Control: "},                                 \
    {Connection, "Connection: "},                                       \
    {Content_Disposition, "Content-Disposition: "},                     \
    {Content_Encoding, "Content-Encoding: "},                           \
    {Content_Language, "Content-Language: "},                           \
    {Content_Length, "Content-Length: "},                               \
    {Content_Location, "Content-Location: "},                           \
    {Content_MD5, "Content-MD5: "},                                     \
    {Content_Range, "Content-Range: "},                                 \
    {Content_Type, "Content-Type: "},                                   \
    {Date, "Date: "},                                                   \
    {Delta_Base, "Delta-Base: "},                                       \
    {ETag, "ETag: "},                                                   \
    {Expires, "Expires: "},                                             \
    {IM, "IM: "},                                                       \
    {Last_Modified, "Last-Modified: "},                                 \
    {Link, "Link: "},                                                   \
    {Location, "Location: "},                                           \
    {P3P, "P3P: "},                                                     \
    {Pragma, "Pragma: "},                                               \
    {Preference_Applied, "Preference-Applied: "},                       \
    {Proxy_Authenticate, "Proxy-Authenticate: "},                       \
    {Public_Key_Pins, "Public-Key-Pins: "},                             \
    {Retry_After, "Retry-After: "},                                     \
    {Server, "Server: "},                                               \
    {Set_Cookie, "Set-Cookie: "},                                       \
    {Strict_Transport_Security, "Strict-Transport-Security: "},         \
    {Trailer, "Trailer: "},                                             \
    {Transfer_Encoding, "Transfer-Encoding: "},                         \
    {Tk, "Tk: "},                                                       \
    {Upgrade, "Upgrade: "},                                             \
    {Vary, "Vary: "},                                                   \
    {Via, "Via: "},                                                     \
    {Warning, "Warning: "},                                             \
    {WWW_Authenticate, "WWW-Authenticate: "},                           \
    {X_Frame_Options, "X-Frame-Options: "}                              \
    }}
typedef struct HttpResponse {
    const char* status;
#foreach { {H, S} HEADERS
    const char* H;
#foreach }
    const char* body;
} MUT_CONST_TYPE_VARIANTS(HttpResponse);
void
format_http_response(HttpResponse_p res, str* response)
{
    str_append(response, "HTTP/1.1 ");
    str_append(response, res->status? res->status: "500");
    str_append(response, " OK\r\n");
#foreach { {H, S} HEADERS
    if (res->H) {
        str_append(response, S);
        str_append(response, res->H);
        str_append(response, "\r\n");
    }
#foreach }
    if (res->body) {
        char size_string[16];
        snprintf(size_string, sizeof(size_string), "%d", strlen(res->body));
        str_append(response, "Content-Length: ");
        str_append(response, size_string);
        str_append(response, "\r\n\r\n");
        str_append(response, res->body);
    } else {
        str_append(response, "\r\n");
    }
}
#foreach }

static const char MIME_TYPE_HTML[] = "text/html; charset=utf-8";

void
on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
    if (nread > 0) {
        /* This will print to the console the raw request fragment:
        char* log_message = (char*) malloc(nread + 1);
        auto free(log_message);
        memcpy(log_message, buf->base, nread);
        log_message[nread] = '\0';
        eprintln("Received: %s\n-----------", log_message);

        More concisely:
        eprint("Received: ");
        fwrite(stderr, buf->base, nread); fputc('\n', stderr);
        eprintln("\n-----------");
        */

        str body = str_init("<!DOCTYPE html>\n");
        auto free(body.value);
        static unsigned int request_count = 0;
        time_t right_now = time(NULL);
        struct tm today = *localtime(&right_now);
        &body @ str_append...
                ("<html lang='en'>"),
                ("\n  <head>"),
                ("\n    <meta charset='utf-8'/>"),
                ("\n    <style type='text/css'>"
                 "\n      body { background:#ccc; }"
                 "\n      .visit-counter { font-family:monospace; font-size:120%; }"
                 "\n    </style>"),
                ("\n  </head>"),
                ("\n  <body>"),
                ("\n    Hello from <a href='https://sentido-labs.com'>"),
                ("Sentido® Labs"),
                ("</a>"),
                _date(", today is the %Y-%m-%d", &today),
                _date(" at %H:%M:%S.", &today),
                ("\n    <div>Visit count: <span class='visit-counter'>"),
                _int("%06d", ++request_count),
                ("</span></div>"),
                ("\n  </body>"),
                ("\n</html>");

        str content_length = str_init("");
        str_append_int(&content_length, "%d", body.size);
        HttpResponse response = {
            .status = "200",
            .Content_Type = MIME_TYPE_HTML,
            .Content_Length = str_c_str(&content_length),
            //.Connection = "close", // Useful if body size is not known.
            .body = str_c_str(&body)
        };
        str response_str = str_init("");
        format_http_response(&response, &response_str);
        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(str_c_str(&response_str), response_str.size);
        // In some cases, as the requests pile up, read is not called.
        eprintln("read:   %p", (void*)client);
        uv_write((uv_write_t*) req, client, &req->buf, 1, res_written);
        /* response.data/str_c_str(&response) freed at res_written(). */
        //return;
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            eprintln("Read error %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t*) client, NULL);
    }

    free(buf->base);
}

void
on_new_connection(uv_stream_t *server, int status)
{
    if (status < 0) {
        eprintln("New connection error %s", uv_strerror(status));
        // error!
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    eprintln("client: %p", (void*)client);
    uv_tcp_init(loop, client);
    // Wait five seconds, then check whether the connection is still there:
    uv_tcp_keepalive(client, true, 5);
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, alloc_buffer, on_read);
    } else {
        eprintln("--- accept failed");
        uv_close((uv_handle_t*) client, client_closed);
    }
}

typedef struct WatchdogInstructions {
    const char* message;
    unsigned int wait_ms;
} MUT_CONST_TYPE_VARIANTS(WatchdogInstructions);

void
watchdog(void *arg)
{
    WatchdogInstructions_p instructions = arg;
    eprintln("Message for watchdog: %s", instructions->message);
    uv_sleep(instructions->wait_ms);
    eprintln("Awakened!");
    uv_stop(loop);
}

void
on_handle_closed(uv_handle_t *handle)
{
    eprintln("Closed handle >%p<", (void*)handle);
}
void
close_each_handle(uv_handle_t *handle, void *arg)
{
    eprintln(">>> close each handle: %s %p",
             uv_handle_type_name(uv_handle_get_type(handle)), (void*)handle);
    uv_close(handle, on_handle_closed);
}

void
on_sigint(uv_signal_t *handle, int signum)
{
    uv_stop(loop);
}


const char* const usage_es =
        "Uso: {#template} [opciones]\n"
        "  Sirve la fecha actual y la cuenta de visitas como HTML.\n"
        "  --address=0.0.0.0 IP en la que escuchar las peticiones.\n"
        "  --port=7000       Puerto.\n"
        "  --exit-after-brief-delay Sale tras 100ms, para pruebas.\n"
        ;
const char* const usage_en =
        "Usage: {#template} [options]\n"
        "  Serves the current date and visit count as HTML."
        "  --address=0.0.0.0 IP at which to listen for queries.\n"
        "  --port=7000       Port.\n"
        "  --exit-after-brief-delay Exit after 100ms, for testing.\n"
        ;

typedef struct Options {
    const char*  host;
    unsigned int port;
    int          backlog;
    bool         exit_after_brief_delay;
} MUT_CONST_TYPE_VARIANTS(Options);

int
main(int argc, char* argv[])
{
    int err = 0;

    mut_Options options = {
        .host = "0.0.0.0", .port = 7000,
        .backlog = 128,
        .exit_after_brief_delay = false
    };

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (str_eq("-h", arg) || str_eq("--help", arg)) {
            eprintln("{#Template} v%s, libuv v%s",
                     {#TEMPLATE}_VERSION,
                     uv_version_string());
            eprint(LANG(usage_es, usage_en));
            return err;
        } else if (str_eq("--exit-after-brief-delay", arg)) {
            options.exit_after_brief_delay = true;
        } else if (strn_eq("--address=", arg, strlen("--address="))) {
            options.host = &arg[strlen("--address=")];
        } else if (strn_eq("--port=",    arg, strlen("--port="))) {
            char* end;
            options.port = strtol(&arg[strlen("--port=")], &end, 10);
            if (end != arg + strlen(arg) ||
                options.port < 0 || options.port > 65535) {
                eprintln(LANG("El número de puerto debe ser un entero"
                              " entre 0 y 65535",
                              "Port number must be an integer"
                              " between 0 and 65535."));
                return 11;
            }
        }
    }

    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    uv_ip4_addr(options.host, options.port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    err = uv_listen((uv_stream_t*)&server, options.backlog, on_new_connection);
    if (err) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(err));
        return err;
    }

    eprintln("Listening on http://%s:%u", options.host, options.port);

    if (options.exit_after_brief_delay) {
        // This is so that the Valgrind test finishes without having to
        // interrupt the server:
        // http://docs.libuv.org/en/v1.x/guide/threads.html
        WatchdogInstructions instructions = { "Good doggy!", 100/*ms*/ };
        @uv_thread_...
                t watchdog_id,
                create(&watchdog_id, watchdog, (void*)&instructions),
                join(&watchdog_id);
    }

    // Finish the server nicely when interrupted:
    uv_signal_t sigint;
    uv_signal_init(loop, &sigint);
    uv_signal_start(&sigint, on_sigint, SIGINT);

    err = uv_run(loop, UV_RUN_DEFAULT);

    eprintln("Closing event loop...");
    while ((err = uv_loop_close(loop))) {
        if (err == UV_EBUSY) {
            eprintln("Not closed yet, server busy.");
        } else {
            eprintln("Not closed yet, result is %d.", err);
        }
        uv_walk(loop, close_each_handle, NULL);
        uv_run(loop, UV_RUN_NOWAIT);
    }
    eprintln("Event loop closed.");

    return err;
}
