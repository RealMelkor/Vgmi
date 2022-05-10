# Vgmi

Gemini client written in C with vim-like keybindings

## Keybindings

* k  - Scroll up
* j  - Scroll down
* h  - Switch to the previous tab
* l  - Switch to the next tab
* H  - Go back in the history
* L  - Go forward in the history
* gg - Go at the top of the page
* G  - Go at the bottom of the page
* :  - Open input mode
* u  - Open input mode with the current url
* f  - Show the history
* r  - Reload the page
* [number]Tab - Select link
* Tab - Follow selected link
* Shift+Tab - Open selected link in a new tab

You can prefix a movement key with a number to repeat it.

## Commands

* :q		- Close the current tab
* :qa		- Close all tabs, exit the program
* :o [url]	- Open an url
* :s [search]	- Search the Geminispace using geminispace.info
* :nt [url]	- Open a new tab, the url is optional
* :[number]	- Follow the link 
* :gencert	- Generate a certificate for the current capsule

## Dependencies

* [libtls][0] - a new TLS library
* [termbox2][1] - terminal rendering library

### Optional dependency
* [stb-image][2] - image loading library

### On Linux
* [libbsd][3] - utility functions from BSD systems

[0]: https://git.causal.agency/libretls/about/
[1]: https://github.com/termbox/termbox2
[2]: https://github.com/nothings/stb/blob/master/stb_image.h
[3]: https://libbsd.freedesktop.org/wiki/
