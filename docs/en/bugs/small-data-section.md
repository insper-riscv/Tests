# Small data (`.sdata`, `.sbss`) and the `gp` register

🌐 [Português](../../pt-br/bugs/small-data-section.md) · [English](small-data-section.md)

This document describes how the RISC-V GCC places small globals in dedicated sections, what `crt0.S` and `link.ld` must do for these globals to work, and how to diagnose a failure in this region.

## 1. Small globals and `gp`

By ABI default, the RISC-V GCC places globals of up to 8 bytes in `.sdata` (initialized) and `.sbss` (zeroed), and accesses them relative to the `gp` register (`lb rd, offset(gp)`) instead of by absolute address. The instructions get shorter, but the access is only correct if `gp` points near the middle of the small-data region, by the `__global_pointer$` convention.

Two requirements follow.

| Requirement | Where it is met |
| :--- | :--- |
| `gp` initialized before any access relative to it | `crt0.S` |
| `.sdata`, `.sbss`, and `.srodata` inside the range that `crt0.S` copies and zeroes | `link.ld` |

## 2. Initializing `gp`

The first instruction of `_start` loads `gp`, before any other `la`. The `.option norelax` prevents the assembler from replacing this computation with an access relative to `gp` itself, which does not hold a valid value yet:

```asm
.option push
.option norelax
1:  auipc gp, %pcrel_hi(__global_pointer$)
    addi  gp, gp, %pcrel_lo(1b)
.option pop
```

The `tp` register (x4) is initialized the same way; the complete state at the instant `main()` is called is described in the [Insper-RISC-V boot document](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/en/platform/boot.md).

## 3. Linker rules

`link.ld` matches `.sdata`, `.srodata`, and `.sbss` explicitly. A bare `*(.data*)` pattern does not match these sections, and the linker would place them after `_data_end` has already been computed, outside the copy range:

```ld
.data : {
    _data_start = .;
    *(.data*)
    . = ALIGN(4);
    PROVIDE(__global_pointer$ = . + 0x800);
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
    _data_end = .;
} > RAM AT> ROM

.sbss (NOLOAD) : {
    _bss_start = .;
    *(.dynsbss)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
} > RAM
```

`__global_pointer$` sits `0x800` past the start of `.sdata`. With signed 12-bit immediates, `gp` reaches `.data` backwards and `.sbss` forwards, both within range.

## 4. Effect of violating the rules

With `.sdata` outside the copy range, `_data_start` and `_data_end` become equal and the copy loop of `crt0.S` ends at the first iteration without copying anything:

```asm
1:
    bge t1, t2, 2f      // _data_start >= _data_end, exits immediately
    lw  t3, 0(t0)
    sw  t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j   1b
2:
```

The global keeps the undefined RAM contents, which start blank on real hardware. With `gp` uninitialized, every access relative to it goes to an arbitrary address. The core is Harvard, with no MMU and no exception handling, so an address outside the valid RAM and ROM can hang the pipeline. The visible result is a program that never writes its verdict, rather than a clean failure.

## 5. Diagnosis

The commands below, run on the generated `.elf`, confirm the situation.

| Command | What to look for |
| :--- | :--- |
| `riscv32-unknown-elf-nm -S build/real/<test>.elf \| grep -E "_data_\|<symbol>"` | `_data_start` equal to `_data_end` with real objects in `.data` |
| `riscv32-unknown-elf-readelf -S build/real/<test>.elf \| grep -E "\.data\|\.sdata"` | object in `.sdata` or `.sbss` outside the copied range |

The `data-init-byte` test (`c/data-init-byte/src.c`) isolates the case: a single `static volatile unsigned char` initialized with `0xA5u`, which GCC places in `.sdata`.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
