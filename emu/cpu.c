#ifndef AGAIN
#include "misc.h"
#include "emu/cpu.h"
#include "emu/modrm.h"
#include "emu/interrupt.h"

static void trace_cpu(struct cpu_state *cpu) {
    TRACE("eax=%x ebx=%x ecx=%x edx=%x esi=%x edi=%x ebp=%x esp=%x",
            cpu->eax, cpu->ebx, cpu->ecx, cpu->edx, cpu->esi, cpu->edi, cpu->ebp, cpu->esp);
}

// fuck preprocessor
#define OP_SIZE 32
#define cpu_step CONCAT(cpu_step, OP_SIZE)
#endif

#undef oprnd_t
#if OP_SIZE == 32
#define oprnd_t dword_t
#else
#define oprnd_t word_t
#endif

// this will be the next PyEval_EvalFrameEx.
int cpu_step(struct cpu_state *cpu) {
    // watch out: these macros can evaluate the arguments any number of times
#define MEM(addr) MEM_GET(cpu, addr, OP_SIZE)
#define MEM8(addr) MEM_GET(cpu, addr, 8)
#define REG(reg_id) REG_VAL(cpu, reg_id, OP_SIZE)
#define REGPTR(regptr) REG(CONCAT3(regptr.reg,OP_SIZE,_id))

    // used by MODRM_MEM, don't use for anything else
    struct modrm_info modrm;
    dword_t modrm_addr;
#define DECODE_MODRM(size) \
    modrm_decode##size(cpu, &modrm_addr, &modrm)
#define MODRM_VAL \
    *(modrm.type == mod_reg ? &REGPTR(modrm.modrm_reg) : &MEM(modrm_addr))

#define PUSH(thing) \
    cpu->esp -= OP_SIZE/8; \
    MEM(cpu->esp) = thing

#undef imm
#define imm CONCAT(imm, OP_SIZE)
    byte_t imm8;
    oprnd_t imm;
#define READIMM \
    imm = MEM(cpu->eip); \
    cpu->eip += OP_SIZE/8; \
    TRACE("immediate: %x", imm)
#define READIMM8 \
    imm8 = MEM8(cpu->eip); \
    cpu->eip++; \
    TRACE("immediate: %x", imm8)

    // TODO use different registers in 16-bit mode

    byte_t insn = MEM8(cpu->eip);
    printf("0x%x: ", insn);
    cpu->eip++;
    switch (insn) {
        // if any instruction handlers declare variables, they should create a
        // new block for those variables

        // push dword register
        case 0x50:
            TRACE("push eax");
            PUSH(cpu->eax); break;
        case 0x51:
            TRACE("push ecx");
            PUSH(cpu->ecx); break;
        case 0x52:
            TRACE("push edx");
            PUSH(cpu->edx); break;
        case 0x53:
            TRACE("push ebx");
            PUSH(cpu->ebx); break;
        case 0x54: {
            TRACE("push esp");
            // need to make sure to push the old value
            dword_t old_esp = cpu->esp;
            PUSH(old_esp);
            break;
        }
        case 0x55:
            TRACE("push ebp");
            PUSH(cpu->ebp); break;
        case 0x56:
            TRACE("push esi");
            PUSH(cpu->esi); break;
        case 0x57:
            TRACE("push edi");
            PUSH(cpu->edi); break;

        // operand size prefix
        case 0x66:
#if OP_SIZE == 32
            TRACE("entering 16 bit mode");
            return cpu_step16(cpu);
#else
            TRACE("entering 32 bit mode");
            return cpu_step32(cpu);
#endif

        // subtract dword immediate byte from modrm
        case 0x83:
            TRACE("sub imm, modrm");
            DECODE_MODRM(32);
            READIMM8;
            // must cast to a signed value so sign extension occurs
            MODRM_VAL -= (int8_t) imm8;
            break;

        // move dword register to dword modrm
        case 0x89:
            TRACE("mov reg, modrm");
            DECODE_MODRM(32);
            MODRM_VAL = REGPTR(modrm.reg);
            break;

        // lea dword modrm to register
        case 0x8d:
            TRACE("lea modrm, reg");
            DECODE_MODRM(32);
            if (modrm.type == mod_reg) {
                return INT_UNDEFINED;
            }
            REGPTR(modrm.reg) = modrm_addr;
            break;

        // move dword immediate to register
        case 0xb8:
            TRACE("mov immediate, eax");
            READIMM; cpu->eax = imm; break;
        case 0xb9:
            TRACE("mov immediate, ecx");
            READIMM; cpu->ecx = imm; break;
        case 0xba:
            TRACE("mov immediate, edx");
            READIMM; cpu->edx = imm; break;
        case 0xbb:
            TRACE("mov immediate, ebx");
            READIMM; cpu->ebx = imm; break;
        case 0xbc:
            TRACE("mov immediate, esp");
            READIMM; cpu->esp = imm; break;
        case 0xbd:
            TRACE("mov immediate, ebx");
            READIMM; cpu->ebx = imm; break;
        case 0xbe:
            TRACE("mov immediate, ebx");
            READIMM; cpu->ebx = imm; break;
        case 0xbf:
            TRACE("mov immediate, ebx");
            READIMM; cpu->ebx = imm; break;

        case 0xcd:
            TRACE("interrupt");
            READIMM8; return imm8;

        // move byte immediate to modrm
        case 0xc6:
            TRACE("mov imm8, modrm8");
            DECODE_MODRM(32);
            READIMM8; MODRM_VAL = imm8;
            break;
        // move dword immediate to modrm
        case 0xc7:
            TRACE("mov imm, modrm");
            DECODE_MODRM(32);
            READIMM; MODRM_VAL = imm;
            break;

        default:
            TRACE("undefined");
            debugger;
            return INT_UNDEFINED;
    }
    trace_cpu(cpu);
    return -1; // everything is ok.
}

#ifndef AGAIN
#define AGAIN

#undef OP_SIZE
#define OP_SIZE 16
#include "cpu.c"

void cpu_run(struct cpu_state *cpu) {
    while (true) {
        int interrupt = cpu_step32(cpu);
        if (interrupt != INT_NONE) {
            TRACE("interrupt %d", interrupt);
            /* handle_interrupt(interrupt); */
            return;
        }
    }
}

#endif