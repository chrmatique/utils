#include "generate.h"

#include <ctype.h>
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
    case MODE_SIZE:
        return count + 1;
    case MODE_MARKDOWN:
        return count * 600 + 256;
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

/* -------------------------------------------------------------------------- */
/* Size-based generation                                                      */
/* -------------------------------------------------------------------------- */

static int generate_size(Buf *buf, uint64_t *rng, size_t target) {
    if (target == 0) {
        return 0;
    }

    while (buf->len + 2 < target) {
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        size_t add = WORD_LENS[idx] + (buf->len == 0 ? 0 : 1);
        if (buf->len + add + 1 > target) {
            break;
        }
        if (buf->len > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        if (append_word(buf, idx, 0) != 0) {
            return -1;
        }
    }

    /* Trim a trailing space so the byte budget can be used for the newline. */
    if (buf->len > 0 && buf->data[buf->len - 1] == ' ') {
        buf->len--;
    }

    if (buf->len > 0 && buf->len < target) {
        return buf_append_byte(buf, '\n');
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Markdown generation                                                        */
/* -------------------------------------------------------------------------- */

static int append_inline_format(Buf *buf, uint64_t *rng) {
    uint64_t roll = rng_bounded(rng, 100);
    const char *wrap = "";
    if (roll < 20) {
        wrap = "**";
    } else if (roll < 40) {
        wrap = "_";
    } else if (roll < 55) {
        wrap = "`";
    }

    if (*wrap && buf_append_cstr(buf, wrap) != 0) {
        return -1;
    }

    size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
    if (append_word(buf, idx, 0) != 0) {
        return -1;
    }

    if (*wrap && buf_append_cstr(buf, wrap) != 0) {
        return -1;
    }
    return 0;
}

static int append_md_paragraph(Buf *buf, uint64_t *rng) {
    size_t words = (size_t)(SENTENCE_WORDS_MIN +
        rng_bounded(rng, 20 - SENTENCE_WORDS_MIN + 1));
    for (size_t i = 0; i < words; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        if (rng_bounded(rng, 100) < 30) {
            if (append_inline_format(buf, rng) != 0) {
                return -1;
            }
        } else {
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (append_word(buf, idx, 0) != 0) {
                return -1;
            }
        }
    }
    return buf_append_byte(buf, '\n');
}

static int append_md_heading(Buf *buf, uint64_t *rng) {
    size_t level = 1 + (size_t)rng_bounded(rng, 6); /* # to ###### */
    for (size_t i = 0; i < level; i++) {
        if (buf_append_byte(buf, '#') != 0) {
            return -1;
        }
    }
    if (buf_append_byte(buf, ' ') != 0) {
        return -1;
    }
    size_t words = 2 + (size_t)rng_bounded(rng, 5);
    for (size_t i = 0; i < words; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        if (append_word(buf, idx, i == 0) != 0) {
            return -1;
        }
    }
    return buf_append_byte(buf, '\n');
}

static int append_md_unordered_list(Buf *buf, uint64_t *rng) {
    size_t items = 2 + (size_t)rng_bounded(rng, 4);
    for (size_t i = 0; i < items; i++) {
        if (buf_append_cstr(buf, "- ") != 0) {
            return -1;
        }
        size_t words = 2 + (size_t)rng_bounded(rng, 6);
        for (size_t j = 0; j < words; j++) {
            if (j > 0 && buf_append_byte(buf, ' ') != 0) {
                return -1;
            }
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (append_word(buf, idx, 0) != 0) {
                return -1;
            }
        }
        if (buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
    }
    return 0;
}

static int append_md_ordered_list(Buf *buf, uint64_t *rng) {
    size_t items = 2 + (size_t)rng_bounded(rng, 4);
    for (size_t i = 0; i < items; i++) {
        if (buf_append(buf, "1. ", 3) != 0) {
            return -1;
        }
        size_t words = 2 + (size_t)rng_bounded(rng, 6);
        for (size_t j = 0; j < words; j++) {
            if (j > 0 && buf_append_byte(buf, ' ') != 0) {
                return -1;
            }
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (append_word(buf, idx, 0) != 0) {
                return -1;
            }
        }
        if (buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
    }
    return 0;
}

static int append_md_code_block(Buf *buf, uint64_t *rng) {
    const char *langs[] = {"c", "python", "js", "sh", "text"};
    size_t lang_idx = (size_t)rng_bounded(rng, sizeof(langs) / sizeof(langs[0]));
    if (buf_append_cstr(buf, "```") != 0 ||
        buf_append_cstr(buf, langs[lang_idx]) != 0 ||
        buf_append_byte(buf, '\n') != 0) {
        return -1;
    }

    size_t lines = 2 + (size_t)rng_bounded(rng, 5);
    for (size_t i = 0; i < lines; i++) {
        if (buf_append_cstr(buf, "    ") != 0) {
            return -1;
        }
        size_t words = 2 + (size_t)rng_bounded(rng, 5);
        for (size_t j = 0; j < words; j++) {
            if (j > 0 && buf_append_byte(buf, ' ') != 0) {
                return -1;
            }
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (append_word(buf, idx, 0) != 0) {
                return -1;
            }
        }
        if (buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
    }

    return buf_append_cstr(buf, "```\n");
}

static int append_md_table(Buf *buf, uint64_t *rng) {
    size_t cols = 2 + (size_t)rng_bounded(rng, 3);
    size_t rows = 2 + (size_t)rng_bounded(rng, 4);

    for (size_t c = 0; c < cols; c++) {
        if (c == 0) {
            if (buf_append_byte(buf, '|') != 0) {
                return -1;
            }
        }
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        if (buf_append_byte(buf, ' ') != 0 ||
            append_word(buf, idx, 0) != 0 ||
            buf_append_cstr(buf, " |") != 0) {
            return -1;
        }
    }
    if (buf_append_byte(buf, '\n') != 0) {
        return -1;
    }

    for (size_t c = 0; c < cols; c++) {
        if (c == 0) {
            if (buf_append_byte(buf, '|') != 0) {
                return -1;
            }
        }
        if (buf_append_cstr(buf, " --- |") != 0) {
            return -1;
        }
    }
    if (buf_append_byte(buf, '\n') != 0) {
        return -1;
    }

    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            if (c == 0) {
                if (buf_append_byte(buf, '|') != 0) {
                    return -1;
                }
            }
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (buf_append_byte(buf, ' ') != 0 ||
                append_word(buf, idx, 0) != 0 ||
                buf_append_cstr(buf, " |") != 0) {
                return -1;
            }
        }
        if (buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
    }
    return 0;
}

static int append_md_blockquote(Buf *buf, uint64_t *rng) {
    size_t lines = 1 + (size_t)rng_bounded(rng, 3);
    for (size_t i = 0; i < lines; i++) {
        if (buf_append_cstr(buf, "> ") != 0) {
            return -1;
        }
        size_t words = 2 + (size_t)rng_bounded(rng, 6);
        for (size_t j = 0; j < words; j++) {
            if (j > 0 && buf_append_byte(buf, ' ') != 0) {
                return -1;
            }
            size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
            if (append_word(buf, idx, 0) != 0) {
                return -1;
            }
        }
        if (buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
    }
    return 0;
}

static int append_md_section(Buf *buf, uint64_t *rng) {
    uint64_t roll = rng_bounded(rng, 100);
    if (roll < 25) {
        return append_md_heading(buf, rng);
    } else if (roll < 50) {
        return append_md_unordered_list(buf, rng);
    } else if (roll < 65) {
        return append_md_ordered_list(buf, rng);
    } else if (roll < 80) {
        return append_md_code_block(buf, rng);
    } else if (roll < 90) {
        return append_md_table(buf, rng);
    } else if (roll < 96) {
        return append_md_blockquote(buf, rng);
    } else {
        return append_md_paragraph(buf, rng);
    }
}

static int generate_markdown(Buf *buf, uint64_t *rng, size_t count) {
    /* Always start with a title. */
    if (buf_append_byte(buf, '#') != 0 ||
        buf_append_byte(buf, ' ') != 0) {
        return -1;
    }
    size_t title_words = 2 + (size_t)rng_bounded(rng, 5);
    for (size_t i = 0; i < title_words; i++) {
        if (i > 0 && buf_append_byte(buf, ' ') != 0) {
            return -1;
        }
        size_t idx = (size_t)rng_bounded(rng, WORD_COUNT);
        if (append_word(buf, idx, i == 0) != 0) {
            return -1;
        }
    }
    if (buf_append_cstr(buf, "\n\n") != 0) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (i > 0 && buf_append_byte(buf, '\n') != 0) {
            return -1;
        }
        if (append_md_section(buf, rng) != 0) {
            return -1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

int generate(const GenOptions *opts, GenResult *out) {
    Buf buf = {0};
    buf.cap = estimate_capacity(opts->mode,
                                opts->mode == MODE_SIZE ? opts->target_size : opts->count);
    if (buf.cap < 256) {
        buf.cap = 256;
    }
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
    case MODE_SIZE:
        rc = generate_size(&buf, &rng, opts->target_size);
        break;
    case MODE_MARKDOWN:
        rc = generate_markdown(&buf, &rng, opts->count);
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

int gen_compress(GenResult *out) {
    if (!out || !out->data || out->len == 0) {
        return 0;
    }

    size_t j = 0;
    int in_space = 1; /* Treat leading whitespace as already collapsed. */
    for (size_t i = 0; i < out->len; i++) {
        char c = out->data[i];
        int is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (is_space) {
            if (!in_space) {
                out->data[j++] = ' ';
                in_space = 1;
            }
        } else {
            out->data[j++] = c;
            in_space = 0;
        }
    }

    /* Trim trailing space and ensure a single trailing newline. */
    if (j > 0 && out->data[j - 1] == ' ') {
        j--;
    }
    out->data[j++] = '\n';

    char *shrunk = realloc(out->data, j);
    if (shrunk) {
        out->data = shrunk;
    }
    out->len = j;
    return 0;
}
