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

void init_base_lut(void);

int parse_fasta_stream(FILE *fp, void *ctx,
                       emit_base_cb emit_base,   
                       emit_bulk_cb emit_bulk,   
                       end_seq_cb end_seq);      

int parse_fasta_path(const char *path, void *ctx,
                     emit_base_cb emit_base,
                     emit_bulk_cb emit_bulk,
                     end_seq_cb end_seq);

static inline void fastio_init(void){ init_base_lut(); }

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
