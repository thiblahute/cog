/*
 * cog-platform-sdl2.c
 * Copyright (C) 2018 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <SDL.h>
#include <SDL_egl.h>
#include <SDL_opengles2.h>
#include <cog.h>
#include <wpe/wpe.h>
#include <wpe/fdo.h>
#include <wpe/fdo-egl.h>


#define WINDOW_TITLE "Cog"
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 620


struct platform_window
{
    SDL_Window *sdl_window;
    SDL_GLContext sdl_glcontext;
    struct wpe_view_backend_exportable_fdo *exportable;
    WebKitWebViewBackend *view_backend;
    GSource *event_source;
};

static struct platform_window win = { .sdl_window = NULL, };


/*
 * This implements a GSource which fetches events from SDL, and handles
 * dispatching them to WPE as needed. This way the GLib main loop used
 * by WPE WebKit will automatically pull events from SDL for us.
 */

typedef gboolean (*SdlEventSourceFunc) (GSource*, const SDL_Event*, void*);

typedef struct {
    GSource   parent;
    SDL_Event sdl_event;
} SdlEventSource;

static gboolean
sdl_event_source_check (GSource* source)
{
    SdlEventSource *sdl_source = (SdlEventSource*) source;
    return SDL_PollEvent (&sdl_source->sdl_event) > 0;
}

static gboolean
sdl_event_source_dispatch (GSource    *source,
                           GSourceFunc callback,
                           void       *userdata)
{
    SdlEventSource *sdl_source = (SdlEventSource*) source;
    return (*(SdlEventSourceFunc) callback) (source,
                                             &sdl_source->sdl_event,
                                             userdata);
}

static GSourceFuncs s_sdl_event_source_funcs = {
    .check = sdl_event_source_check,
    .dispatch = sdl_event_source_dispatch,
};

static GSource*
sdl_event_source_new (void)
{
    SdlEventSource *sdl_source =
        (SdlEventSource*) g_source_new (&s_sdl_event_source_funcs,
                                        sizeof (SdlEventSource));
    memset (&sdl_source->sdl_event, 0x00, sizeof (SDL_Event));
    return (GSource*) sdl_source;
}



