#ifndef FASTIO_H
#define FASTIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

// Reading buffer
typedef struct {
    gzFile fp;
    unsigned char *buf;
    size_t buf_size;
    size_t buf_pos;
    size_t buf_len;
} FastReader;

// Writing buffer
typedef struct {
    FILE *fp;
    unsigned char *buf;
    size_t buf_size;
    size_t buf_pos;
} FastWriter;

// Reader funcs
FastReader *fr_open(const char *path, size_t buf_size);
void fr_close(FastReader *fr);
int fr_read(FastReader *fr, unsigned char *dst, size_t len);

// Writer funcs
FastWriter *fw_open(const char *path, size_t buf_size);
void fw_close(FastWriter *fw);
int fw_write(FastWriter *fw, const unsigned char *src, size_t len);

#endif
