/* See LICENSE file for copyright and license details. */
#if defined(__has_include)
#if __has_include(<stb_image.h>)
#else
#undef TERMINAL_IMG_VIEWER
#endif
#endif

#ifdef TERMINAL_IMG_VIEWER
#ifndef _IMG_H_
#define _IMG_H_

#include <stdint.h>
#include <stb_image.h>

int img_display(uint8_t* data, int w, int h, int offsety);

#endif
#endif
