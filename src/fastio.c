#include "fastio.h"

FastReader *fr_open(const char *path, size_t buf_size) {
    FastReader *fr = malloc(sizeof(FastReader));
    fr->fp = gzopen(path, "rb");
    if (!fr->fp) { free(fr); return NULL; }
    fr->buf = malloc(buf_size);
    fr->buf_size = buf_size;
    fr->buf_pos = fr->buf_len = 0;
    return fr;
}

void fr_close(FastReader *fr) {
    if (!fr) return;
    gzclose(fr->fp);
    free(fr->buf);
    free(fr);
}

int fr_read(FastReader *fr, unsigned char *dst, size_t len) {
    size_t total = 0;
    while (total < len) {
        if (fr->buf_pos >= fr->buf_len) {
            fr->buf_len = gzread(fr->fp, fr->buf, fr->buf_size);
            fr->buf_pos = 0;
            if (fr->buf_len == 0) break; // EOF
        }
        size_t copy_len = fr->buf_len - fr->buf_pos;
        if (copy_len > len - total) copy_len = len - total;
        memcpy(dst + total, fr->buf + fr->buf_pos, copy_len);
        fr->buf_pos += copy_len;
        total += copy_len;
    }
    return (int)total;
}

FastWriter *fw_open(const char *path, size_t buf_size) {
    FastWriter *fw = malloc(sizeof(FastWriter));
    fw->fp = fopen(path, "wb");
    if (!fw->fp) { free(fw); return NULL; }
    fw->buf = malloc(buf_size);
    fw->buf_size = buf_size;
    fw->buf_pos = 0;
    return fw;
}

void fw_close(FastWriter *fw) {
    if (!fw) return;
    if (fw->buf_pos > 0) fwrite(fw->buf, 1, fw->buf_pos, fw->fp);
    fclose(fw->fp);
    free(fw->buf);
    free(fw);
}

int fw_write(FastWriter *fw, const unsigned char *src, size_t len) {
    size_t total = 0;
    while (total < len) {
        size_t space = fw->buf_size - fw->buf_pos;
        size_t copy_len = len - total;
        if (copy_len > space) copy_len = space;
        memcpy(fw->buf + fw->buf_pos, src + total, copy_len);
        fw->buf_pos += copy_len;
        total += copy_len;
        if (fw->buf_pos == fw->buf_size) {
            fwrite(fw->buf, 1, fw->buf_size, fw->fp);
            fw->buf_pos = 0;
        }
    }
    return (int)total;
}
