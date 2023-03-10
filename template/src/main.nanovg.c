/* -*- coding: utf-8 c-basic-offset: 4 tab-width: 4 indent-tabs-mode: nil -*-
 * vi: set et ts=4 sw=4: */
/** \file */
#define {#TEMPLATE}_VERSION "0.1a"
/** \mainpage
 * See nanovg’s example_gl3.c:
 *  https://github.com/memononen/nanovg/blob/master/example/example_gl3.c
 *  http://docs.libuv.org/en/v1.x/guide/networking.html#tcp
 *  https://github.com/nikhilm/uvbook/blob/master/code/tcp-echo-server/main.c
 * To compile this, uncomment these Makefile lines:
 *   include Makefile.nanovg.mk
 *   MAIN=src/main.nanovg.c
 *
 * {#Template} 0.1a
 *
 * Just an example of using Cedro together with nanovg,
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

#ifdef NANOVG_GLEW
#	include <GL/glew.h>
#endif
#ifdef __APPLE__
#	define GLFW_INCLUDE_GLCOREARB
#endif
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>
#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <stb_image.h> // Comes from lib/nanovg.

#include <math.h>

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

typedef enum Error {
    @ERROR_...
    NONE,
    INIT_FAIL_GLFW, INIT_FAIL_GLEW, INIT_FAIL_NANOVG, OPEN_FAIL_WINDOW
} MUT_CONST_TYPE_VARIANTS(Error);

Error print_error(Error code)
{
    switch (code) {
        case ERROR_NONE: eprintln("OK"); break;
        case ERROR_INIT_FAIL_GLFW:
            eprintln("Failed to initialize GLFW.");
            break;
        case ERROR_INIT_FAIL_GLEW:
            eprintln("Failed to initialize GLEW.");
            break;
        case ERROR_INIT_FAIL_NANOVG:
            eprintln("Failed to initialize NanoVG.");
            break;
        case ERROR_OPEN_FAIL_WINDOW:
            eprintln("Failed to open Window.");
            break;
    }
    return code;
}

void glfw_error(int error, const char* desc)
{
    eprintln("GLFW error %.8X: %s", error, desc);
}

extern const uint8_t font_sans[]; extern const size_t sizeof_font_sans;
extern const uint8_t icon[];      extern const size_t sizeof_icon;


const char* const usage_es =
        "Uso: {#template} [opciones]\n"
        "  Muestra un paisaje minimalista iluminado por un sol múltiple.\n"
        "  --no-vsync       Deactiva vsync. (inicialmente activado)\n"
        "  --adaptive-vsync Activa el vsync adaptivo si está disponible.\n"
        "  --exit-after-brief-delay Sale tras 100ms, para pruebas.\n"
        "\n En tarjetas gráficas AMD"
        " se pueden activar los indicadores de rendimiento:\n"
        " GALLIUM_HUD=fps,VRAM-usage {#template}\n"
        ;
const char* const usage_en =
        "Usage: {#template} [options]\n"
        "  --no-vsync       Disable vsync. (default is enabled)\n"
        "  --adaptive-vsync Enable adaptive vsync if available.\n"
        "  --exit-after-brief-delay Exit after 1 cycle, for testing.\n"
        "\n On AMD graphic cards"
        " you can enable the performance overlay:\n"
        " GALLIUM_HUD=fps,VRAM-usage {#template}\n"
        ;

typedef struct Options {
    int  vsync; ///< 1: v-sync, 0: no v-sync, -1: adaptive v-sync.
    bool exit_after_brief_delay;
} MUT_CONST_TYPE_VARIANTS(Options);

int
main(int argc, char* argv[])
{
    int err = 0;

    mut_Options options = {
        .vsync = 1
    };

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (str_eq("-h", arg) || str_eq("--help", arg)) {
            eprintln("{#Template} v%s, nanovg v%s, glew v%d.%d.%d, glfw %s",
                     {#TEMPLATE}_VERSION,
                     NANOVG_VERSION,
                     GLEW_VERSION_MAJOR, GLEW_VERSION_MINOR, GLEW_VERSION_MICRO,
                     glfwGetVersionString());
            eprint(LANG(usage_es, usage_en));
            return err;
        } else if (str_eq("--exit-after-brief-delay", arg)) {
            options.exit_after_brief_delay = true;
        } else if (str_eq("--no-vsync", arg)) {
            options.vsync = 0;
        } else if (str_eq("--adaptive-vsync", arg)) {
            options.vsync = -1;
        } else {
            eprintln("Unknown option %s", arg);
            err = 1;
            return err;
        }
    }

    if (!glfwInit()) {
        err = print_error(ERROR_INIT_FAIL_GLFW);
        return err;
    }
    auto glfwTerminate();

    glfwSetErrorCallback(glfw_error);
#ifndef _WIN32 // don't require this on win32, and works with more cards
    @glfwWindowHint...
            (GLFW_CONTEXT_VERSION_MAJOR, 3),
            (GLFW_CONTEXT_VERSION_MINOR, 2),
            (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE),
            (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
    //glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1000, 600, "NanoVG", NULL, NULL);
    if (!window) {
        err = print_error(ERROR_OPEN_FAIL_WINDOW);
        return err;
    }
    auto glfwDestroyWindow(window);
    {
        GLFWimage icons[1];
        icons[0].pixels =
                stbi_load_from_memory(icon, sizeof_icon,
                                      &icons[0].width, &icons[0].height, 0, 4);
        glfwSetWindowIcon(window, 1, icons);
        stbi_image_free(icons[0].pixels);
    }
    glfwMakeContextCurrent(window);
#ifdef NANOVG_GLEW
    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        err = print_error(ERROR_INIT_FAIL_GLEW);
        return err;
    }
    // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS),
    // we'll consume it here.
    glGetError();
#endif

    struct NVGcontext* vg =
            nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!vg) {
        err = print_error(ERROR_INIT_FAIL_NANOVG);
        return err;
    }
    auto nvgDeleteGL3(vg);

    glfwSwapInterval(options.vsync);

    int fontSans =
            nvgCreateFontMem(vg, "sans",
                             (unsigned char*) font_sans, sizeof_font_sans,
                             false);

    char text_buffer[300] = {0};
    const double π = 3.1415926535;
    const double rad_to_deg = 180.0 / π;

    while (!glfwWindowShouldClose(window))
    {
        double mx, my;
        int winWidth, winHeight;
        int fbWidth, fbHeight;
        double pxRatio;

        window @glfw...
                GetWindowSize(&winWidth, &winHeight),
                GetFramebufferSize(&fbWidth, &fbHeight);
        pxRatio = (double)fbWidth / (double)winWidth;

        @gl...
                Viewport(0, 0, fbWidth, fbHeight),
                ClearColor(0.0f, 0.0f, 0.0f, 1.0f),
                Clear(GL_COLOR_BUFFER_BIT  |
                      GL_DEPTH_BUFFER_BIT  |
                      GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        vg @nvg... BeginFrame(winWidth, winHeight, pxRatio);

        /* Calling glFinish() before polling the events reduces latency
           because we get the values after all pending GL operations
           are done, closer to the time when we swap buffers. */
        glFinish();

        double t = glfwGetTime();
        glfwPollEvents();
        window @glfw... GetCursorPos(&mx, &my);

        const double r = 100;
        const double period = 0.5/*s*/;
        double
                alpha_x = (double)t / period,
                beta_x  = (double)t / (0.7*period) - π*2.0/3.0,
                theta_x = (double)t / (0.9*period) - π*4.0/3.0,
                alpha_y = (double)t / period,
                beta_y  = (double)t / period - π*2.0/3.0,
                theta_y = (double)t / period - π*4.0/3.0;
        double sat_x[3] = { r * cos@... (alpha_x), (beta_x), (theta_x) };
        double sat_y[3] = { r * sin@... (alpha_y), (beta_y), (theta_y) };

        double horizon = 2*fbHeight/3;
        double luma = fmin(1.0, fmax(0.0, 1.0 - my/fbHeight));
        NVGpaint sky =
                vg @nvg...
                LinearGradient(0, 0, 0, horizon,
                               @nvg...
                               HSLA(0.60, 1.00, 0.20*luma, 255),
                               HSLA(0.60, 1.00, 0.90*luma, 255));
        NVGpaint ground =
                vg @nvg...
                LinearGradient(0, horizon, 0, fbHeight,
                               @nvg...
                               HSLA(0.34, 0.73, 0.20*luma, 255),
                               HSLA(0.34, 0.73, 0.67*luma, 255));

        vg @nvg...
                Save(),
                BeginPath(),
                Rect(0,0, fbWidth,horizon),
                FillPaint(sky),
                Fill(),
                BeginPath(),
                Circle(mx,my, 50),
                Circle(mx + sat_x[0], my + sat_y[0], 10),
                Circle(mx + sat_x[1], my + sat_y[1], 15),
                Circle(mx + sat_x[2], my + sat_y[2], 20),
                PathWinding(NVG_HOLE),
                StrokeColor(nvgRGBA(240, 120, 10, 127)),
                StrokeWidth(5),
                Stroke(),
                FillColor(nvgRGBA(255, 192, 0, 255)),
                Fill(),
                BeginPath(),
                Rect(0,horizon, fbWidth,fbHeight-horizon),
                FillPaint(ground),
                Fill(),
                FontSize(20.0f),
                FontFace("sans"),
                TextAlign(NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE),
                FillColor(nvgRGBA(230, 230, 230, 255)),
                Text(10, fbHeight - 40,
                     (snprintf(text_buffer, sizeof(text_buffer),
                               "X: %4.0f  α: %6.2f  β: %6.2f  θ: %6.2f"
                               "  luma: %1.2f",
                               mx,
                               /* Adding 360 avoids negative numbers. */
                               fmod(alpha_x * rad_to_deg + 360.0, 360.0),
                               fmod( beta_x * rad_to_deg + 360.0, 360.0),
                               fmod(theta_x * rad_to_deg + 360.0, 360.0), luma),
                      text_buffer), NULL),
                Text(10, fbHeight - 18,
                     (snprintf(text_buffer, sizeof(text_buffer),
                               "Y: %4.0f  α: %6.2f  β: %6.2f  θ: %6.2f",
                               my,
                               fmod(alpha_y * rad_to_deg + 360.0, 360.0),
                               fmod( beta_y * rad_to_deg + 360.0, 360.0),
                               fmod(theta_y * rad_to_deg + 360.0, 360.0)),
                      text_buffer), NULL),
                Restore();

        nvgEndFrame(vg);

        glfwMakeContextCurrent(window);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glfwSwapBuffers(window);

        if (options.exit_after_brief_delay) break;
    }

    return err;
}
