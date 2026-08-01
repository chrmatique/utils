#ifndef WORDS_H
#define WORDS_H

#include <stddef.h>

static const char *const WORDS[] = {
    "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing",
    "elit", "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore",
    "et", "dolore", "magna", "aliqua", "enim", "ad", "minim", "veniam",
    "quis", "nostrud", "exercitation", "ullamco", "laboris", "nisi", "aliquip",
    "ex", "ea", "commodo", "consequat", "duis", "aute", "irure", "in",
    "reprehenderit", "voluptate", "velit", "esse", "cillum", "fugiat", "nulla",
    "pariatur", "excepteur", "sint", "occaecat", "cupidatat", "non", "proident",
    "sunt", "culpa", "qui", "officia", "deserunt", "mollit", "anim", "id",
    "est", "laborum", "at", "vero", "eos", "accusamus", "iusto", "odio",
    "dignissimos", "ducimus", "blanditiis", "praesentium", "voluptatum", "deleniti",
    "atque", "corrupti", "quos", "dolores", "quas", "molestias", "excepturi",
    "obcaecati", "cupiditate", "provident", "similique", "architecto", "beatae",
    "vitae", "dicta", "explicabo", "nemo", "ipsam", "voluptatem", "quia",
    "voluptas", "aspernatur", "aut", "odit", "fugit", "numquam", "eius",
    "modi", "tempora", "magnam", "quaerat", "voluptatem", "sequi", "nesciunt",
    "neque", "porro", "quisquam", "dolorem", "adipisci", "numquam", "eius",
    "modi", "tempora", "incidunt", "magnam", "aliquam", "quaerat", "voluptatem",
};

static const size_t WORD_LENS[] = {
    5, 5, 5, 3, 4, 11, 10, 4, 3, 2, 7, 6, 10, 2, 6, 2, 6, 5, 6, 4, 2, 5,
    6, 4, 7, 12, 7, 7, 4, 7, 2, 2, 7, 9, 4, 4, 5, 2, 13, 9, 5, 4, 6, 6, 5,
    8, 9, 4, 8, 9, 3, 8, 4, 5, 3, 7, 8, 6, 4, 2, 3, 7, 2, 4, 3, 9, 5, 4,
    11, 7, 10, 11, 10, 8, 5, 8, 4, 7, 4, 9, 9, 9, 10, 9, 9, 10, 6, 5, 5, 9,
    4, 5, 10, 4, 8, 10, 3, 4, 5, 7, 4, 4, 7, 6, 7, 10, 5, 8, 5, 5, 8, 7, 8,
    7, 4, 4, 7, 8, 6, 7, 7, 10,
};

#define WORD_COUNT (sizeof(WORDS) / sizeof(WORDS[0]))

#endif
