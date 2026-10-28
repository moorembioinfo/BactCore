#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SIMD_ENABLED
#include <immintrin.h>
#endif

#include "engine.h"
#include "fastio.h"

#ifndef BC_BUFSZ
#define BC_BUFSZ (1u << 22)
#endif

#define INITIAL_COLUMNS (1u << 20)

typedef enum { PS_IN_HEADER = 0, PS_IN_SEQ = 1 } parse_state_t;

typedef struct {
    size_t length;
    size_t sequences;
    uint16_t *invalid16;
    uint64_t *keep;
    uint64_t *variable;
    uint8_t *reference;
} first_pass_t;

static inline int bit_is_set(const uint64_t *bits, size_t column) {
    return (int)((bits[column >> 6] >> (column & 63)) & 1u);
}

static inline void bit_set(uint64_t *bits, size_t column) {
    bits[column >> 6] |= UINT64_C(1) << (column & 63);
}

static inline void bit_clear(uint64_t *bits, size_t column) {
    bits[column >> 6] &= ~(UINT64_C(1) << (column & 63));
}

static inline void bits_or32(uint64_t *bits, size_t column, uint32_t values) {
    const size_t word = column >> 6;
    const unsigned shift = (unsigned)(column & 63);
    bits[word] |= (uint64_t)values << shift;
    if (shift > 32)
        bits[word + 1] |= (uint64_t)values >> (64 - shift);
}

static inline void bits_clear32(uint64_t *bits, size_t column,
                                uint32_t values) {
    const size_t word = column >> 6;
    const unsigned shift = (unsigned)(column & 63);
    bits[word] &= ~((uint64_t)values << shift);
    if (shift > 32)
        bits[word + 1] &= ~((uint64_t)values >> (64 - shift));
}

static inline uint32_t bits_get_lanes(const uint64_t *bits, size_t column,
                                      unsigned lanes) {
    const size_t word = column >> 6;
    const unsigned shift = (unsigned)(column & 63);
    uint64_t values = bits[word] >> shift;
    if (shift + lanes > 64)
        values |= bits[word + 1] << (64 - shift);
    const uint32_t lane_mask = lanes == 32
        ? UINT32_MAX
        : (UINT32_C(1) << lanes) - 1;
    return (uint32_t)values & lane_mask;
}

#ifdef SIMD_ENABLED
static inline __m256i uppercase32(__m256i input) {
    return _mm256_and_si256(input, _mm256_set1_epi8((char)0xdf));
}

static inline __m256i valid_vector32(__m256i uppercase) {
    __m256i valid = _mm256_cmpeq_epi8(
        uppercase, _mm256_set1_epi8('A'));
    valid = _mm256_or_si256(
        valid, _mm256_cmpeq_epi8(uppercase, _mm256_set1_epi8('C')));
    valid = _mm256_or_si256(
        valid, _mm256_cmpeq_epi8(uppercase, _mm256_set1_epi8('G')));
    return _mm256_or_si256(
        valid, _mm256_cmpeq_epi8(uppercase, _mm256_set1_epi8('T')));
}

static inline uint32_t special_mask32(__m256i input) {
    __m256i special = _mm256_cmpeq_epi8(input, _mm256_set1_epi8('>'));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8(' ')));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8('\t')));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8('\n')));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8('\r')));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8('\f')));
    special = _mm256_or_si256(
        special, _mm256_cmpeq_epi8(input, _mm256_set1_epi8('\v')));
    return (uint32_t)_mm256_movemask_epi8(special);
}

static inline void add_invalid16(uint16_t *counts, size_t column,
                                 uint32_t invalid) {
    while (invalid) {
        const unsigned offset = (unsigned)__builtin_ctz(invalid);
        uint16_t *count = &counts[column + offset];
        if (*count != UINT16_MAX) ++*count;
        invalid &= invalid - 1;
    }
}
#endif

static int next_capacity(size_t current, size_t needed, size_t *result) {
    size_t capacity = current ? current : INITIAL_COLUMNS;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity < needed) return -1;
    *result = capacity;
    return 0;
}

