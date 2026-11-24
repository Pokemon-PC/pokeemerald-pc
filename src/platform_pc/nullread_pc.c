/*
 * nullread_pc.c - reads through NULL behave as on the GBA.
 *
 * On the GBA, address 0 is the BIOS: reading a field through a NULL pointer
 * (a freed screen's data read by a sprite or VBlank callback one frame too
 * late, ...) returns a value and the game goes on. Such bugs are spread over
 * the decompiled code. On PC the read faults. Rather than patching every
 * site, the crash handler calls pc_emulate_null_read(): if the faulting
 * instruction is a plain load or compare from an address in the GBA BIOS
 * range (0x0000-0x3FFF), it is emulated with the value 0 and execution goes
 * on after it. Every emulated site is reported once (with a backtrace), so
 * the bugs stay visible; anything else is still a crash.
 *
 * Supported x86-64 forms (memory operand only): MOV r,m (8A/8B), MOVZX
 * (0F B6/B7), MOVSX (0F BE/BF), MOVSXD (63), CMP m,imm (80/81/83 /7),
 * CMP m,r / r,m (38-3B), TEST m,imm (F6/F7 /0), TEST m,r (84/85).
 */
#include "pc_hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define GBA_BIOS_END 0x4000u

#define FL_CF 0x0001u
#define FL_PF 0x0004u
#define FL_AF 0x0010u
#define FL_ZF 0x0040u
#define FL_SF 0x0080u
#define FL_OF 0x0800u
#define FL_ARITH (FL_CF | FL_PF | FL_AF | FL_ZF | FL_SF | FL_OF)

static uint32_t parity_flag(uint64_t v) {
    uint8_t b = (uint8_t)v;
    b ^= b >> 4;
    b ^= b >> 2;
    b ^= b >> 1;
    return (b & 1) ? 0 : FL_PF;
}

/* Flags of (a - b) on `bits`-wide operands. */
static uint32_t sub_flags(uint64_t a, uint64_t b, int bits) {
    uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1);
    a &= mask;
    b &= mask;
    uint64_t r = (a - b) & mask;
    uint64_t sign = 1ull << (bits - 1);
    uint32_t f = 0;
    if (a < b) f |= FL_CF;
    if (r == 0) f |= FL_ZF;
    if (r & sign) f |= FL_SF;
    if (((a ^ b) & (a ^ r)) & sign) f |= FL_OF;
    if (((a ^ b ^ r) & 0x10) != 0) f |= FL_AF;
    return f | parity_flag(r);
}

static uint32_t logic_flags(uint64_t r, int bits) {
    uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1);
    r &= mask;
    uint32_t f = 0;
    if (r == 0) f |= FL_ZF;
    if (r & (1ull << (bits - 1))) f |= FL_SF;
    return f | parity_flag(r);
}

/* Writes `value` (already sign/zero extended) to register `reg` with x86-64
 * partial-register rules for the destination size. */
static void write_reg(uint64_t *regs[16], int reg, int bits, bool legacy_high8, uint64_t value) {
    if (bits == 8 && legacy_high8) {             /* AH, CH, DH, BH */
        uint64_t *r = regs[reg - 4];
        *r = (*r & ~0xFF00ull) | ((value & 0xFF) << 8);
    } else if (bits == 8) {
        *regs[reg] = (*regs[reg] & ~0xFFull) | (value & 0xFF);
    } else if (bits == 16) {
        *regs[reg] = (*regs[reg] & ~0xFFFFull) | (value & 0xFFFF);
    } else if (bits == 32) {
        *regs[reg] = value & 0xFFFFFFFFull;      /* zero-extends */
    } else {
        *regs[reg] = value;
    }
}

static uint64_t read_reg(uint64_t *regs[16], int reg, int bits, bool legacy_high8) {
    if (bits == 8 && legacy_high8) {
        return (*regs[reg - 4] >> 8) & 0xFF;
    }
    uint64_t v = *regs[reg];
    return bits == 64 ? v : v & ((1ull << bits) - 1);
}

