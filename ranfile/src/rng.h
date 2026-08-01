#ifndef RNG_H
#define RNG_H

#include <stdint.h>

static inline uint64_t rng_next(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}

static inline uint64_t rng_bounded(uint64_t *state, uint64_t n) {
    return rng_next(state) % n;
}

#endif
