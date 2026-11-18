// RV32_TEST_KIND: unit
#include <stdint.h>

#include "rv32_test.h"

// Nunca escreve tohost: o hospedeiro o dá por travado pelo tempo limite, e o programa seguinte da sequência
// tem de rodar mesmo assim.
int main(void) {
    volatile uint32_t spin = 0u;
    for (;;) {
        spin = spin + 1u;
    }
}
