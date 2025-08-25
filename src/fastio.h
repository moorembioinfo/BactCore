#ifndef FASTIO_H
#define FASTIO_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*emit_base_cb)(void *ctx, unsigned char base);
typedef void (*emit_bulk_cb)(void *ctx, const unsigned char *bases, size_t n);
typedef void (*end_seq_cb)(void *ctx);

extern unsigned char FASTIO_OUT_MAP[256];
extern uint8_t FASTIO_VALID_BITS[256];

void fastio_init(void);

int parse_fasta_stream(FILE *fp, void *ctx,
                       emit_base_cb emit_base,
                       emit_bulk_cb emit_bulk,
                       end_seq_cb end_seq);

int parse_fasta_path(const char *path, void *ctx,
                     emit_base_cb emit_base,
                     emit_bulk_cb emit_bulk,
                     end_seq_cb end_seq);

static inline int parse_fasta(FILE *fp, void *ctx,
                              emit_base_cb emit_base,
                              end_seq_cb end_seq)
{
    return parse_fasta_stream(fp, ctx, emit_base, NULL, end_seq);
}

#ifdef __cplusplus
}
#endif

#endif
