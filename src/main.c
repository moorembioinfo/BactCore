#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "engine.h"
#include "fastio.h"

static void usage(void) {
    fprintf(stderr,
        "Usage: BactCore [--strict] [--snps] [--threshold=0.95] <in.fa> <out.fa>\n"
        "\n"
        "Options:\n"
        "  --strict            Retain only columns with 0%% invalid gaps and ambiguous characters (equiv --threshold=1.0).\n"
        "  --snps              With --strict, retain only polymorphic A/C/G/T sites.\n"
        "  --threshold=VAL     Proportion of valid chars (ACGT) required (default 0.95)\n"
    );
}

int main(int argc, char** argv) {
    int strict = 0;
    int snps   = 0;
    double threshold = 0.95;

    const char* in_path  = NULL;
    const char* out_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--strict"))   { strict = 1; continue; }
        if (!strcmp(argv[i], "--snps"))     { snps = 1; continue; }
        if (!strncmp(argv[i], "--threshold", 11)) {
            const char *value = NULL;
            if (argv[i][11] == '=') value = argv[i] + 12;
            else if (argv[i][11] == '\0' && i + 1 < argc) value = argv[++i];
            else { usage(); return 64; }

            char *end = NULL;
            errno = 0;
            threshold = strtod(value, &end);
            if (errno != 0 || end == value || *end != '\0' ||
                !isfinite(threshold) || threshold < 0.0 || threshold > 1.0) {
                fprintf(stderr, "Invalid threshold: %s (expected 0..1)\n", value);
                return 64;
            }
            continue;
        }
        if (argv[i][0] == '-') { usage(); return 64; }
        if (!in_path) in_path = argv[i];
        else if (!out_path) out_path = argv[i];
        else { usage(); return 64; }
    }

    if (!in_path || !out_path) { usage(); return 64; }

    if (strict) threshold = 1.0;

    engine_cfg_t cfg = {
        .strict     = strict,
        .snps_only  = (strict && snps),
        .threshold  = threshold
    };

    fastio_init();

    int rc = engine_run(in_path, out_path, &cfg);
    if (rc != 0) fprintf(stderr, "BactCore: failed with code %d\n", rc);
    return rc;
}
