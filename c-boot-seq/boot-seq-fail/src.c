// RV32_TEST_KIND: unit
#include "rv32_test.h"

// Termina reprovado, com o código 5 em tohost. A etapa boot-seq espera FAIL deste programa, e o programa
// seguinte da sequência tem de rodar do mesmo jeito.
int main(void) {
    RV32_FAIL(5);
}
