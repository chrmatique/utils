#ifndef GENERATE_H
#define GENERATE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MODE_WORDS,
    MODE_SENTENCES,
    MODE_PARAGRAPHS,
} GenMode;

typedef struct {
    GenMode mode;
    size_t count;
    uint64_t seed;
} GenOptions;

typedef struct {
    char *data;
    size_t len;
} GenResult;

int generate(const GenOptions *opts, GenResult *out);
void gen_result_free(GenResult *out);

#endif
