#include <stdlib.h>
#include <string.h>

#include "e_classifier.h"


static void et_classifier_init(struct e_environment *env, void *params) {
    env->env_state = malloc(sizeof(struct ec_state));
    struct ec_state *state = (struct ec_state *)env->env_state;

    memcpy(&state->params, params, sizeof(struct ec_params));
    state->which = 0;
}

static void et_classifier_step(struct e_environment *env) {
    struct ec_state *state = (struct ec_state *)env->env_state;

    memcpy(env->inputs, state->params.inputs + (state->params.input_size * state->which), sizeof(float) * env->net->in_size);
    e_env_call_network(env);

    if (state->which == 0) {
        env->fitness = 0.0;
    }

    float fitness_sum = 0.0;
    float *expected_outs = state->params.desired_outs + (state->params.output_size * state->which);

    for (int i = 0; i < env->net->out_size; i++) {
        const float err = (env->outputs[i] - expected_outs[i]);
        fitness_sum -= err * err;
    }

    env->fitness += fitness_sum;    

    state->which = (state->which + 1) % state->params.num_examples;
}

struct e_environment_type et_classifier = {
    .init = et_classifier_init,
    .step = et_classifier_step
};
