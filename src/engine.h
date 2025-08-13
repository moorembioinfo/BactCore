#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Single-threaded ultra-fast 
typedef struct {
    bool    strict;      // --strict
    bool    snps_only;   // --strict + --snps
    double  threshold;   // retain columns with (ATCG)/M >= threshold 
} engine_cfg_t;

// Run
int engine_run(const char* in_path, const char* out_path, const engine_cfg_t* cfg);

#endif