static int grow_flexible(uint16_t **counts, size_t old_capacity,
                         size_t new_capacity) {
    if (new_capacity > SIZE_MAX / sizeof(**counts)) return -1;
    uint16_t *grown = (uint16_t *)realloc(
        *counts, new_capacity * sizeof(**counts));
    if (!grown) return -1;
    memset(grown + old_capacity, 0,
           (new_capacity - old_capacity) * sizeof(*grown));
    *counts = grown;
    return 0;
}

static int grow_strict(uint64_t **keep, uint64_t **variable,
                       uint8_t **reference, size_t old_capacity,
                       size_t new_capacity, int snps_only) {
    if (new_capacity > SIZE_MAX - 63) return -1;
    const size_t old_words = (old_capacity + 63) / 64;
    const size_t new_words = (new_capacity + 63) / 64;
    if (new_words > SIZE_MAX / sizeof(**keep)) return -1;

    uint64_t *grown_keep =
        (uint64_t *)realloc(*keep, new_words * sizeof(**keep));
    if (!grown_keep) return -1;
    memset(grown_keep + old_words, 0,
           (new_words - old_words) * sizeof(*grown_keep));
    *keep = grown_keep;

    if (!snps_only) return 0;

    uint64_t *grown_variable =
        (uint64_t *)realloc(*variable, new_words * sizeof(**variable));
    if (!grown_variable) return -1;
    memset(grown_variable + old_words, 0,
           (new_words - old_words) * sizeof(*grown_variable));
    *variable = grown_variable;

    uint8_t *grown_reference =
        (uint8_t *)realloc(*reference, new_capacity);
    if (!grown_reference) return -1;
    memset(grown_reference + old_capacity, 0,
           new_capacity - old_capacity);
    *reference = grown_reference;
    return 0;
}

static int finish_sequence(size_t column, size_t *length,
                           size_t *sequences) {
    if (*sequences == 0) {
        if (column == 0) {
            fprintf(stderr, "Empty first FASTA sequence.\n");
            return -1;
        }
        *length = column;
    } else if (column != *length) {
        fprintf(stderr, "Unequal sequence lengths (%zu vs %zu)\n",
                column, *length);
        return -1;
    }
    if (*sequences == SIZE_MAX) {
        fprintf(stderr, "Too many sequences.\n");
        return -1;
    }
    ++*sequences;
    return 0;
}

/*
 * Count only invalid calls. Valid bacterial alignments are normally sparse in
 * gaps/ambiguities, so this avoids a counter write for almost every input byte.
 * Strict mode needs only a one-bit keep mask. SNP mode adds a byte reference
 * and a one-bit variability mask; the byte lookup is deliberately faster than
 * unpacking two-bit bases in the hot loop.
 */
