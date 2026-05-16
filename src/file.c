#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "../include/file.h"

bool has_extension(const char *filename, const char *ext) {
    size_t filenameLength = strlen(filename);
    size_t extLength = strlen(ext);
    return filenameLength >= extLength &&
    strcmp(filename + filenameLength - extLength, ext) == 0;
}

