/*
punycode.c from RFC 3492
http://www.nicemice.net/idn/
Adam M. Costello
http://www.nicemice.net/amc/

This is ANSI C code (C89) implementing Punycode (RFC 3492).



C. Disclaimer and license

    Regarding this entire document or any portion of it (including
    the pseudocode and C code), the author makes no guarantees and
    is not responsible for any damage resulting from its use.  The
    author grants irrevocable permission to anyone to use, modify,
    and distribute it in any way that does not diminish the rights
    of anyone else to use, modify, and distribute it, provided that
    redistributed derivative works do not contain misleading author or
    version information.  Derivative works need not be licensed under
    similar terms.
*/

#include <limits.h>

enum punycode_status {
	punycode_success,
	punycode_bad_input,  /* Input is invalid.                       */
	punycode_big_output, /* Output would exceed the space provided. */
	punycode_overflow    /* Input needs wider integers to process.  */
};

#if UINT_MAX >= (1 << 26) - 1
typedef unsigned int punycode_uint;
#else
typedef unsigned long punycode_uint;
#endif

enum punycode_status punycode_encode(punycode_uint input_length,
		const punycode_uint input[],
		const unsigned char case_flags[],
		punycode_uint* output_length,
		char output[]);

enum punycode_status punycode_decode(punycode_uint input_length,
		const char input[],
		punycode_uint* output_length,
		punycode_uint output[],
		unsigned char case_flags[]);
