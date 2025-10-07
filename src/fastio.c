#include "fastio.h"
#include <string.h>
#include <stdlib.h>

#ifndef FASTIO_BUF_SZ
#define FASTIO_BUF_SZ (1u << 22)
#endif

#ifndef FASTIO_OUT_SZ
#define FASTIO_OUT_SZ (1u << 16)
#endif

unsigned char FASTIO_OUT_MAP[256];
uint8_t      FASTIO_VALID_BITS[256];

void fastio_init(void){
    for (int i = 0; i < 256; ++i) {
        FASTIO_OUT_MAP[i] = '-';
        FASTIO_VALID_BITS[i] = 0;
    }
    FASTIO_OUT_MAP[' ']  = 0;
    FASTIO_OUT_MAP['\t'] = 0;
    FASTIO_OUT_MAP['\n'] = 0;
    FASTIO_OUT_MAP['\r'] = 0;
    FASTIO_OUT_MAP['\f'] = 0;
    FASTIO_OUT_MAP['\v'] = 0;

    FASTIO_OUT_MAP['A'] = 'A'; FASTIO_OUT_MAP['a'] = 'A';
    FASTIO_OUT_MAP['C'] = 'C'; FASTIO_OUT_MAP['c'] = 'C';
    FASTIO_OUT_MAP['G'] = 'G'; FASTIO_OUT_MAP['g'] = 'G';
    FASTIO_OUT_MAP['T'] = 'T'; FASTIO_OUT_MAP['t'] = 'T';
    FASTIO_OUT_MAP['-'] = '-';

    FASTIO_VALID_BITS['A'] = FASTIO_VALID_BITS['a'] = 1u<<0;
    FASTIO_VALID_BITS['C'] = FASTIO_VALID_BITS['c'] = 1u<<1;
    FASTIO_VALID_BITS['G'] = FASTIO_VALID_BITS['g'] = 1u<<2;
    FASTIO_VALID_BITS['T'] = FASTIO_VALID_BITS['t'] = 1u<<3;
}

typedef enum { IN_HEADER, IN_SEQ } ParseState;

static inline void flush_out(void *ctx,
#ifdef FASTIO_SINK
                             const unsigned char *outbuf, size_t out_n)
{
    if (out_n) { FASTIO_SINK(ctx, outbuf, out_n); }
}
#else
                             const unsigned char *outbuf, size_t out_n,
                             emit_bulk_cb emit_bulk, emit_base_cb emit_base)
{
    if (!out_n) return;
    if (emit_bulk) {
        emit_bulk(ctx, outbuf, out_n);
    } else if (emit_base) {
        for (size_t i = 0; i < out_n; ++i) emit_base(ctx, outbuf[i]);
    }
}
#endif

static void parse_buffer(const unsigned char *buf, size_t n,
                         void *ctx,
#ifndef FASTIO_SINK
                         emit_bulk_cb emit_bulk, emit_base_cb emit_base,
#endif
                         end_seq_cb end_seq,
                         int *p_seen_any_seq, ParseState *p_state,
                         unsigned char *outbuf, size_t *p_out_n)
{
    int seen = *p_seen_any_seq;
    ParseState st = *p_state;
    size_t out_n = *p_out_n;

    for (size_t i = 0; i < n; ) {
        unsigned char c = buf[i++];

        if (c == '>') {
#ifdef FASTIO_SINK
            flush_out(ctx, outbuf, out_n);
#else
            flush_out(ctx, outbuf, out_n, emit_bulk, emit_base);
#endif
            out_n = 0;
            if (seen && end_seq) end_seq(ctx);
            seen = 1;
            st = IN_HEADER;
            while (i < n && buf[i-1] != '\n') { if (buf[i] == '\n') { i++; break; } i++; }
            continue;
        }

        if (st == IN_HEADER) {
            if (c == '\n') st = IN_SEQ;
            continue;
        }

        unsigned char m = FASTIO_OUT_MAP[c];
        if (m) {
            outbuf[out_n++] = m;
            if (out_n == FASTIO_OUT_SZ) {
#ifdef FASTIO_SINK
                flush_out(ctx, outbuf, out_n);
#else
                flush_out(ctx, outbuf, out_n, emit_bulk, emit_base);
#endif
                out_n = 0;
            }
        }
    }

    *p_seen_any_seq = seen;
    *p_state = st;
    *p_out_n = out_n;
}

int parse_fasta_stream(FILE *fp, void *ctx,
                       emit_base_cb emit_base,
                       emit_bulk_cb emit_bulk,
                       end_seq_cb end_seq)
{
    if (!fp) return -1;

    int seen = 0;
    ParseState st = IN_SEQ;

    unsigned char *inbuf  = (unsigned char*)malloc(FASTIO_BUF_SZ);
    unsigned char *outbuf = (unsigned char*)malloc(FASTIO_OUT_SZ);
    if (!inbuf || !outbuf) { free(inbuf); free(outbuf); return -2; }

    size_t out_n = 0;

    for (;;) {
        size_t n = fread(inbuf, 1, FASTIO_BUF_SZ, fp);
        if (n == 0) break;

        parse_buffer(inbuf, n, ctx,
#ifndef FASTIO_SINK
                     emit_bulk, emit_base,
#endif
                     end_seq, &seen, &st, outbuf, &out_n);
    }

#ifdef FASTIO_SINK
    flush_out(ctx, outbuf, out_n);
#else
    flush_out(ctx, outbuf, out_n, emit_bulk, emit_base);
#endif

    if (seen && end_seq) end_seq(ctx);

    free(inbuf);
    free(outbuf);
    return 0;
}

int parse_fasta_path(const char *path, void *ctx,
                     emit_base_cb emit_base,
                     emit_bulk_cb emit_bulk,
                     end_seq_cb end_seq)
{
    FILE *fp = NULL;
    if (!path || (path[0] == '-' && path[1] == '\0')) {
        fp = stdin;
    } else {
        fp = fopen(path, "rb");
        if (!fp) return -3;
    }
    int rc = parse_fasta_stream(fp, ctx, emit_base, emit_bulk, end_seq);
    if (fp != stdin) fclose(fp);
    return rc;
}