static int first_pass(const char *path, int strict_mode, int snps_only,
                      int need_counts, first_pass_t *result) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        perror("open input");
        return 11;
    }
    unsigned char *buffer = (unsigned char *)malloc(BC_BUFSZ);
    if (!buffer) {
        perror("malloc input buffer");
        fclose(input);
        return 10;
    }

    size_t capacity = 0;
    size_t length = 0;
    size_t sequences = 0;
    size_t column = 0;
    int have_sequence = 0;
    parse_state_t state = PS_IN_HEADER;
    int rc = 0;

    for (;;) {
        const size_t bytes = fread(buffer, 1, BC_BUFSZ, input);
        if (bytes == 0) break;
        for (size_t i = 0; i < bytes;) {
            const unsigned char c = buffer[i];
#ifdef SIMD_ENABLED
            if (state == PS_IN_SEQ && bytes - i >= 32) {
                const __m256i input =
                    _mm256_loadu_si256((const __m256i *)(buffer + i));
                const uint32_t special = special_mask32(input);
                const unsigned lanes = special
                    ? (unsigned)__builtin_ctz(special)
                    : 32;
                if (lanes != 0) {
                    if (sequences != 0 && length - column < lanes) {
                        fprintf(stderr,
                                "Sequence is longer than the first (%zu).\n",
                                length);
                        rc = 2;
                        goto done;
                    }
                    if (sequences == 0 && capacity - column < lanes) {
                        size_t new_capacity;
                        if (column > SIZE_MAX - lanes ||
                            next_capacity(capacity, column + lanes,
                                          &new_capacity) != 0) {
                            fprintf(stderr, "Alignment is too wide.\n");
                            rc = 10;
                            goto done;
                        }
                        int grow_rc = 0;
                        if (strict_mode) {
                            grow_rc = grow_strict(
                                &result->keep, &result->variable,
                                &result->reference, capacity, new_capacity,
                                snps_only);
                        } else if (need_counts) {
                            grow_rc = grow_flexible(&result->invalid16,
                                                    capacity, new_capacity);
                        }
                        if (grow_rc != 0) {
                            perror("grow column state");
                            rc = 10;
                            goto done;
                        }
                        capacity = new_capacity;
                    }

                    const __m256i uppercase = uppercase32(input);
                    const __m256i valid_vector = valid_vector32(uppercase);
                    const uint32_t lane_mask = lanes == 32
                        ? UINT32_MAX
                        : (UINT32_C(1) << lanes) - 1;
                    const uint32_t valid = (uint32_t)
                        _mm256_movemask_epi8(valid_vector) & lane_mask;
                    const uint32_t invalid = ~valid & lane_mask;
                    if (strict_mode) {
                        if (sequences == 0) {
                            bits_or32(result->keep, column, valid);
                            if (snps_only) {
                                if (lanes == 32) {
                                    _mm256_storeu_si256(
                                        (__m256i *)(result->reference + column),
                                        uppercase);
                                } else {
                                    unsigned char normalized[32];
                                    _mm256_storeu_si256(
                                        (__m256i *)normalized, uppercase);
                                    memcpy(result->reference + column,
                                           normalized, lanes);
                                }
                            }
                        } else {
                            bits_clear32(result->keep, column, invalid);
                            if (snps_only) {
                                unsigned char reference_bytes[32] = {0};
                                if (lanes == 32) {
                                    memcpy(reference_bytes,
                                           result->reference + column, 32);
                                } else {
                                    memcpy(reference_bytes,
                                           result->reference + column, lanes);
                                }
                                const __m256i reference = _mm256_loadu_si256(
                                    (const __m256i *)reference_bytes);
                                const uint32_t equal = (uint32_t)
                                    _mm256_movemask_epi8(_mm256_cmpeq_epi8(
                                        uppercase, reference));
                                bits_or32(result->variable, column,
                                          valid & ~equal & lane_mask);
                            }
                        }
                    } else if (need_counts && invalid) {
                        add_invalid16(result->invalid16, column, invalid);
                    }
                    column += lanes;
                    i += lanes;
                    continue;
                }
            }
#endif
            if (c == '>') {
                if (have_sequence &&
                    finish_sequence(column, &length, &sequences) != 0) {
                    rc = 2;
                    goto done;
                }
                have_sequence = 1;
                column = 0;
                state = PS_IN_HEADER;
                ++i;
                continue;
            }
            if (state == PS_IN_HEADER) {
                if (c == '\n') state = PS_IN_SEQ;
                ++i;
                continue;
            }

            const unsigned char mapped = FASTIO_OUT_MAP[c];
            if (!mapped) {
                ++i;
                continue;
            }

            if (sequences != 0 && column >= length) {
                fprintf(stderr, "Sequence is longer than the first (%zu).\n",
                        length);
                rc = 2;
                goto done;
            }
            if (sequences == 0 && column == capacity) {
                size_t new_capacity;
                if (next_capacity(capacity, column + 1, &new_capacity) != 0) {
                    fprintf(stderr, "Alignment is too wide.\n");
                    rc = 10;
                    goto done;
                }
                int grow_rc = 0;
                if (strict_mode) {
                    grow_rc = grow_strict(&result->keep, &result->variable,
                                          &result->reference, capacity,
                                          new_capacity, snps_only);
                } else if (need_counts) {
                    grow_rc = grow_flexible(&result->invalid16, capacity,
                                            new_capacity);
                }
                if (grow_rc != 0) {
                    perror("grow column state");
                    rc = 10;
                    goto done;
                }
                capacity = new_capacity;
            }

            const uint8_t valid = FASTIO_VALID_BITS[c];
            if (strict_mode) {
                if (sequences == 0) {
                    if (valid) {
                        bit_set(result->keep, column);
                        if (snps_only)
                            result->reference[column] = mapped;
                    }
                } else if (!valid) {
                    bit_clear(result->keep, column);
                } else if (snps_only &&
                           mapped != result->reference[column]) {
                    bit_set(result->variable, column);
                }
            } else if (need_counts && !valid) {
                uint16_t *count = &result->invalid16[column];
                if (*count != UINT16_MAX) ++*count;
            }
            ++column;
            ++i;
        }
    }

    if (ferror(input)) {
        perror("read input");
        rc = 11;
        goto done;
    }
    if (!have_sequence) {
        fprintf(stderr, "Empty FASTA or parse error.\n");
        rc = 3;
        goto done;
    }
    if (finish_sequence(column, &length, &sequences) != 0) {
        rc = 2;
        goto done;
    }

    result->length = length;
    result->sequences = sequences;

