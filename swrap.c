/*
 * Copyright (c) 2025 Ian Marco Moffett and the Osmora Team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Hyra nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _DEFAULT_SOURCE
#include <sys/mman.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>

/* MUST BE A POWER OF TWO */
#define SECTOR_SIZE 512

#define ALIGN_UP(value, align) (((value) + (align)-1) & ~((align)-1))

static const char *input_dir = NULL;
static const char *output_file = NULL;
static uint64_t total_bytes = 0;

static void
help(void)
{
    printf(
        "swrap - wrap directory in sector friendly image\n"
        "Copyright (c) 2025 Ian Marco Moffett and the OSMORA team\n"
        "[-h]   Show this menu\n"
        "[-i]   Input directory\n"
        "[-o]   Output image\n"
    );
}

static void
append_file(const char *path, int outfd)
{
    /* static, .bss */
    __attribute__((section(".bss")))
    static char sect[SECTOR_SIZE];

    void *data;
    size_t file_len, pad_len;
    uint16_t misalign;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    file_len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    data = mmap(NULL, file_len, PROT_READ, MAP_SHARED, fd, 0);

    if (data == NULL) {
        perror("mmap");
        return;
    }

    total_bytes += file_len;
    write(outfd, data, file_len);

    /* Pad up to nearest sector size if needed */
    misalign = total_bytes & (SECTOR_SIZE - 1);
    if (misalign != 0) {
        pad_len = SECTOR_SIZE - misalign;
        assert(pad_len < SECTOR_SIZE);
        write(outfd, sect, pad_len);
    }

    close(fd);
}

static int
walk_dirs(const char *dirpath, int outfd)
{
    char pathbuf[256];
    struct dirent *dirent;
    DIR *dir;

    dir = opendir(dirpath);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] == '.') {
            continue;
        }

        snprintf(
            pathbuf,
            sizeof(pathbuf),
            "%s/%s",
            dirpath,
            dirent->d_name
        );

        /*
         * If this is a directory, recurse. However, if this
         * is a regular file, append it to the output
         */
        if (dirent->d_type == DT_DIR) {
            printf("[d] %s\n", pathbuf);
            walk_dirs(pathbuf, outfd);
        } else if (dirent->d_type == DT_REG) {
            printf("[f] %s\n", pathbuf);
            append_file(pathbuf, outfd);
        }
    }

    return 0;
}

static int
wrap(void)
{
    int outfd, error;

    outfd = open(output_file, O_RDWR | O_CREAT, 0666);
    if (outfd < 0) {
        perror("open");
        return -1;
    }

    /* Skip the header */
    lseek(outfd, SECTOR_SIZE, SEEK_SET);
    if ((error = walk_dirs(input_dir, outfd)) < 0) {
        close(outfd);
        return error;
    }

    /* Write the total size */
    total_bytes = ALIGN_UP(total_bytes, SECTOR_SIZE);
    lseek(outfd, 0, SEEK_SET);
    write(outfd, &total_bytes, sizeof(total_bytes));
    close(outfd);
    return 0;
}

int
main(int argc, char **argv)
{
    int opt;

    if (argc < 2) {
        printf("fatal: too few arguments\n");
        help();
        return -1;
    }

    while ((opt = getopt(argc, argv, "hi:o:")) != -1) {
        switch (opt) {
        case 'h':
            help();
            break;
        case 'i':
            input_dir = strdup(optarg);
            break;
        case 'o':
            output_file = strdup(optarg);
            break;
        }
    }

    if (input_dir == NULL) {
        printf("fatal: expected input directory\n");
        help();
        return -1;
    }

    if (output_file == NULL) {
        printf("fatal: expected output file\n");
        help();
        return -1;
    }

    return wrap();
}
