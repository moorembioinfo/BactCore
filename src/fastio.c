#include "fastio.h"
#include <string.h>
#include <stdlib.h>  

#ifndef FASTIO_BUF_SZ
#define FASTIO_BUF_SZ (1u << 20) /* 1 MiB */
#endif


static unsigned char BASE_LUT[256];

void init_base_lut(void){
    for (int i = 0; i < 256; ++i) BASE_LUT[i] = '-';
    BASE_LUT[(unsigned char)'A'] = 'A'; BASE_LUT[(unsigned char)'a'] = 'A';
    BASE_LUT[(unsigned char)'C'] = 'C'; BASE_LUT[(unsigned char)'c'] = 'C';
    BASE_LUT[(unsigned char)'G'] = 'G'; BASE_LUT[(unsigned char)'g'] = 'G';
    BASE_LUT[(unsigned char)'T'] = 'T'; BASE_LUT[(unsigned char)'t'] = 'T';
    BASE_LUT[(unsigned char)'-'] = '-';
}

typedef enum { IN_HEADER, IN_SEQ } ParseState;

static inline int is_ws(unsigned char c){
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static inline void skip_line(const unsigned char *buf, size_t *i, size_t n){
    while (*i < n && buf[*i] != '\n') (*i)++;
    if (*i < n) (*i)++;
}

static void parse_chunk(const unsigned char *buf, size_t n,
                        void *ctx, emit_base_cb emit_base, end_seq_cb end_seq,
                        int *p_seen_any_seq, ParseState *p_state)
{
    int seen_any_seq = *p_seen_any_seq;
    ParseState st = *p_state;

    for (size_t i = 0; i < n; ) {
        unsigned char c = buf[i];

        if (c == '>') {
            if (seen_any_seq && end_seq) end_seq(ctx);
            seen_any_seq = 1;
            st = IN_HEADER;
            skip_line(buf, &i, n);
            continue;
        }

        if (st == IN_HEADER) {
            skip_line(buf, &i, n);
            st = IN_SEQ;
            continue;
        }

        if (is_ws(c)) { i++; continue; }

        unsigned char out = BASE_LUT[c]; 
        if (emit_base) emit_base(ctx, out);
        i++;
    }

    *p_seen_any_seq = seen_any_seq;
    *p_state = st;
}

int parse_fasta_stream(FILE *fp, void *ctx,
                       emit_base_cb emit_base,
                       end_seq_cb end_seq)
{
    if (!fp) return -1;

    int seen_any_seq = 0;
    ParseState state = IN_SEQ;

    unsigned char *buf = (unsigned char*)malloc(FASTIO_BUF_SZ);
    if (!buf) return -2;

    for (;;) {
        size_t n = fread(buf, 1, FASTIO_BUF_SZ, fp);
        if (n == 0) break;
        parse_chunk(buf, n, ctx, emit_base, end_seq, &seen_any_seq, &state);
    }

    if (seen_any_seq && end_seq) end_seq(ctx);

    free(buf);
    return 0;
}

int parse_fasta_path(const char *path, void *ctx,
                     emit_base_cb emit_base,
                     end_seq_cb end_seq)
{
    FILE *fp = NULL;
    if (!path || (path[0] == '-' && path[1] == '\0')) {
        fp = stdin;
    } else {
        fp = fopen(path, "rb");
        if (!fp) return -3;
    }

    int rc = parse_fasta_stream(fp, ctx, emit_base, end_seq);
    if (fp != stdin) fclose(fp);
    return rc;
}
