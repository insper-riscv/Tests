// RV32_TEST_KIND: unit
#include <array>
#include <cstdint>
#include <vector>

extern "C" {
#include "rv32_platform.h"
}
extern "C" {
#include "rv32_test.h"
}

// C++ sobre a BOOT e o crt0: construtores de objetos globais e estáticos, classe com função virtual,
// new e delete, std::array e std::vector, e um throw capturado por catch com a destruição dos objetos
// locais. Cada passo falha com o seu código.

namespace {

// Quantas construções e destruições de Tracker aconteceram.
int constructed = 0;
int destroyed = 0;

// Objeto que registra a própria construção e destruição, para conferir a ordem e a limpeza da pilha.
struct Tracker {
    Tracker() { ++constructed; }
    ~Tracker() { ++destroyed; }
};

// Variável global com construtor: o crt0 tem de chamar o inicializador antes da main.
const int global_value = [] { return 41; }() + 1;
Tracker global_tracker;

// A classe com função virtual, para a tabela virtual e a chamada dinâmica.
struct Shape {
    virtual ~Shape() = default;
    virtual int sides() const = 0;
};
struct Triangle final : Shape {
    int sides() const override { return 3; }
};
struct Square final : Shape {
    int sides() const override { return 4; }
};

// Lança uma exceção com dois objetos locais vivos; o desenrolamento tem de destruí-los.
[[noreturn]] void thrower() {
    Tracker first;
    Tracker second;
    throw 7;
}

// Objeto estático local, construído na primeira chamada e uma só vez.
int static_local() {
    static const int value = [] { return 5; }();
    return value;
}

}  // namespace

int main() {
    if (global_value != 42) RV32_FAIL(1);
    if (constructed != 1) RV32_FAIL(2);

    Shape *shapes[2] = {new Triangle, new Square};
    const int total = shapes[0]->sides() + shapes[1]->sides();
    delete shapes[0];
    delete shapes[1];
    if (total != 7) RV32_FAIL(3);

    const std::array<int, 4> numbers{1, 2, 3, 4};
    int sum = 0;
    for (const int n : numbers) sum += n;
    if (sum != 10) RV32_FAIL(4);

    std::vector<int> values;
    for (int i = 0; i < 16; ++i) values.push_back(i * i);
    if (values.size() != 16 || values[15] != 225) RV32_FAIL(5);

    if (static_local() != 5 || static_local() != 5) RV32_FAIL(6);

    const int before = destroyed;
    int caught = 0;
    try {
        thrower();
    } catch (int code) {
        caught = code;
    }
    if (caught != 7) RV32_FAIL(7);
    if (destroyed - before != 2) RV32_FAIL(8);

    RV32_PASS();
}
