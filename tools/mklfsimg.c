//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  mklfsimg CLI: mklfsimg <source-dir> <output-image>
//
//  Builds a LittleFS backup image from <source-dir>. Copy the image to the SD
//  card and run `.restore "/sd/<image>` on the device to populate the internal
//  filesystem. See tools/mklfsimg.h for the geometry and format notes.
//

#include "mklfsimg.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <source-dir> <output-image>\n", argv[0]);
        return 2;
    }

    char err[256];
    if (!mklfsimg_build(argv[1], argv[2], err, sizeof(err)))
    {
        fprintf(stderr, "mklfsimg: %s\n", err);
        return 1;
    }

    printf("wrote %s from %s (%u blocks x %u bytes)\n",
           argv[2], argv[1], MKLFSIMG_BLOCK_COUNT, MKLFSIMG_BLOCK_SIZE);
    return 0;
}
