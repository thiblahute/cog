/*
 * cog-platform-default.c
 * Copyright (C) 2018 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "cog.h"


gboolean
cog_platform_setup (CogPlatform *platform,
                    CogShell    *shell,
                    const char  *params,
                    GError     **error)
{
    g_assert_nonnull (platform);
    g_return_val_if_fail (COG_IS_SHELL (shell), FALSE);

    const char *lib = (params && *params) ? params : "libWPEBackend-default.so";

    if (!wpe_loader_init (lib)) {
        g_set_error (error,
                     COG_PLATFORM_WPE_ERROR,
                     COG_PLATFORM_WPE_ERROR_INIT,
                     "Failed to set backend library '%s'",
                     lib);
        return FALSE;
    }

    return TRUE;
}


void
cog_platform_teardown (CogPlatform *platform)
{
}


WebKitWebViewBackend*
cog_platform_get_view_backend (CogPlatform   *platform,
                               WebKitWebView *related_view,
                               GError       **error)
{
    return NULL;
}
