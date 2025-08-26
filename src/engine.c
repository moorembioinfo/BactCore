
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "engine.h"
#include "fastio.h"

/* Fallback externs in case fastio.h doesn't declare them */
extern unsigned char FASTIO_OUT_MAP[256];
extern uint8_t FASTIO_VALID_BITS[256];

#ifndef BC_BUFSZ
#define BC_BUFSZ (1u<<22)
#endif

typedef struct { size_t L, M; } shape_t;
typedef enum { PS_IN_HEADER=0, PS_IN_SEQ=1 } parse_state_t;

static shape_t scan_shape(const char* in_path){
    FILE* f = fopen(in_path, "rb");
    if (!f) { perror("open input"); exit(1); }
    unsigned char *buf = (unsigned char*)malloc(BC_BUFSZ);
    if (!buf) { perror("malloc"); fclose(f); exit(1); }
    size_t L = 0, cur = 0, M = 0;
    int have_seq = 0;
    parse_state_t st = PS_IN_HEADER;
    for (;;) {
        size_t n = fread(buf, 1, BC_BUFSZ, f);
        if (n == 0) break;
        size_t i = 0;
        while (i < n) {
            unsigned char c = buf[i++];
            if (c == '>') {
                if (have_seq) {
                    if (L == 0) L = cur; else if (cur != L) { fprintf(stderr, "Unequal sequence lengths (%zu vs %zu)\n", cur, L); exit(2); }
                }
                M++;
                cur = 0;
                have_seq = 1;
                st = PS_IN_HEADER;
                continue;
            }
            if (st == PS_IN_HEADER) {
                if (c == '\n') st = PS_IN_SEQ;
                continue;
            }
            unsigned char m = FASTIO_OUT_MAP[c];
            if (m) cur++;
        }
    }
    fclose(f);
    free(buf);
    if (have_seq) {
        if (L == 0) L = cur; else if (cur != L) { fprintf(stderr, "Unequal sequence lengths (%zu vs %zu)\n", cur, L); exit(2); }
    }
    if (L == 0 || M == 0) { fprintf(stderr, "Empty FASTA or parse error.\n"); exit(3); }
    shape_t s; s.L=L; s.M=M; return s;
}

static inline uint64_t ceil_mul(double f, uint64_t m){
    double x = f * (double)m;
    uint64_t t = (uint64_t)x;
    return (t == x) ? t : t + 1;
}

int engine_run(const char* in_path, const char* out_path, const engine_cfg_t* cfg){
    assert(cfg);
    fastio_init();

    shape_t shp = scan_shape(in_path);
    const size_t L = shp.L, M = shp.M;
    const uint64_t need_valid = cfg->strict ? M : ceil_mul(cfg->threshold, M);

    uint16_t *valid = (uint16_t*)calloc(L, sizeof(uint16_t));
    uint8_t  *seen  = (uint8_t*)calloc(L, sizeof(uint8_t));
    if (!valid || !seen) { perror("calloc"); free(valid); free(seen); return 10; }

    FILE* f = fopen(in_path, "rb");
    if (!f) { perror("open input"); free(valid); free(seen); return 11; }
    unsigned char *buf = (unsigned char*)malloc(BC_BUFSZ);
    if (!buf) { perror("malloc"); fclose(f); free(valid); free(seen); return 11; }

    size_t col = 0;
    parse_state_t st = PS_IN_HEADER;
    for (;;) {
        size_t n = fread(buf, 1, BC_BUFSZ, f);
        if (n == 0) break;
        size_t i = 0;
        while (i < n) {
            unsigned char c = buf[i++];
            if (c == '>') {
                st = PS_IN_HEADER;
                col = 0;
                continue;
            }
            if (st == PS_IN_HEADER) {
                if (c == '\n') st = PS_IN_SEQ;
                continue;
            }
            unsigned char m = FASTIO_OUT_MAP[c];
            if (m) {
                unsigned char vb = FASTIO_VALID_BITS[c];
                if (vb) valid[col]++;
                seen[col] |= vb;
                col++;
            }
        }
    }
    fclose(f);
    free(buf);

    const size_t nwords = (L + 63) / 64;
    uint64_t *mask = (uint64_t*)calloc(nwords, sizeof(uint64_t));
    if (!mask) { perror("calloc mask"); free(valid); free(seen); return 12; }

    if (cfg->snps_only) {
        for (size_t k = 0; k < L; ++k) {
            if (valid[k] >= need_valid) {
                uint8_t bm = seen[k];
                if (!(bm && (bm & (bm - 1)) == 0))
                    mask[k >> 6] |= (1ull << (k & 63));
            }
        }
    } else {
        for (size_t k = 0; k < L; ++k)
            if (valid[k] >= need_valid)
                mask[k >> 6] |= (1ull << (k & 63));
    }

    FILE* in2 = fopen(in_path, "rb");
    if (!in2) { perror("reopen input"); free(valid); free(seen); free(mask); return 13; }
    FILE* out = fopen(out_path, "wb");
    if (!out) { perror("open output"); fclose(in2); free(valid); free(seen); free(mask); return 14; }

    unsigned char *inbuf = (unsigned char*)malloc(BC_BUFSZ);
    unsigned char *outbuf = (unsigned char*)malloc(BC_BUFSZ);
    if (!inbuf || !outbuf) { perror("malloc"); fclose(in2); fclose(out); free(inbuf); free(outbuf); free(valid); free(seen); free(mask); return 15; }

    st = PS_IN_HEADER;
    col = 0;
    int have_seq_written = 0;
    size_t outn = 0;

    for (;;) {
        size_t n = fread(inbuf, 1, BC_BUFSZ, in2);
        if (n == 0) break;
        size_t i = 0;
        while (i < n) {
            unsigned char c = inbuf[i++];
            if (c == '>') {
                if (have_seq_written) {
                    if (outn) { fwrite(outbuf, 1, outn, out); outn = 0; }
                    fputc('\n', out);
                    have_seq_written = 0;
                }
                fputc('>', out);
                st = PS_IN_HEADER;
                col = 0;
                continue;
            }
            if (st == PS_IN_HEADER) {
                fputc((int)c, out);
                if (c == '\n') st = PS_IN_SEQ;
                continue;
            }
            unsigned char m = FASTIO_OUT_MAP[c];
            if (m) {
                uint64_t w = mask[col >> 6];
                if (w & (1ull << (col & 63))) {
                    outbuf[outn++] = m;
                    if (outn == BC_BUFSZ) { fwrite(outbuf, 1, outn, out); outn = 0; }
                    have_seq_written = 1;
                }
                col++;
            }
        }
    }

    if (have_seq_written) {
        if (outn) fwrite(outbuf, 1, outn, out);
        fputc('\n', out);
    }

    fclose(in2);
    fclose(out);
    free(inbuf);
    free(outbuf);
    free(valid);
    free(seen);
    free(mask);
    return 0;
}
