#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "generate.h"

#define SIZE_GB_THRESHOLD ((size_t)512 * 1024 * 1024)

static void usage(FILE *out) {
    fprintf(out,
            "Usage: ranfile [OPTIONS]\n"
            "\n"
            "Generate random lorem-style text.\n"
            "\n"
            "Options:\n"
            "  -w, --words N         Generate N words\n"
            "  -s, --sentences N     Generate N sentences\n"
            "  -p, --paragraphs N    Generate N paragraphs\n"
            "      --size SIZE       Generate text to reach SIZE bytes (B, MB, GB)\n"
            "      --markdown N      Generate N random Markdown sections\n"
            "  -c, --compress        Collapse output to a single line\n"
            "  -y, --no-confirm      Skip the large-file confirmation prompt\n"
            "  -o, --output FILE     Write output to FILE (default: stdout)\n"
            "      --seed U          64-bit seed for reproducible output\n"
            "  -h, --help            Show this help message\n"
            "\n"
            "Exactly one of -w, -s, -p, --size, or --markdown is required.\n");
}

static int parse_u64(const char *s, uint64_t *out) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = (uint64_t)value;
    return 0;
}

static int parse_count(const char *s, size_t *out) {
    uint64_t value = 0;
    if (parse_u64(s, &value) != 0 || value == 0) {
        return -1;
    }
    *out = (size_t)value;
    return 0;
}

static int parse_md_count(const char *s, size_t *out) {
    uint64_t value = 0;
    if (parse_u64(s, &value) != 0) {
        return -1;
    }
    *out = (size_t)value;
    return 0;
}

static int parse_data_size(const char *s, size_t *out) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }

    char *end = NULL;
    errno = 0;
    double value = strtod(s, &end);
    if (errno != 0 || end == s || value <= 0) {
        return -1;
    }

    while (*end == ' ' || *end == '\t') {
        end++;
    }

    /* Trim trailing whitespace so " 1 gb " is accepted. */
    char unit[8] = {0};
    size_t unit_len = 0;
    for (const char *p = end; *p && !isspace((unsigned char)*p) && unit_len < sizeof(unit) - 1; p++) {
        unit[unit_len++] = *p;
    }

    double multiplier = 1.0;
    if (strcasecmp(unit, "B") == 0) {
        multiplier = 1.0;
    } else if (strcasecmp(unit, "MB") == 0) {
        multiplier = 1024.0 * 1024.0;
    } else if (strcasecmp(unit, "GB") == 0) {
        multiplier = 1024.0 * 1024.0 * 1024.0;
    } else {
        return -1;
    }

    double bytes = value * multiplier;
    if (bytes > (double)SIZE_MAX) {
        return -1;
    }
    *out = (size_t)bytes;
    return 0;
}

static uint64_t default_seed(void) {
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        uint64_t seed = 0;
        if (fread(&seed, sizeof(seed), 1, urandom) == 1 && seed != 0) {
            fclose(urandom);
            return seed;
        }
        fclose(urandom);
    }
    return (uint64_t)time(NULL);
}

static int confirm_large_file(size_t bytes) {
    fprintf(stderr,
            "You are about to generate %.2f GB of data. Continue? [y/N] ",
            (double)bytes / (1024.0 * 1024.0 * 1024.0));
    fflush(stderr);

    char line[16];
    if (!fgets(line, sizeof(line), stdin)) {
        return 0;
    }
    return (line[0] == 'y' || line[0] == 'Y');
}

int main(int argc, char **argv) {
    GenMode mode = MODE_WORDS;
    int mode_set = 0;
    size_t count = 0;
    size_t target_size = 0;
    const char *output_path = NULL;
    uint64_t seed = 0;
    int seed_set = 0;
    int compress = 0;
    int no_confirm = 0;

    static struct option long_opts[] = {
        {"words", required_argument, NULL, 'w'},
        {"sentences", required_argument, NULL, 's'},
        {"paragraphs", required_argument, NULL, 'p'},
        {"size", required_argument, NULL, 257},
        {"markdown", required_argument, NULL, 258},
        {"compress", no_argument, NULL, 'c'},
        {"no-confirm", no_argument, NULL, 'y'},
        {"output", required_argument, NULL, 'o'},
        {"seed", required_argument, NULL, 256},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "w:s:p:o:cyh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'w':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, -p, --size, or --markdown\n");
                return 2;
            }
            if (parse_count(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid word count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_WORDS;
            mode_set = 1;
            break;
        case 's':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, -p, --size, or --markdown\n");
                return 2;
            }
            if (parse_count(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid sentence count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_SENTENCES;
            mode_set = 1;
            break;
        case 'p':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, -p, --size, or --markdown\n");
                return 2;
            }
            if (parse_count(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid paragraph count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_PARAGRAPHS;
            mode_set = 1;
            break;
        case 257:
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, -p, --size, or --markdown\n");
                return 2;
            }
            if (parse_data_size(optarg, &target_size) != 0) {
                fprintf(stderr, "ranfile: invalid size '%s'\n", optarg);
                return 2;
            }
            mode = MODE_SIZE;
            mode_set = 1;
            break;
        case 258:
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, -p, --size, or --markdown\n");
                return 2;
            }
            if (parse_md_count(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid markdown section count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_MARKDOWN;
            mode_set = 1;
            break;
        case 'c':
            compress = 1;
            break;
        case 'y':
            no_confirm = 1;
            break;
        case 'o':
            output_path = optarg;
            break;
        case 256:
            if (parse_u64(optarg, &seed) != 0) {
                fprintf(stderr, "ranfile: invalid seed '%s'\n", optarg);
                return 2;
            }
            seed_set = 1;
            break;
        case 'h':
            usage(stdout);
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }

    if (!mode_set) {
        fprintf(stderr, "ranfile: one of -w, -s, -p, --size, or --markdown is required\n");
        usage(stderr);
        return 2;
    }

    if (!seed_set) {
        seed = default_seed();
    }

    if (mode == MODE_SIZE && target_size >= SIZE_GB_THRESHOLD && !no_confirm) {
        if (!confirm_large_file(target_size)) {
            fprintf(stderr, "ranfile: aborted\n");
            return 2;
        }
    }

    GenOptions opts = {
        .mode = mode,
        .count = count,
        .seed = seed,
        .target_size = target_size,
    };

    GenResult result = {0};
    if (generate(&opts, &result) != 0) {
        fprintf(stderr, "ranfile: out of memory\n");
        return 1;
    }

    if (compress && gen_compress(&result) != 0) {
        fprintf(stderr, "ranfile: out of memory\n");
        gen_result_free(&result);
        return 1;
    }

    FILE *out = stdout;
    if (output_path) {
        out = fopen(output_path, "wb");
        if (!out) {
            fprintf(stderr, "ranfile: cannot open '%s': %s\n", output_path,
                    strerror(errno));
            gen_result_free(&result);
            return 1;
        }
    }

    if (result.len > 0 &&
        fwrite(result.data, 1, result.len, out) != result.len) {
        fprintf(stderr, "ranfile: write error%s\n",
                output_path ? "" : " on stdout");
        gen_result_free(&result);
        if (output_path) {
            fclose(out);
        }
        return 1;
    }

    gen_result_free(&result);
    if (output_path) {
        if (fclose(out) != 0) {
            fprintf(stderr, "ranfile: close error on '%s'\n", output_path);
            return 1;
        }
    }

    return 0;
}