bool pc_emulate_null_read(uint64_t *regs[16], uint64_t *rip, uint32_t *eflags, uintptr_t fault) {
    if (fault >= GBA_BIOS_END) {
        return false;
    }
    const uint8_t *p = (const uint8_t *)(uintptr_t)*rip;
    const uint8_t *start = p;
    bool opsize16 = false;
    uint8_t rex = 0;
    for (;;) {
        if (*p == 0x66) { opsize16 = true; p++; }
        else if (*p == 0x2E || *p == 0x3E || *p == 0x26 || *p == 0x36) { p++; }
        else break;
    }
    if ((*p & 0xF0) == 0x40) {
        rex = *p++;
    }
    bool rexw = rex & 8, rexr = rex & 4;

    int op = *p++;
    int op2 = -1;
    if (op == 0x0F) {
        op2 = *p++;
    }

    /* ModRM + SIB + displacement */
    uint8_t modrm = *p++;
    int mod = modrm >> 6, reg = ((modrm >> 3) & 7) | (rexr ? 8 : 0), rm = modrm & 7;
    if (mod == 3) {
        return false;                      /* register operand: cannot be the fault */
    }
    if (rm == 4) {
        uint8_t sib = *p++;
        if (mod == 0 && (sib & 7) == 5) p += 4;
    } else if (mod == 0 && rm == 5) {
        p += 4;                            /* RIP-relative */
    }
    if (mod == 1) p += 1;
    if (mod == 2) p += 4;

    int opbits = rexw ? 64 : opsize16 ? 16 : 32;
    bool high8 = rex == 0 && (reg & 7) >= 4 && reg < 8;
    uint32_t newflags = 0;
    bool setflags = false;

    if (op2 < 0) {
        switch (op) {
        case 0x8A:                                         /* MOV r8, m8 */
            write_reg(regs, reg, 8, high8, 0);
            break;
        case 0x8B:                                         /* MOV r, m */
            write_reg(regs, reg, opbits, false, 0);
            break;
        case 0x63:                                         /* MOVSXD r64, m32 */
            write_reg(regs, reg, rexw ? 64 : 32, false, 0);
            break;
        case 0x80: case 0x81: case 0x83: {                 /* group 1, imm */
            int sub = (modrm >> 3) & 7;
            int bits = op == 0x80 ? 8 : opbits;
            int64_t imm;
            if (op == 0x81) {
                if (bits == 16) { imm = (int16_t)(p[0] | (p[1] << 8)); p += 2; }
                else { int32_t v; memcpy(&v, p, 4); imm = v; p += 4; }
            } else {
                imm = (int8_t)*p++;
            }
            if (sub != 7) return false;                    /* only CMP reads without writing */
            newflags = sub_flags(0, (uint64_t)imm, bits);
            setflags = true;
            break;
        }
        case 0x38: case 0x39: {                            /* CMP m, r */
            int bits = op == 0x38 ? 8 : opbits;
            newflags = sub_flags(0, read_reg(regs, reg, bits, op == 0x38 && high8), bits);
            setflags = true;
            break;
        }
        case 0x3A: case 0x3B: {                            /* CMP r, m */
            int bits = op == 0x3A ? 8 : opbits;
            newflags = sub_flags(read_reg(regs, reg, bits, op == 0x3A && high8), 0, bits);
            setflags = true;
            break;
        }
        case 0x84: case 0x85:                              /* TEST m, r */
            newflags = logic_flags(0, op == 0x84 ? 8 : opbits);
            setflags = true;
            break;
        case 0xF6: case 0xF7: {                            /* TEST m, imm */
            if (((modrm >> 3) & 7) != 0) return false;
            int bits = op == 0xF6 ? 8 : opbits;
            p += op == 0xF6 ? 1 : (bits == 16 ? 2 : 4);
            newflags = logic_flags(0, bits);
            setflags = true;
            break;
        }
        default:
            return false;
        }
    } else {
        switch (op2) {
        case 0xB6: case 0xB7: case 0xBE: case 0xBF:        /* MOVZX / MOVSX */
            write_reg(regs, reg, opbits, false, 0);
            break;
        default:
            return false;
        }
    }
    if (setflags) {
        *eflags = (*eflags & ~FL_ARITH) | newflags;
    }
    *rip += (uint64_t)(p - start);
    return true;
}
