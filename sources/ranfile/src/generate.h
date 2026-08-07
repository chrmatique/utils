#ifndef GENERATE_H
#define GENERATE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MODE_WORDS,
    MODE_SENTENCES,
    MODE_PARAGRAPHS,
    MODE_SIZE,
    MODE_MARKDOWN,
} GenMode;

typedef struct {
    GenMode mode;
    size_t count;
    uint64_t seed;
    size_t target_size; /* bytes, used when mode == MODE_SIZE */
} GenOptions;

typedef struct {
    char *data;
    size_t len;
} GenResult;

int generate(const GenOptions *opts, GenResult *out);
void gen_result_free(GenResult *out);

/* Compress whitespace/newlines into a single space; trailing newline preserved. */
int gen_compress(GenResult *out);

#endif
