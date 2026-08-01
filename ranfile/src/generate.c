#include "generate.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "rng.h"
#include "words.h"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static int buf_reserve(Buf *buf, size_t extra) {
    size_t need = buf->len + extra;
    if (need <= buf->cap) {
        return 0;
    }

    size_t new_cap = buf->cap ? buf->cap : 256;
    while (new_cap < need) {
        new_cap *= 2;
    }

    char *next = realloc(buf->data, new_cap);
    if (!next) {
        return -1;
    }

    buf->data = next;
    buf->cap = new_cap;
    return 0;
}

static int buf_append(Buf *buf, const char *src, size_t n) {
    if (buf_reserve(buf, n) != 0) {
        return -1;
    }
    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return 0;
}

static int buf_append_byte(Buf *buf, char c) {
    return buf_append(buf, &c, 1);
}

static int buf_append_cstr(Buf *buf, const char *s) {
    return buf_append(buf, s, strlen(s));
}

static size_t estimate_capacity(GenMode mode, size_t count) {
    switch (mode) {
    case MODE_WORDS:
        return count * 8 + 1;
    case MODE_SENTENCES:
        return count * 80 + 1;
    case MODE_PARAGRAPHS:
        return count * 400 + 1;
    }
    return 256;
}

static char sentence_punct(uint64_t *rng) {
    uint64_t roll = rng_bounded(rng, 100);
    if (roll < 85) {
        return '.';
    }
    if (roll < 95) {
        return '?';
    }
    return '!';
}

static int append_word(Buf *buf, size_t word_idx, int capitalize) {
    const char *word = WORDS[word_idx];
    size_t len = WORD_LENS[word_idx];

    if (capitalize && len > 0) {
        char upper = (char)(word[0] - 'a' + 'A');
        if (buf_append(buf, &upper, 1) != 0) {
            return -1;
        }
        if (len > 1 && buf_append(buf, word + 1, len - 1) != 0) {
            return -1;
        }
        return 0;
    }

    return buf_append(buf, word, len);
}

static int append_sentence(Buf *buf, uint64_t *rng) {
    size_t word_count = (size_t)(SENTENCE_WORDS_MIN +
        rng_bounded(rng, SENTENCE_WORDS_MAX - SENTENCE_WORDS_MIN + 1));

    for (size_t i = 0; i < word_count; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        if (append_word(buf, idx, i == 0) != 0) {
            return -1;
        }
    }

    char punct = sentence_punct(rng);
    if (buf_append_byte(buf, punct) != 0) {
        return -1;
    }
    return 0;
}

static int generate_words(Buf *buf, uint64_t *rng, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        if (append_word(buf, idx, 0) != 0) {
            return -1;
        }
    }
    return buf_append_byte(buf, '\n');
}

static int generate_sentences(Buf *buf, uint64_t *rng, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        if (append_sentence(buf, rng) != 0) {
            return -1;
        }
    }
    return buf_append_byte(buf, '\n');
}

static int generate_paragraphs(Buf *buf, uint64_t *rng, size_t count) {
    for (size_t p = 0; p < count; p++) {
        if (p > 0 && buf_append_cstr(buf, "\n\n") != 0) {
            return -1;
        }

        size_t sentence_count = (size_t)(PARAGRAPH_SENTENCES_MIN +
            rng_bounded(rng, PARAGRAPH_SENTENCES_MAX - PARAGRAPH_SENTENCES_MIN + 1));

        for (size_t s = 0; s < sentence_count; s++) {
            if (s > 0 && buf_append_byte(buf, ' ') != 0) {
                return -1;
            }
            if (append_sentence(buf, rng) != 0) {
                return -1;
            }
        }
    }
    return buf_append_byte(buf, '\n');
}

int generate(const GenOptions *opts, GenResult *out) {
    Buf buf = {0};
    buf.cap = estimate_capacity(opts->mode, opts->count);
    buf.data = malloc(buf.cap);
    if (!buf.data) {
        return -1;
    }

    uint64_t rng = opts->seed;
    if (rng == 0) {
        rng = 1;
    }

    int rc = 0;
    switch (opts->mode) {
    case MODE_WORDS:
        rc = generate_words(&buf, &rng, opts->count);
        break;
    case MODE_SENTENCES:
        rc = generate_sentences(&buf, &rng, opts->count);
        break;
    case MODE_PARAGRAPHS:
        rc = generate_paragraphs(&buf, &rng, opts->count);
        break;
    }

    if (rc != 0) {
        free(buf.data);
        out->data = NULL;
        out->len = 0;
        return -1;
    }

    out->data = buf.data;
    out->len = buf.len;
    return 0;
}

void gen_result_free(GenResult *out) {
    free(out->data);
    out->data = NULL;
    out->len = 0;
}
