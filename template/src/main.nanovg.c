/** See nanovg’s example_gl3.c:
 * https://github.com/memononen/nanovg/blob/master/example/example_gl3.c
 *
 * To compile this, uncomment these Makefile lines:
 * include Makefile.nanovg.mk
 * MAIN=src/main.nanovg.c
 */

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>

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

#include <stb_image.h>

#include <assert.h>
#include <sys/resource.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#define str_eq(a, b)     (0 == strcmp(a, b))
#define strn_eq(a, b, n) (0 == strncmp(a, b, n))
#define mem_eq(a, b, n)  (0 == memcmp(a, b, n))

#pragma Cedro 1.0

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
    * const           T##_p
#define }

typedef enum Error {
  @ERROR_...
  NONE,
  INIT_FAIL_GLFW, INIT_FAIL_GLEW, INIT_FAIL_NANOVG, OPEN_FAIL_WINDOW
} MUT_CONST_TYPE_VARIANTS(Error);

Error print_error(Error code)
{
  switch (code) {
    case ERROR_NONE: fprintf(stderr, "OK\n"); break;
    case ERROR_INIT_FAIL_GLFW:
      fprintf(stderr, "Failed to initialize GLFW.\n");
      break;
    case ERROR_INIT_FAIL_GLEW:
      fprintf(stderr, "Failed to initialize GLEW.\n");
      break;
    case ERROR_INIT_FAIL_NANOVG:
      fprintf(stderr, "Failed to initialize NanoVG.\n");
      break;
    case ERROR_OPEN_FAIL_WINDOW:
      fprintf(stderr, "Failed to open Window.\n");
      break;
  }
  return code;
}

void glfw_error(int error, const char* desc)
{
  fprintf(stderr, "GLFW error %.8X: %s\n", error, desc);
}

extern const uint8_t font_sans[]; extern const size_t sizeof_font_sans;
extern const uint8_t icon[];      extern const size_t sizeof_icon;

int main(int argc, char* argv[])
{
  int err = 0;
  // Enable core dumps:
  struct rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
  assert(0 == setrlimit(RLIMIT_CORE, &core_limit));

  int vsync = 1; // 1: v-sync, 0: no v-sync, -1: adaptive v-sync.
  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (str_eq("-h", arg) || str_eq("--help", arg)) {
      fprintf(stderr, "Usage: %s [options]\n", argv[0]);
      fprintf(stderr,
              "  --no-vsync       Disable vsync. (default is enabled)\n"
              "  --adaptive-vsync Enable adaptive vsync if available.\n"
              "  --perf           On AMD graphic cards,"
              " enable the performance overlay.\n"
              "                   Same as: GALLIUM_HUD=fps,VRAM-usage %s\n",
              argv[0]
              );
      return err;
    } else if (str_eq("--no-vsync", arg)) {
      vsync = 0;
    } else if (str_eq("--adaptive-vsync", arg)) {
      vsync = -1;
    } else if (str_eq("--perf", arg)) {
      // Enable Gallium performance overlay:
      setenv("GALLIUM_HUD", "fps,VRAM-usage", 1);
    } else {
      fprintf(stderr, "Unknown option %s\n", arg);
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

  glfwSwapInterval(vsync);

  int fontSans =
      nvgCreateFontMem(vg, "sans",
                       (unsigned char*) font_sans, sizeof_font_sans,
                       false);

  struct timespec timespec;
  const double nsec_to_sec = 1.0 / 1000000000.0;

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
        Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vg @nvg... BeginFrame(winWidth, winHeight, pxRatio);

    glFinish();

    if (clock_gettime(CLOCK_MONOTONIC, &timespec) != 0) {
      fprintf(stderr, "FAILED TO READ SYSTEM CLOCK\n");
      break;
    }

    double t =
        (double)timespec.tv_sec +
        (double)timespec.tv_nsec * nsec_to_sec;

    const double r = 100;
    const double period = 0.5/*s*/;
    double
        alpha_x = (double)t / period,
        beta_x  = (double)t / (0.7*period) - π*2.0/3.0,
        theta_x = (double)t / (0.9*period) - π*4.0/3.0,
        alpha_y = (double)t / period,
        beta_y  = (double)t / period - π*2.0/3.0,
        theta_y = (double)t / period - π*4.0/3.0;
    double sat_x[3] = { r * cos(alpha_x), r * cos(beta_x), r * cos(theta_x) };
    double sat_y[3] = { r * sin(alpha_y), r * sin(beta_y), r * sin(theta_y) };

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

    glfwPollEvents();
    window @glfw... GetCursorPos(&mx, &my);

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
                       "X: %4.0f  α: %6.2f  β: %6.2f  θ: %6.2f luma: %1.2f",
                       mx,
                       fmod(alpha_x * rad_to_deg, 360.0),
                       fmod( beta_x * rad_to_deg, 360.0),
                       fmod(theta_x * rad_to_deg, 360.0), luma),
              text_buffer), NULL),
        Text(10, fbHeight - 18,
             (snprintf(text_buffer, sizeof(text_buffer),
                       "Y: %4.0f  α: %6.2f  β: %6.2f  θ: %6.2f",
                       my,
                       fmod(alpha_y * rad_to_deg, 360.0),
                       fmod( beta_y * rad_to_deg, 360.0),
                       fmod(theta_y * rad_to_deg, 360.0)),
              text_buffer), NULL),
        Restore();

    nvgEndFrame(vg);

    glfwMakeContextCurrent(window);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSwapBuffers(window);
  }

  return err;
}
