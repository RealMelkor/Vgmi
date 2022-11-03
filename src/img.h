/* See LICENSE file for copyright and license details. */
#if defined(TERMINAL_IMG_VIEWER) && defined(__has_include) && \
    !(__has_include(<stb_image.h>))
#undef TERMINAL_IMG_VIEWER
#endif

#ifdef TERMINAL_IMG_VIEWER
#ifndef _IMG_H_
#define _IMG_H_

#include <stdint.h>
#include <stb_image.h>

int img_display(uint8_t* data, int w, int h, int offsety);

#endif
#endif
