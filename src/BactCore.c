#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INVALID_CHARS "N-"
#define VALID_NUCLEOTIDES "ATCG"

/*
Matthew P. Moore 2024
https://github.com/moorembioinfo/BactCore
Compile with: 
gcc -O3 -o BctCore BactCore.c
Compile for optimal speed:
gcc -O3 -march=native -flto -funroll-loops -ffast-math -o BactCore BactCore.c
moorembioinfo/BactCore is licensed under the
GNU General Public License v3.0
*/

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [options] input.fasta [output.fasta]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --strict     Retain only columns with 0%% invalid characters.\n");
    fprintf(stderr, "  --snps       Retain only columns with variability among ATCG.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int arg_index = 1;
    bool snps_mode = false;
    double invalid_threshold = 5.0; //  5% cutoff

    // ** Command-Line **
    if (argc < 2) {
        usage(argv[0]);
    }

    // Parse flags
    while (arg_index < argc && argv[arg_index][0] == '-') {
        if (strcmp(argv[arg_index], "--strict") == 0) {
            invalid_threshold = 0.0; // Set threshold to 0%
        } else if (strcmp(argv[arg_index], "--snps") == 0) {
            snps_mode = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_index]);
            usage(argv[0]);
        }
        arg_index++;
    }

    if (argc - arg_index < 1) {
        usage(argv[0]);
    }

    const char *input_filename = argv[arg_index++];
    const char *output_filename = (argc > arg_index) ? argv[arg_index] : NULL;

    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    FILE *output_file = stdout;
    if (output_filename) {
        output_file = fopen(output_filename, "w");
        if (!output_file) {
            perror("Error opening output file");
            fclose(input_file);
            return EXIT_FAILURE;
        }
    }

    // ** Two-Pass Approach **
    // ** First Pass: Count gaps **
    size_t sequence_length = 0;
    size_t num_sequences = 0;
    int *invalid_counts = NULL;
    bool **nucleotide_presence = NULL; 

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool in_sequence = false;
    size_t seq_pos = 0;

    while ((read = getline(&line, &len, input_file)) != -1) {
        if (line[0] == '>') {
            num_sequences++;
            in_sequence = true;
            seq_pos = 0;
        } else if (in_sequence) {
            size_t line_len = strcspn(line, "\r\n");
            line[line_len] = '\0';

            if (sequence_length == 0) {
                sequence_length = line_len;
                invalid_counts = calloc(sequence_length, sizeof(int));
                nucleotide_presence = malloc(sequence_length * sizeof(bool *));
                if (!invalid_counts || !nucleotide_presence) {
                    perror("Memory allocation failed");
                    free(line);
                    fclose(input_file);
                    if (output_file != stdout) fclose(output_file);
                    return EXIT_FAILURE;
                }
                for (size_t i = 0; i < sequence_length; i++) {
                    nucleotide_presence[i] = calloc(4, sizeof(bool)); // A, T, C, G
                    if (!nucleotide_presence[i]) {
                        perror("Memory allocation failed");
                        free(line);
                        fclose(input_file);
                        if (output_file != stdout) fclose(output_file);
                        // Free previously allocated memory
                        for (size_t k = 0; k < i; k++) {
                            free(nucleotide_presence[k]);
                        }
                        free(nucleotide_presence);
                        free(invalid_counts);
                        return EXIT_FAILURE;
                    }
                }
            }

            for (size_t i = 0; i < line_len; i++) {
                char c = line[i];
                if (strchr(INVALID_CHARS, c)) {
                    invalid_counts[seq_pos + i]++;
                } else if (strchr(VALID_NUCLEOTIDES, c)) {
                    switch (c) {
                        case 'A':
                            nucleotide_presence[seq_pos + i][0] = true;
                            break;
                        case 'T':
                            nucleotide_presence[seq_pos + i][1] = true;
                            break;
                        case 'C':
                            nucleotide_presence[seq_pos + i][2] = true;
                            break;
                        case 'G':
                            nucleotide_presence[seq_pos + i][3] = true;
                            break;
                        default:
                            break;
                    }
                }
            }
            seq_pos += line_len;
        }
    }
    free(line);
    fclose(input_file);

    bool *columns_to_keep = malloc(sequence_length * sizeof(bool));
    if (!columns_to_keep) {
        perror("Memory allocation failed");
        free(invalid_counts);
        for (size_t i = 0; i < sequence_length; i++) {
            free(nucleotide_presence[i]);
        }
        free(nucleotide_presence);
        if (output_file != stdout) fclose(output_file);
        return EXIT_FAILURE;
    }

    size_t new_sequence_length = 0;
    for (size_t i = 0; i < sequence_length; i++) {
        double percentage = (invalid_counts[i] / (double)num_sequences) * 100.0;
        bool keep_column = percentage <= invalid_threshold;

        if (snps_mode && keep_column) {
            // Check for variability among nucleotides
            int nucleotide_count = 0;
            for (int j = 0; j < 4; j++) {
                if (nucleotide_presence[i][j]) {
                    nucleotide_count++;
                }
            }
            if (nucleotide_count <= 1) {
                keep_column = false; // No variability
            }
        }

        columns_to_keep[i] = keep_column;
        if (keep_column) {
            new_sequence_length++;
        }
    }
    free(invalid_counts);
    for (size_t i = 0; i < sequence_length; i++) {
        free(nucleotide_presence[i]);
    }
    free(nucleotide_presence);

    // ** Second Pass: Output filtered sites **
    input_file = fopen(input_filename, "r");
    if (!input_file) {
        perror("Error reopening input file");
        free(columns_to_keep);
        if (output_file != stdout) fclose(output_file);
        return EXIT_FAILURE;
    }

    line = NULL;
    len = 0;
    in_sequence = false;
    seq_pos = 0;

    while ((read = getline(&line, &len, input_file)) != -1) {
        if (line[0] == '>') {
            fputs(line, output_file);
            in_sequence = true;
            seq_pos = 0;
        } else if (in_sequence) {
            size_t line_len = strcspn(line, "\r\n");
            line[line_len] = '\0'; // Remove newline characters

            for (size_t i = 0; i < line_len; i++) {
                if (columns_to_keep[seq_pos + i]) {
                    fputc(line[i], output_file);
                }
            }
            fputc('\n', output_file);
            seq_pos += line_len;
        }
    }
    free(line);
    fclose(input_file);
    free(columns_to_keep);

    if (output_file != stdout) fclose(output_file);
    return EXIT_SUCCESS;
}
