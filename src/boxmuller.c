#include <float.h>
#include <math.h>

#include "boxmuller.h"
#include "mathutil.h"
#include "rng.h"


static void bm_update(struct bm_state *state) {
    const float mag = state->sigma * sqrtf(2.0 * logf(state->a));

    const float z1 = mag * cosf(two_pi * state->b) + state->mu;
    const float z2 = mag * sinf(two_pi * state->b) + state->mu;

    state->a = state->b;
    state->b = z1;
}

void bm_state_init(struct bm_state *state, float mu, float sigma) {
    do {
        state->a = rng_random_float(1.0);
    } while (state->a <= FLT_EPSILON);

    state->b = rng_random_float(1.0);

    state->mu = mu;
    state->sigma = sigma;

    bm_update(state);
}

float bm_next(struct bm_state *state) {
    bm_update(state);

    return state->b;
}
