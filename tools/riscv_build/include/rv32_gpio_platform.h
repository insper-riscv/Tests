/* Configuração do driver GPIO para a plataforma internal-mem. */
#ifndef RV32_GPIO_PLATFORM_H
#define RV32_GPIO_PLATFORM_H

#include <stdint.h>

#include "rv32_platform.h"

/* O único GPIO desta plataforma é o grupo de quatro bits do motor. */
#define RV32_GPIO_DEFAULT_BASE ((uintptr_t)RV32_MOTOR_BASE)
#define RV32_GPIO_DEFAULT_PIN_COUNT 4U

/* A plataforma não instancia os GPIO dedicados de chaves e botões. */
#define RV32_GPIO_SWITCHES_BASE 0
#define RV32_GPIO_SWITCHES_PIN_COUNT 0
#define RV32_GPIO_KEYS_BASE 0
#define RV32_GPIO_KEYS_PIN_COUNT 0

#define RV32_GPIO_GROUP_COUNT 1U
#define RV32_GPIO_GROUP_BASES {RV32_GPIO_DEFAULT_BASE}
#define RV32_GPIO_GROUP_WIDTHS {RV32_GPIO_DEFAULT_PIN_COUNT}

#endif
