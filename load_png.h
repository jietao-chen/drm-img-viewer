#include <stdio.h>
#include <stdlib.h>
#include <png.h>

// returns: 0=success, non-zero=error code
int load_png(const char *filename, unsigned char **out_data, int *out_width, int *out_height, int *out_rowbytes);