#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "engine.h"
#include "bitpack.h"

//  1 MiB buffers 
#ifndef BC_BUFSZ
#define BC_BUFSZ (1u<<20)
#endif

// LUT storage
uint8_t BASE_LUT[256];

typedef struct { size_t L, M; } shape_t;

// get shape
static shape_t scan_shape(const char* in_path) {
    FILE* f = fopen(in_path, "rb");
    if (!f) { perror("open input"); exit(1); }

    char *line = (char*)malloc(BC_BUFSZ);
    size_t L = 0, cur = 0, M = 0;
    int in_seq = 0;

    while (fgets(line, (int)BC_BUFSZ, f)) {
        if (line[0] == '>') {
            if (in_seq) {
                if (L == 0) L = cur;
                else if (cur != L) {
                    fprintf(stderr, "Unequal sequence lengths (%zu vs %zu)\n", cur, L);
                    exit(2);
                }
            }
            M++; in_seq = 1; cur = 0;
            continue;
        }
        if (!in_seq) continue;
        for (const unsigned char *p=(unsigned char*)line;; ++p) {
            unsigned char c = *p;
            if (!c) break;
            if (c=='\n' || c=='\r' || c==' ' || c=='\t') continue;
            cur++;
        }
    }
    if (in_seq) {
        if (L == 0) L = cur;
        else if (cur != L) {
            fprintf(stderr, "Unequal sequence lengths (%zu vs %zu)\n", cur, L);
            exit(2);
        }
    }
    free(line);
    fclose(f);
    if (L == 0 || M == 0) { fprintf(stderr, "Empty FASTA or parse error.\n"); exit(3); }
    return (shape_t){ L, M };
}

static inline uint64_t ceil_mul(double f, uint64_t m) {
    double x = f * (double)m;
    uint64_t t = (uint64_t)x;
    return (t == x) ? t : t + 1;
}

int engine_run(const char* in_path, const char* out_path, const engine_cfg_t* cfg) {
    assert(cfg);
    init_base_lut();

    // Shape
    shape_t shp = scan_shape(in_path);
    const size_t L = shp.L, M = shp.M;
    const uint64_t need_valid = cfg->strict ? M : ceil_mul(cfg->threshold, M);

    // Per-column accumulators 
    uint16_t *valid = (uint16_t*)calloc(L, sizeof(uint16_t));  //  ATCG per column
    uint8_t  *seen  = (uint8_t*)calloc(L, sizeof(uint8_t));    // bitmask 
    if (!valid || !seen) { perror("calloc accumulators"); free(valid); free(seen); return 10; }

    // PASS 1
    FILE* f = fopen(in_path, "rb");
    if (!f) { perror("open input"); free(valid); free(seen); return 11; }

    char *line = (char*)malloc(BC_BUFSZ);
    size_t col = 0; int in_seq = 0;

    while (fgets(line, (int)BC_BUFSZ, f)) {
        if (line[0] == '>') { in_seq = 1; col = 0; continue; }
        if (!in_seq) continue;

        const unsigned char *p = (unsigned char*)line;
        for (;;) {
            unsigned char ch = *p++;
            if (!ch) break;                                  // end of this chunk
            if (ch=='\n' || ch=='\r' || ch==' ' || ch=='\t') continue;
            uint8_t b = BASE_LUT[ch];
            valid[col] += (b != 0);   // 1 if ATCG, 0 if not valid
            seen[col]  |= b;          // OR A/C/G/T bit
            col++;
        }
    }
    fclose(f);
    free(line);

    // Build mask
    const size_t nwords = (L + 63) / 64;
    uint64_t *mask = (uint64_t*)calloc(nwords, sizeof(uint64_t));
    if (!mask) { perror("calloc mask"); free(valid); free(seen); return 12; }

    if (cfg->snps_only) {
        for (size_t k = 0; k < L; ++k) {
            if (valid[k] >= need_valid) {
                uint8_t bm = seen[k];
                if (!(bm && (bm & (bm - 1)) == 0))  // keep only polymorphic
                    mask[k >> 6] |= (1ull << (k & 63));
            }
        }
    } else {
        for (size_t k = 0; k < L; ++k)
            if (valid[k] >= need_valid)
                mask[k >> 6] |= (1ull << (k & 63));
    }

    free(valid); free(seen);

    // PASS 2, mask then output
    FILE* out = fopen(out_path, "wb");
    if (!out) { perror("open output"); free(mask); return 13; }
    FILE* in2 = fopen(in_path, "rb");
    if (!in2) { perror("reopen input"); fclose(out); free(mask); return 14; }

    char *buf = (char*)malloc(BC_BUFSZ);
    col = 0; in_seq = 0;

    while (fgets(buf, (int)BC_BUFSZ, in2)) {
        if (buf[0] == '>') { fputs(buf, out); in_seq = 1; col = 0; continue; }
        if (!in_seq) continue;

        const unsigned char *p = (unsigned char*)buf;
        for (;;) {
            unsigned char ch = *p++;
            if (!ch) break;
            if (ch=='\n' || ch=='\r' || ch==' ' || ch=='\t') continue;
            uint64_t w = mask[col >> 6];
            if (w & (1ull << (col & 63))) fputc(ch, out);
            col++;
        }
        fputc('\n', out);
    }

    fclose(in2);
    fclose(out);
    free(buf);
    free(mask);
    return 0;
}
