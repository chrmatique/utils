#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "generate.h"

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
            "  -o, --output FILE    Write output to FILE (default: stdout)\n"
            "      --seed U          64-bit seed for reproducible output\n"
            "  -h, --help            Show this help message\n"
            "\n"
            "Exactly one of -w, -s, or -p is required.\n");
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

static int parse_size(const char *s, size_t *out) {
    uint64_t value = 0;
    if (parse_u64(s, &value) != 0 || value == 0) {
        return -1;
    }
    *out = (size_t)value;
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

int main(int argc, char **argv) {
    GenMode mode = MODE_WORDS;
    int mode_set = 0;
    size_t count = 0;
    const char *output_path = NULL;
    uint64_t seed = 0;
    int seed_set = 0;

    static struct option long_opts[] = {
        {"words", required_argument, NULL, 'w'},
        {"sentences", required_argument, NULL, 's'},
        {"paragraphs", required_argument, NULL, 'p'},
        {"output", required_argument, NULL, 'o'},
        {"seed", required_argument, NULL, 256},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "w:s:p:o:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'w':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, or -p\n");
                return 2;
            }
            if (parse_size(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid word count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_WORDS;
            mode_set = 1;
            break;
        case 's':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, or -p\n");
                return 2;
            }
            if (parse_size(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid sentence count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_SENTENCES;
            mode_set = 1;
            break;
        case 'p':
            if (mode_set) {
                fprintf(stderr, "ranfile: specify only one of -w, -s, or -p\n");
                return 2;
            }
            if (parse_size(optarg, &count) != 0) {
                fprintf(stderr, "ranfile: invalid paragraph count '%s'\n", optarg);
                return 2;
            }
            mode = MODE_PARAGRAPHS;
            mode_set = 1;
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
        fprintf(stderr, "ranfile: one of -w, -s, or -p is required\n");
        usage(stderr);
        return 2;
    }

    if (!seed_set) {
        seed = default_seed();
    }

    GenOptions opts = {
        .mode = mode,
        .count = count,
        .seed = seed,
    };

    GenResult result = {0};
    if (generate(&opts, &result) != 0) {
        fprintf(stderr, "ranfile: out of memory\n");
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