done:
    free(buffer);
    fclose(input);
    return rc;
}

static inline uint64_t ceil_mul(double fraction, uint64_t count) {
    const double value = fraction * (double)count;
    const uint64_t truncated = (uint64_t)value;
    return truncated == value ? truncated : truncated + 1;
}

static int recount_invalid32(const char *path, size_t length,
                             size_t expected_sequences, uint32_t *counts) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        perror("reopen input");
        return 11;
    }
    unsigned char *buffer = (unsigned char *)malloc(BC_BUFSZ);
    if (!buffer) {
        perror("malloc input buffer");
        fclose(input);
        return 10;
    }

    size_t sequences = 0;
    size_t column = 0;
    int have_sequence = 0;
    parse_state_t state = PS_IN_HEADER;
    int rc = 0;
    for (;;) {
        const size_t bytes = fread(buffer, 1, BC_BUFSZ, input);
        if (bytes == 0) break;
        for (size_t i = 0; i < bytes; ++i) {
            const unsigned char c = buffer[i];
            if (c == '>') {
                if (have_sequence) {
                    if (column != length) {
                        fprintf(stderr,
                                "Input changed while it was being processed.\n");
                        rc = 2;
                        goto done;
                    }
                    ++sequences;
                }
                have_sequence = 1;
                column = 0;
                state = PS_IN_HEADER;
                continue;
            }
            if (state == PS_IN_HEADER) {
                if (c == '\n') state = PS_IN_SEQ;
                continue;
            }
            if (!FASTIO_OUT_MAP[c]) continue;
            if (column >= length) {
                fprintf(stderr,
                        "Input changed while it was being processed.\n");
                rc = 2;
                goto done;
            }
            if (!FASTIO_VALID_BITS[c] && counts[column] != UINT32_MAX)
                ++counts[column];
            ++column;
        }
    }
    if (ferror(input)) {
        perror("read input");
        rc = 11;
        goto done;
    }
    if (have_sequence) {
        if (column != length) {
            fprintf(stderr, "Input changed while it was being processed.\n");
            rc = 2;
            goto done;
        }
        ++sequences;
    }
    if (sequences != expected_sequences) {
        fprintf(stderr, "Input changed while it was being processed.\n");
        rc = 2;
    }

done:
    free(buffer);
    fclose(input);
    return rc;
}

