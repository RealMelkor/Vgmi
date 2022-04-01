# Vgmi

Gemini client written in C with vim-like keybindings

## Keybindings

* h  - Go back in the history
* l  - Go forward in the history
* k  - Scroll up
* j  - Scroll down
* gg - Go at the top of the page
* G  - Go at the top of the page
* :  - Open input mode
* u  - Open input mode with the current url
* r  - Reload the page
* [number]Tab - Select link
* Tab - Follow selected link
* Shift+Tab - Open selected link in a new tab

You can prefix a movement key with a number to repeat it.

## Commands

* :q        - Exit the program
* :o	    - Open an url
* :nt <url> - Open a new tab, the url is optional
* :[number] - Follow the link 

## Dependencies

* [libtls][0] - a new TLS library
* [termbox2][1] - terminal rendering library

[0]: https://www.libressl.org/
[1]: https://github.com/termbox/termbox2
