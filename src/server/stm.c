/**
 * stm.c - pequeño motor de maquina de estados donde los eventos son los
 *         del selector.c
 */
#include <stdlib.h>
#include "../include/stm.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

void
stm_init(struct state_machine *stm) {
    // verificamos que los estados son correlativos, y que están bien asignados.
    for(unsigned i = 0 ; i <= stm->max_state; i++) {
        if(i != stm->states[i].state) {
            abort();
        }
    }

    if(stm->initial < stm->max_state) {
        stm->current = NULL;
    } else {
        abort();
    }
}

// inicializa el estado de una conexion particular si no lo estaba ya
inline static void
handle_first(struct state_machine *stm, struct selector_key *key) {
    if(stm->current == NULL) {
        stm->current = stm->states + stm->initial;
        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}

// avanza al siguiente state (HELLO_WRITE, etc) y ejecuta los hooks correspondientes (on_departure(), etc)
inline static
void jump(struct state_machine *stm, unsigned next, struct selector_key *key) {
    if(next > stm->max_state) {
        abort();
    }
    // checkeamos si se esta produciendo un cambio de estado realmente, porque tambien podriamos quedarnos en el mismo y en ese caso no deberiamos ejecutar los hooks
    if(stm->current != stm->states + next) {
        // llama al on_departure() del estado saliente
        if(stm->current != NULL && stm->current->on_departure != NULL) {
            stm->current->on_departure(stm->current->state, key);
        }

        // cambio de estado
        stm->current = stm->states + next;

        // llama al on_arrival() del estado entrante
        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}

unsigned
stm_handler_read(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_read_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_read_ready(key);
    jump(stm, ret, key);

    return ret;
}

unsigned
stm_handler_write(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_write_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_write_ready(key);
    jump(stm, ret, key);

    return ret;
}

unsigned
stm_handler_block(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_block_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_block_ready(key);
    jump(stm, ret, key);

    return ret;
}

void
stm_handler_close(struct state_machine *stm, struct selector_key *key) {
    if(stm->current != NULL && stm->current->on_departure != NULL) {
        stm->current->on_departure(stm->current->state, key);
    }
}

unsigned
stm_state(struct state_machine *stm) {
    unsigned ret = stm->initial;
    if(stm->current != NULL) {
        ret= stm->current->state;
    }
    return ret;
}