static int write_output(const char *input_path, const char *output_path,
                        const uint64_t *mask, size_t length) {
    FILE *input = fopen(input_path, "rb");
    if (!input) {
        perror("reopen input");
        return 13;
    }
    FILE *output = fopen(output_path, "wb");
    if (!output) {
        perror("open output");
        fclose(input);
        return 14;
    }

    unsigned char *input_buffer = (unsigned char *)malloc(BC_BUFSZ);
    unsigned char *output_buffer = (unsigned char *)malloc(BC_BUFSZ);
    if (!input_buffer || !output_buffer) {
        perror("malloc I/O buffers");
        free(input_buffer);
        free(output_buffer);
        fclose(input);
        fclose(output);
        return 15;
    }

    parse_state_t state = PS_IN_HEADER;
    size_t column = 0;
    size_t output_bytes = 0;
    int have_sequence = 0;
    int rc = 0;

    for (;;) {
        const size_t bytes = fread(input_buffer, 1, BC_BUFSZ, input);
        if (bytes == 0) break;
        for (size_t i = 0; i < bytes;) {
            const unsigned char c = input_buffer[i];
#ifdef SIMD_ENABLED
            if (state == PS_IN_SEQ && bytes - i >= 32) {
                const __m256i input = _mm256_loadu_si256(
                    (const __m256i *)(input_buffer + i));
                const uint32_t special = special_mask32(input);
                const unsigned lanes = special
                    ? (unsigned)__builtin_ctz(special)
                    : 32;
                if (lanes != 0) {
                    if (length - column < lanes) {
                        fprintf(stderr,
                                "Input changed while it was being processed.\n");
                        rc = 2;
                        goto done;
                    }
                    const uint32_t lane_mask = lanes == 32
                        ? UINT32_MAX
                        : (UINT32_C(1) << lanes) - 1;
                    const uint32_t selected =
                        bits_get_lanes(mask, column, lanes);
                    if (selected && output_bytes == BC_BUFSZ) {
                        if (fwrite(output_buffer, 1, output_bytes, output) !=
                            output_bytes) {
                            perror("write output");
                            rc = 16;
                            goto done;
                        }
                        output_bytes = 0;
                    }
                    if (selected == lane_mask) {
                        if (BC_BUFSZ - output_bytes < lanes) {
                            if (fwrite(output_buffer, 1, output_bytes,
                                       output) != output_bytes) {
                                perror("write output");
                                rc = 16;
                                goto done;
                            }
                            output_bytes = 0;
                        }
                        const __m256i uppercase = uppercase32(input);
                        const __m256i valid = valid_vector32(uppercase);
                        const __m256i normalized = _mm256_blendv_epi8(
                            _mm256_set1_epi8('-'), uppercase, valid);
                        if (lanes == 32) {
                            _mm256_storeu_si256(
                                (__m256i *)(output_buffer + output_bytes),
                                normalized);
                        } else {
                            unsigned char normalized_bytes[32];
                            _mm256_storeu_si256(
                                (__m256i *)normalized_bytes, normalized);
                            memcpy(output_buffer + output_bytes,
                                   normalized_bytes, lanes);
                        }
                        output_bytes += lanes;
                    } else if (selected) {
                        uint32_t remaining = selected;
                        while (remaining) {
                            const unsigned offset =
                                (unsigned)__builtin_ctz(remaining);
                            output_buffer[output_bytes++] =
                                FASTIO_OUT_MAP[input_buffer[i + offset]];
                            if (output_bytes == BC_BUFSZ) {
                                if (fwrite(output_buffer, 1, output_bytes,
                                           output) != output_bytes) {
                                    perror("write output");
                                    rc = 16;
                                    goto done;
                                }
                                output_bytes = 0;
                            }
                            remaining &= remaining - 1;
                        }
                    }
                    column += lanes;
                    i += lanes;
                    continue;
                }
            }
#endif
            if (c == '>') {
                if (have_sequence) {
                    if (column != length) {
                        fprintf(stderr,
                                "Input changed while it was being processed.\n");
                        rc = 2;
                        goto done;
                    }
                    if (output_bytes) {
                        if (fwrite(output_buffer, 1, output_bytes, output) !=
                            output_bytes) {
                            perror("write output");
                            rc = 16;
                            goto done;
                        }
                        output_bytes = 0;
                    }
                    if (fputc('\n', output) == EOF) {
                        perror("write output");
                        rc = 16;
                        goto done;
                    }
                }
                if (fputc('>', output) == EOF) {
                    perror("write output");
                    rc = 16;
                    goto done;
                }
                have_sequence = 1;
                column = 0;
                state = PS_IN_HEADER;
                ++i;
                continue;
            }
            if (state == PS_IN_HEADER) {
                if (fputc((int)c, output) == EOF) {
                    perror("write output");
                    rc = 16;
                    goto done;
                }
                if (c == '\n') state = PS_IN_SEQ;
                ++i;
                continue;
            }

            const unsigned char mapped = FASTIO_OUT_MAP[c];
            if (!mapped) {
                ++i;
                continue;
            }
            if (column >= length) {
                fprintf(stderr, "Input changed while it was being processed.\n");
                rc = 2;
                goto done;
            }
            if (bit_is_set(mask, column)) {
                output_buffer[output_bytes++] = mapped;
                if (output_bytes == BC_BUFSZ) {
                    if (fwrite(output_buffer, 1, output_bytes, output) !=
                        output_bytes) {
                        perror("write output");
                        rc = 16;
                        goto done;
                    }
                    output_bytes = 0;
                }
            }
            ++column;
            ++i;
        }
    }

    if (ferror(input)) {
        perror("read input");
        rc = 13;
        goto done;
    }
    if (have_sequence) {
        if (column != length) {
            fprintf(stderr, "Input changed while it was being processed.\n");
            rc = 2;
            goto done;
        }
        if (output_bytes &&
            fwrite(output_buffer, 1, output_bytes, output) != output_bytes) {
            perror("write output");
            rc = 16;
            goto done;
        }
        if (fputc('\n', output) == EOF) {
            perror("write output");
            rc = 16;
            goto done;
        }
    }

done:
    if (fclose(input) != 0 && rc == 0) {
        perror("close input");
        rc = 13;
    }
    if (fclose(output) != 0 && rc == 0) {
        perror("close output");
        rc = 16;
    }
    free(input_buffer);
    free(output_buffer);
    return rc;
}

