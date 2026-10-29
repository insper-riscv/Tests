// RV32_EXT: M,Zifencei
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdio.h>

// Command monitor on the console of the JTAG UART.
//
// The program reads a line from the UART, echoes what it reads, runs the command and answers on the
// UART. The numbers are hexadecimal, the words are 32 bits and word-aligned.
//
//   help                    lists the commands
//   peek ADDR               shows the word at ADDR
//   poke ADDR VALUE         writes VALUE to the word at ADDR
//   dump ADDR N             shows N words from ADDR
//   run ADDR                calls the code at ADDR (a function that returns) and shows what it returns
//   status                  shows the index and the state of the last ELF, from the display register
//   reset                   restarts the core from the boot ROM
//   quit                    ends the program with a PASS (what a test run needs)
//
// An unknown or malformed command prints "error: ..." and the monitor goes on; nothing a host sends
// ends the loop but `quit`. On the board, run the program, then talk to it with
//
//   riscv-tools --config tools/riscv_build/config.sdram.yaml console --interactive
//
// In simulation the testbench sends uart_input.txt through the UART and compares everything the
// program printed with uart_expected.txt. With nobody on the UART the program ends at once.

#define UART_RXDATA ((volatile unsigned int *)0xC0000004u)
#define UART_STATUS ((volatile unsigned int *)0xC0000008u)
#define DISPLAY ((volatile unsigned int *)0xD0000000u)
#define HOST_ATTACHED(status) (((status) >> 17) & 1u)
#define RX_VALID(word) (((word) >> 8) & 1u)
#define ATTACH_POLLS 40000000u
#define LINE_MAX 64
#define ARGS_MAX 3
#define DUMP_MAX 64
#define DUMP_PER_LINE 4

static void print_error(const char *why) {
    printf("error: %s\n", why);
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex(const char *text, unsigned int *value) {
    unsigned int result = 0;
    int digits = 0;
    for (; *text; text++) {
        int d = hex_digit(*text);
        if (d < 0 || digits == 8) return 0;
        result = (result << 4) | (unsigned int)d;
        digits++;
    }
    *value = result;
    return digits > 0;
}

static int same(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

// Splits the line at the blanks, in place; returns how many words it found
static int split(char *line, char *words[ARGS_MAX + 1]) {
    int count = 0;
    while (*line) {
        while (*line == ' ') *line++ = '\0';
        if (!*line) break;
        if (count == ARGS_MAX + 1) return count + 1;
        words[count++] = line;
        while (*line && *line != ' ') line++;
    }
    return count;
}

static int address_argument(const char *text, unsigned int *address) {
    if (!parse_hex(text, address)) {
        print_error("bad number");
        return 0;
    }
    if (*address & 3u) {
        print_error("address not word-aligned");
        return 0;
    }
    return 1;
}

static void command_dump(unsigned int address, unsigned int count) {
    if (count > DUMP_MAX) {
        print_error("at most 40 words");
        return;
    }
    for (unsigned int i = 0; i < count; i++) {
        if (i % DUMP_PER_LINE == 0) printf("%08x:", address + 4 * i);
        printf(" %08x", *(volatile unsigned int *)(address + 4 * i));
        if (i % DUMP_PER_LINE == DUMP_PER_LINE - 1 || i == count - 1) printf("\n");
    }
}

static int command_run(unsigned int address) {
    __asm__ volatile("fence.i" ::: "memory");
    return ((int (*)(void))address)();
}

static void command_help(void) {
    puts("help | peek A | poke A V | dump A N | run A | status | reset | quit");
}

// Runs one command line; returns 1 for quit
static int execute(char *line) {
    char *w[ARGS_MAX + 1];
    unsigned int a = 0, v = 0;
    int n = split(line, w);
    if (n == 0) return 0;
    if (n > ARGS_MAX) {
        print_error("too many words");
    } else if (same(w[0], "help") && n == 1) {
        command_help();
    } else if (same(w[0], "peek") && n == 2) {
        if (address_argument(w[1], &a)) printf("%08x: %08x\n", a, *(volatile unsigned int *)a);
    } else if (same(w[0], "poke") && n == 3) {
        if (address_argument(w[1], &a)) {
            if (parse_hex(w[2], &v)) *(volatile unsigned int *)a = v;
            else print_error("bad number");
        }
    } else if (same(w[0], "dump") && n == 3) {
        if (address_argument(w[1], &a)) {
            if (parse_hex(w[2], &v)) command_dump(a, v);
            else print_error("bad number");
        }
    } else if (same(w[0], "run") && n == 2) {
        if (address_argument(w[1], &a)) printf("returned %08x\n", (unsigned int)command_run(a));
    } else if (same(w[0], "status") && n == 1) {
        unsigned int d = *DISPLAY;
        printf("index %02x state %04x\n", (d >> 16) & 0xFFu, d & 0xFFFFu);
    } else if (same(w[0], "reset") && n == 1) {
        puts("reset");
        ((void (*)(void))0)();
    } else if (same(w[0], "quit") && n == 1) {
        return 1;
    } else {
        print_error("unknown command or wrong arguments");
    }
    return 0;
}

// The next byte from the UART, or -1 when there is none
static int read_byte(void) {
    unsigned int word = *UART_RXDATA;
    return RX_VALID(word) ? (int)(word & 0xFFu) : -1;
}

int main(void) {
    char line[LINE_MAX];
    unsigned int length = 0;
    int discarding = 0;

    for (unsigned int polls = 0; polls < ATTACH_POLLS && !HOST_ATTACHED(*UART_STATUS); polls++) {
    }
    if (!HOST_ATTACHED(*UART_STATUS)) RV32_PASS();

    puts("monitor ready");
    for (;;) {
        int c = read_byte();
        if (c < 0) continue;
        if (c == '\r' || c == '\n') {
            putchar('\n');
            if (discarding) {
                print_error("line too long");
            } else {
                line[length] = '\0';
                if (execute(line)) break;
            }
            length = 0;
            discarding = 0;
        } else if (c == 0x08 || c == 0x7F) {
            if (length > 0 && !discarding) length--;
        } else if (!discarding) {
            if (length == LINE_MAX - 1) {
                discarding = 1;
            } else {
                line[length++] = (char)c;
                putchar(c);
            }
        }
    }
    puts("bye");
    RV32_PASS();
}