static bool
setup_sdl_opengles (struct platform_window* window, GError **error)
{
    g_assert_nonnull (window);
    g_assert_null (window->sdl_glcontext);

    if (!(window->sdl_glcontext = SDL_GL_CreateContext (window->sdl_window))) {
        g_set_error (error,
                     COG_PLATFORM_EGL_ERROR,  // XXX
                     0,
                     "Cannot create GL context: %s",
                     SDL_GetError ());
        return false;
    }

    g_debug ("SDL GL window: %p", SDL_GL_GetCurrentWindow ());
    g_debug ("SDL GL context: %p", SDL_GL_GetCurrentContext ());

    g_message ("GL vendor: %s", glGetString (GL_VENDOR));
    g_message ("GL renderer: %s", glGetString (GL_RENDERER));
    g_message ("GL extensions: %s", glGetString (GL_EXTENSIONS));
    g_message ("GL version: %s", glGetString (GL_VERSION));
    g_message ("GLSL version: %s", glGetString (GL_SHADING_LANGUAGE_VERSION));

    if (SDL_GL_SetSwapInterval(1) != 0) {
        g_set_error (error,
                     COG_PLATFORM_EGL_ERROR,  // XXX
                     0,
                     "Cannot enable GL VSync: %s",
                     SDL_GetError ());
        return false;
    }

#if 0
    SDL_GL_MakeCurrent (window->sdl_window, window->sdl_glcontext);
#endif

    // EGLDisplay display = eglGetCurrentDisplay ();
    // EGLDisplay display = eglGetDisplay ((EGLNativeDisplayType) 
    // g_assert (display != EGL_NO_DISPLAY);

#if 0
    EGLint egl_major = 0, egl_minor = 0;
    if (eglInitialize (display, &egl_major, &egl_minor) == EGL_FALSE) {
        g_set_error_literal (error,
                             COG_PLATFORM_EGL_ERROR,  // XXX
                             eglGetError (),
                             "Cannot initialize EGL",
        return false;
    }

    g_message ("EGL %i.%i successfully initialized.", egl_major, egl_minor);
#endif

    // wpe_fdo_initialize_for_egl_display (display);

    return true;
}


static bool
setup_window (struct platform_window* window, GError **error)
{
    g_assert_nonnull (window);

    static const struct {
        SDL_GLattr sdl_attr;
        int value;
        const char *what;
    } s_context_attributes[] = {
#define _ITEM(name, val) { SDL_GL_ ## name, (val), #name}
        _ITEM (RED_SIZE, 8),
        _ITEM (GREEN_SIZE, 8),
        _ITEM (BLUE_SIZE, 8),
        _ITEM (DOUBLEBUFFER, 1),
        // _ITEM (CONTEXT_EGL, 1),
        _ITEM (CONTEXT_MAJOR_VERSION, 2),
        _ITEM (CONTEXT_MINOR_VERSION, 0),
        _ITEM (CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES),
#undef _ITEM
    };

    for (unsigned i = 0; i < G_N_ELEMENTS (s_context_attributes); i++) {
        if (SDL_GL_SetAttribute (s_context_attributes[i].sdl_attr,
                                 s_context_attributes[i].value) != 0) {
            g_set_error (error,
                         COG_PLATFORM_EGL_ERROR,  // XXX
                         0,
                         "Cannot set SDL EGL context attribute %s: %s",
                         s_context_attributes[i].what,
                         SDL_GetError ());
            return false;
        }
    }

    if (!(window->sdl_window = SDL_CreateWindow (WINDOW_TITLE,
                                                 SDL_WINDOWPOS_CENTERED,
                                                 SDL_WINDOWPOS_CENTERED,
                                                 WINDOW_WIDTH,
                                                 WINDOW_HEIGHT,
                                                 SDL_WINDOW_SHOWN |
                                                 SDL_WINDOW_OPENGL |
                                                 SDL_WINDOW_RESIZABLE))) {
        g_set_error (error,
                     COG_PLATFORM_EGL_ERROR,  // XXX
                     0,
                     "Cannot create SDL window: %s",
                     SDL_GetError());
        return false;
    }

    return true;
}


static void
on_export_egl_image (void *userdata, EGLImageKHR image)
{
    struct platform_window *window = userdata;

    g_message ("FRAME!");

    SDL_GL_MakeCurrent (window->sdl_window, window->sdl_glcontext);

    // TODO: Paint frame.

    SDL_GL_SwapWindow (window->sdl_window);
}


static bool
setup_fdo_exportable (struct platform_window *window, GError **error)
{
    g_assert_nonnull (window);

    static const struct wpe_view_backend_exportable_fdo_egl_client client = {
        .export_egl_image = on_export_egl_image,
    };

    int w, h;
    SDL_GL_GetDrawableSize (window->sdl_window, &w, &h);
    g_assert_cmpint (w, >, 0);
    g_assert_cmpint (h, >, 0);

    window->exportable =
        wpe_view_backend_exportable_fdo_egl_create (&client,
                                                    window,
                                                    (uint32_t) w,
                                                    (uint32_t) h);
    g_assert_nonnull (window->exportable);
    
    window->view_backend =
        webkit_web_view_backend_new (wpe_view_backend_exportable_fdo_get_view_backend (window->exportable),
                                     (GDestroyNotify) wpe_view_backend_exportable_fdo_destroy,
                                     window->exportable);
    g_assert_nonnull (window->view_backend);

    return true;
}


static gboolean
on_sdl_event (GSource                *source,
              const SDL_Event        *event,
              struct platform_window *window)
{
    g_assert_nonnull (event);
    g_assert_nonnull (window);

    switch (event->type) {
        do_quit:
        case SDL_QUIT:
            if (g_application_get_default ())
                g_application_quit (g_application_get_default ());
            return G_SOURCE_REMOVE;

        case SDL_WINDOWEVENT:
            switch (event->window.type) {
                case SDL_WINDOWEVENT_CLOSE:
                    goto do_quit;  // Jump just above.

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    g_message ("New window size: %" PRIi32 "x%" PRIi32,
                               event->window.data1,
                               event->window.data2);
                    g_assert_cmpint (event->window.data1, >, 0);
                    g_assert_cmpint (event->window.data2, >, 0);
                    wpe_view_backend_dispatch_set_size (wpe_view_backend_exportable_fdo_get_view_backend (window->exportable),
                                                        event->window.data1,
                                                        event->window.data2);
                    break;
            }
            break;
    }

    return G_SOURCE_CONTINUE;
}


static bool
setup_event_source (struct platform_window *window, GError **error)
{
    g_assert_nonnull (window);
    g_assert_null (window->event_source);

    window->event_source = sdl_event_source_new ();
    g_source_set_callback (window->event_source,
                           G_SOURCE_FUNC (on_sdl_event),
                           window,
                           NULL);
    g_source_attach (window->event_source, NULL);
}


gboolean
cog_platform_setup (CogPlatform *platform,
                    CogShell    *shell G_GNUC_UNUSED,
                    const char  *params,
                    GError     **error)
{
    g_assert_nonnull (platform);

    wpe_loader_init ("libWPEBackend-fdo-0.1.so");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        g_set_error (error,
                     COG_PLATFORM_EGL_ERROR,  // XXX
                     0,
                     "SDL error: %s",
                     SDL_GetError());
        return FALSE;
    }

    if (setup_event_source (&win, error) &&
        setup_window (&win, error) &&
        setup_sdl_opengles (&win, error) &&
        setup_fdo_exportable (&win, error))
        return TRUE;

    cog_platform_teardown (platform);
    return FALSE;
}


void
cog_platform_teardown (CogPlatform *platform)
{
    g_assert_nonnull (platform);

    if (win.event_source) {
        g_source_destroy (win.event_source);
        g_clear_pointer (&win.event_source, g_source_unref);
    }

    g_clear_pointer (&win.sdl_glcontext, SDL_GL_DeleteContext);
    g_clear_pointer (&win.sdl_window, SDL_DestroyWindow);

    SDL_Quit ();
}


WebKitWebViewBackend*
cog_platform_get_view_backend (CogPlatform   *platform,
                               WebKitWebView *related_view,
                               GError       **error)
{
    g_assert_nonnull (platform);
    g_assert_nonnull (win.view_backend);
    return win.view_backend;
}