int engine_run(const char *input_path, const char *output_path,
               const engine_cfg_t *cfg) {
    assert(cfg);
    fastio_init();

    const int strict_mode = cfg->strict || cfg->threshold >= 1.0;
    const int snps_only = strict_mode && cfg->snps_only;
    const int need_counts = !strict_mode && cfg->threshold > 0.0;

    first_pass_t pass = {0};
    int rc = first_pass(input_path, strict_mode, snps_only, need_counts, &pass);
    if (rc != 0) goto done;
    if (pass.length > SIZE_MAX - 63) {
        fprintf(stderr, "Alignment is too wide.\n");
        rc = 10;
        goto done;
    }

    const size_t words = (pass.length + 63) / 64;
    uint64_t *mask = pass.keep;

    if (strict_mode) {
        if (snps_only) {
            for (size_t i = 0; i < words; ++i)
                mask[i] &= pass.variable[i];
        }
    } else {
        mask = (uint64_t *)calloc(words, sizeof(*mask));
        if (!mask) {
            perror("calloc mask");
            rc = 12;
            goto done;
        }

        if (!need_counts) {
            memset(mask, 0xff, words * sizeof(*mask));
        } else {
            const uint64_t sequence_count = (uint64_t)pass.sequences;
            const uint64_t required_valid =
                ceil_mul(cfg->threshold, sequence_count);
            const uint64_t allowed_invalid =
                sequence_count - required_valid;

            if (allowed_invalid < UINT16_MAX) {
                for (size_t column = 0; column < pass.length; ++column) {
                    if (pass.invalid16[column] <= allowed_invalid)
                        bit_set(mask, column);
                }
            } else {
                if (pass.length > SIZE_MAX / sizeof(uint32_t)) {
                    fprintf(stderr, "Alignment is too wide.\n");
                    rc = 10;
                    free(mask);
                    goto done;
                }
                uint32_t *invalid32 =
                    (uint32_t *)calloc(pass.length, sizeof(*invalid32));
                if (!invalid32) {
                    perror("calloc 32-bit counters");
                    rc = 10;
                    free(mask);
                    goto done;
                }
                rc = recount_invalid32(input_path, pass.length,
                                       pass.sequences, invalid32);
                if (rc == 0) {
                    for (size_t column = 0; column < pass.length; ++column) {
                        if ((uint64_t)invalid32[column] <= allowed_invalid)
                            bit_set(mask, column);
                    }
                }
                free(invalid32);
                if (rc != 0) {
                    free(mask);
                    goto done;
                }
            }
        }
    }

    free(pass.invalid16);
    pass.invalid16 = NULL;
    free(pass.variable);
    pass.variable = NULL;
    free(pass.reference);
    pass.reference = NULL;

    rc = write_output(input_path, output_path, mask, pass.length);
    if (!strict_mode) free(mask);

done:
    free(pass.invalid16);
    free(pass.keep);
    free(pass.variable);
    free(pass.reference);
    return rc;
}
