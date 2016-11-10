#include "stdafx.h"
#include "Emu/IdManager.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"

#include "PPUASMJITRecompiler.h"
#include "PPUDisAsm.h"
#include "PPUThread.h"
#include "PPUInterpreter.h"

#include <cassert>

#define ASMJIT_STATIC
#define ASMJIT_DEBUG

#include "asmjit.h"

#define PPU_OFF_128(x) asmjit::host::oword_ptr(*cpu, OFFSET_32(ppu_thread, x))
#define PPU_OFF_64(x) asmjit::host::qword_ptr(*cpu, OFFSET_32(ppu_thread, x))
#define PPU_OFF_32(x) asmjit::host::dword_ptr(*cpu, OFFSET_32(ppu_thread, x))
#define PPU_OFF_16(x) asmjit::host::word_ptr(*cpu, OFFSET_32(ppu_thread, x))
#define PPU_OFF_8(x) asmjit::host::byte_ptr(*cpu, OFFSET_32(ppu_thread, x))

#define PPU_PS3_OFF_128(x) asmjit::host::oword_ptr(*baseReg, *x)
#define PPU_PS3_OFF_64(x) asmjit::host::qword_ptr(*baseReg, *x)
#define PPU_PS3_OFF_32(x) asmjit::host::dword_ptr(*baseReg, *x)
#define PPU_PS3_OFF_16(x) asmjit::host::word_ptr(*baseReg, *x)
#define PPU_PS3_OFF_8(x) asmjit::host::byte_ptr(*baseReg, *x)

// this makes things too slow to function...grr
//#define PPUJIT_LOGGING_ON

//#define DISABLE_GPR_LOAD
//#define DISABLE_GPR_STORE
//#define DISABLE_GPR_ALU
//#define DISABLE_GPR_CMP
//#define DISABLE_GPR_BIT
//#define DISABLE_GPR_SHIFTS
//#define DISABLE_GPR_ROTATES

//#define DISABLE_FPR_LOAD
//#define DISABLE_FPR_STORE
//#define DISABLE_FPR_MOVESIGN
//#define DISABLE_FPR_ALU
//#define DISABLE_FPR_ROUNDING
//#define DISABLE_FPR_FMA
//#define DISABLE_FPR_CMP

//#define DISABLE_BRANCHES
//#define DISABLE_MF_SPECIAL
//#define DISABLE_CR_SPECIAL

//#define DISABLE_VPR_LOAD
//#define DISABLE_VPR_LOAD_SHIFT
//#define DISABLE_VPR_STORE
//#define DISABLE_VPR_INT_CMP
//#define DISABLE_VPR_INT_ADD
//#define DISABLE_VPR_INT_SUB
//#define DISABLE_VPR_INT_MUL
//#define DISABLE_VPR_INT_MUL_ADD
//#define DISABLE_VPR_INT_SUM_ACROSS
//#define DISABLE_VPR_INT_AVG
//#define DISABLE_VPR_INT_MAX_MIN
//#define DISABLE_VPR_BIT_LOGICAL
//#define DISABLE_VPR_BIT_ROTATE

//#define DISABLE_VPR_FP_ALU
//#define DISABLE_VPR_FP_CONVERT // converts might be wrong currently for the most part
//#define DISABLE_VPR_FP_ROUND
//#define DISABLE_VPR_FP_CMP
//#define DISABLE_VPR_FP_EST
//#define DISABLE_VPR_SPLAT
//#define DISABLE_VPR_PERM_SEL
//#define DISABLE_VPR_MERGE

//#define DISABLE_VPR_PACK
//#define DISABLE_VPR_UNPACK

#ifdef DISABLE_GPR_LOAD
#define GPR_LOAD InterpreterCall(op); return;
#else
#define GPR_LOAD
#endif

#ifdef DISABLE_GPR_STORE
#define GPR_STORE InterpreterCall(op); return;
#else
#define GPR_STORE
#endif

#ifdef DISABLE_VPR_LOAD
#define VPR_LOAD InterpreterCall(op); return;
#else
#define VPR_LOAD
#endif

#ifdef DISABLE_VPR_LOAD_SHIFT
#define VPR_LOAD_SHIFT InterpreterCall(op); return;
#else
#define VPR_LOAD_SHIFT
#endif

#ifdef DISABLE_VPR_STORE
#define VPR_STORE InterpreterCall(op); return;
#else
#define VPR_STORE
#endif

#ifdef DISABLE_VPR_UNPACK
#define VPR_UNPACK InterpreterCall(op); return;
#else
#define VPR_UNPACK
#endif

#ifdef DISABLE_VPR_PACK
#define VPR_PACK InterpreterCall(op); return;
#else
#define VPR_PACK
#endif

#ifdef DISABLE_VPR_MERGE
#define VPR_MERGE InterpreterCall(op); return;
#else
#define VPR_MERGE
#endif

#ifdef DISABLE_VPR_PERM_SEL
#define VPR_PERM_SEL InterpreterCall(op); return;
#else
#define VPR_PERM_SEL
#endif

#ifdef DISABLE_VPR_SPLAT
#define VPR_SPLAT InterpreterCall(op); return;
#else
#define VPR_SPLAT
#endif

#ifdef DISABLE_VPR_FP_EST
#define VPR_FP_EST InterpreterCall(op); return;
#else
#define VPR_FP_EST
#endif

#ifdef DISABLE_VPR_FP_CMP
#define VPR_FP_CMP InterpreterCall(op); return;
#else
#define VPR_FP_CMP
#endif

#ifdef DISABLE_VPR_FP_ROUND
#define VPR_FP_ROUND InterpreterCall(op); return;
#else
#define VPR_FP_ROUND
#endif

#ifdef DISABLE_VPR_FP_CONVERT
#define VPR_FP_CONVERT InterpreterCall(op); return;
#else
#define VPR_FP_CONVERT
#endif

#ifdef DISABLE_VPR_FP_ALU
#define VPR_FP_ALU InterpreterCall(op); return;
#else
#define VPR_FP_ALU
#endif

#ifdef DISABLE_VPR_BIT_ROTATE
#define VPR_BIT_ROTATE InterpreterCall(op); return;
#else
#define VPR_BIT_ROTATE
#endif

#ifdef DISABLE_VPR_BIT_LOGICAL
#define VPR_BIT_LOGICAL InterpreterCall(op); return;
#else
#define VPR_BIT_LOGICAL
#endif

#ifdef DISABLE_VPR_INT_CMP
#define VPR_INT_CMP InterpreterCall(op); return;
#else
#define VPR_INT_CMP
#endif

#ifdef DISABLE_VPR_INT_MAX_MIN
#define VPR_INT_MAX_MIN InterpreterCall(op); return;
#else
#define VPR_INT_MAX_MIN
#endif

#ifdef DISABLE_VPR_INT_AVG
#define VPR_INT_AVG InterpreterCall(op); return;
#else
#define VPR_INT_AVG
#endif

#ifdef DISABLE_VPR_INT_SUM_ACROSS
#define VPR_INT_SUM_ACROSS InterpreterCall(op); return;
#else
#define VPR_INT_SUM_ACROSS
#endif

#ifdef DISABLE_VPR_INT_MUL_ADD
#define VPR_INT_MUL_ADD InterpreterCall(op); return;
#else
#define VPR_INT_MUL_ADD
#endif

#ifdef DISABLE_VPR_INT_MUL
#define VPR_INT_MUL InterpreterCall(op); return;
#else
#define VPR_INT_MUL
#endif

#ifdef DISABLE_VPR_INT_ADD
#define VPR_INT_ADD InterpreterCall(op); return;
#else
#define VPR_INT_ADD
#endif

#ifdef DISABLE_VPR_INT_SUB
#define VPR_INT_SUB InterpreterCall(op); return;
#else
#define VPR_INT_SUB
#endif

#ifdef DISABLE_GPR_BIT
#define GPR_BIT InterpreterCall(op); return;
#else
#define GPR_BIT
#endif

#ifdef DISABLE_GPR_ROTATE
#define GPR_ROTATE InterpreterCall(op); return;
#else
#define GPR_ROTATE
#endif

#ifdef DISABLE_GPR_SHIFT
#define GPR_SHIFT InterpreterCall(op); return;
#else
#define GPR_SHIFT
#endif

#ifdef DISABLE_CR_SPECIAL
#define CR_SPECIAL InterpreterCall(op); return;
#else
#define CR_SPECIAL
#endif

#ifdef DISABLE_MF_SPECIAL
#define MF_SPECIAL InterpreterCall(op); return;
#else
#define MF_SPECIAL
#endif

#ifdef DISABLE_FPR_CMP
#define FPR_CMP InterpreterCall(op); return;
#else
#define FPR_CMP
#endif

#ifdef DISABLE_GPR_CMP
#define GPR_CMP InterpreterCall(op); return;
#else
#define GPR_CMP
#endif

#ifdef DISABLE_GPR_ALU
#define GPR_ALU InterpreterCall(op); return;
#else
#define GPR_ALU
#endif

#ifdef DISABLE_FPR_ALU
#define FPR_ALU InterpreterCall(op); return;
#else
#define FPR_ALU
#endif

#ifdef DISABLE_FPR_FMA
#define FPR_FMA InterpreterCall(op); return;
#else
#define FPR_FMA
#endif

#ifdef DISABLE_FPR_ROUNDING
#define FPR_ROUNDING InterpreterCall(op); return;
#else
#define FPR_ROUNDING
#endif

#ifdef DISABLE_BRANCHES
#define GPR_BRANCH InterpreterCall(op); return;
#else
#define GPR_BRANCH
#endif


#ifdef DISABLE_FPR_LOAD
#define FPR_LOAD InterpreterCall(op); return;
#else
#define FPR_LOAD
#endif

#ifdef DISABLE_FPR_STORE
#define FPR_STORE InterpreterCall(op); return;
#else
#define FPR_STORE
#endif


#ifdef DISABLE_FPR_MOVESIGN
#define FPR_MOVESIGN InterpreterCall(op); return;
#else
#define FPR_MOVESIGN InterpreterCall(op); return;
#endif

#ifdef DISABLE_FPR_ALU
#define FRP_ALU InterpreterCall(op); return;
#else
#define FPR_ALU
#endif


#ifdef PPUJIT_LOGGING_ON
#define PPUJIT_LOGGING(x) x
#else
#define PPUJIT_LOGGING(x)
#endif

const ppu_decoder<ppu_interpreter_fast> s_ppu_interpreter; // TODO: remove
const ppu_decoder<ppu_recompiler> s_ppu_decoder;

// todo: look into generating these on the fly instead of grabbing
// note: negates can be done with psign and a register of 1's
// abs is also an instruction in sse4 and shouldnt be used from here if necessary 
const u128 ppu_recompiler::xmmConstData[] = {
    // 32 bit xmm bswap mask
    u128(0x0c0d0e0f08090a0b, 0x0405060700010203),
    // 64 bit xmm bswap mask
    u128(0x08090a0b0c0d0e0f, 0x0001020304050607),
    // 128 bit xmm bswap mask
    u128(0x0001020304050607, 0x08090a0b0c0d0e0f),
    // Negate 64bit
    u128(0x8000000000000000, 0x8000000000000000),
    // Abs 64bit const
    u128(0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF),
    // Negate 32 bit const
    u128(0x8000000080000000, 0x8000000080000000),
    // mask low 5 bit mask
    u128(0x0000001F0000001F, 0x0000001F0000001F),
    // pack bytes to 16 bit vpperm mask
    u128(0x1e1c1a1816141210, 0x0e0c0a0806040200),
    // pack 16bit to 32bit vpperm mask
    u128(0x1d1c191815141110, 0x0d0c090805040100),
    // byte rotate mask, keep low 3 bits
    u128(0x0707070707070707, 0x0707070707070707),
    // (half)word rotate mask, keep low 4 bits
    u128(0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f),
    // (double)word rotate mask, keep low 5 bits
    u128(0x1f1f1f1f1f1f1f1f, 0x1f1f1f1f1f1f1f1f),
    // pshufb low bytes to high
    u128(0x0e0c0a0806040200, 0xffffffffffffffff),
    // pshufb low bytes to low
    u128(0xffffffffffffffff, 0x0e0c0a0806040200),
    // pshufb low 16 bit to high
    u128(0x0d0c090805040100, 0xffffffffffffffff),
    // pshufb low 16 bit to low
    u128(0xffffffffffffffff, 0x0d0c090805040100),
    // high bit set 16 bit
    u128(0x8000800080008000, 0x8000800080008000),
    // max int 32 bit float
    u128(0x4B8000004B800000, 0x4B8000004B800000)
};

const u128 ppu_recompiler::xmmFloatConstData[] = {
    // high exp bits
    u128(0x0000009E0000009E, 0x0000009E0000009E),
    // high exp bit mask
    u128(0x7F8000007F800000, 0x7F8000007F800000),
    // 32 in each word
    u128(0x0000002000000020, 0x0000002000000020),
    // invalid nan
    u128(0xFFC00000FFC00000, 0xFFC00000FFC00000),
};

const u128 ppu_recompiler::xmmLvslShift[] = {
    u128(0x0001020304050607, 0x08090A0B0C0D0E0F),
    u128(0x0102030405060708, 0x090A0B0C0D0E0F10),
    u128(0x0203040506070809, 0x0A0B0C0D0E0F1011),
    u128(0x030405060708090A, 0x0B0C0D0E0F101112),
    u128(0x0405060708090A0B, 0x0C0D0E0F10111213),
    u128(0x05060708090A0B0C, 0x0D0E0F1011121314),
    u128(0x060708090A0B0C0D, 0x0E0F101112131415),
    u128(0x0708090A0B0C0D0E, 0x0F10111213141516),
    u128(0x08090A0B0C0D0E0F, 0x1011121314151617),
    u128(0x090A0B0C0D0E0F10, 0x1112131415161718),
    u128(0x0A0B0C0D0E0F1011, 0x1213141516171819),
    u128(0x0B0C0D0E0F101112, 0x131415161718191A),
    u128(0x0C0D0E0F10111213, 0x1415161718191A1B),
    u128(0x0D0E0F1011121314, 0x15161718191A1B1C),
    u128(0x0E0F101112131415, 0x161718191A1B1C1D),
    u128(0x0F10111213141516, 0x1718191A1B1C1D1E),
};

const u128 ppu_recompiler::xmmLvsrShift[] = {
    u128(0x1011121314151617, 0x18191A1B1C1D1E1F),
    u128(0x0F10111213141516, 0x1718191A1B1C1D1E),
    u128(0x0E0F101112131415, 0x161718191A1B1C1D),
    u128(0x0D0E0F1011121314, 0x15161718191A1B1C),
    u128(0x0C0D0E0F10111213, 0x1415161718191A1B),
    u128(0x0B0C0D0E0F101112, 0x131415161718191A),
    u128(0x0A0B0C0D0E0F1011, 0x1213141516171819),
    u128(0x090A0B0C0D0E0F10, 0x1112131415161718),
    u128(0x08090A0B0C0D0E0F, 0x1011121314151617),
    u128(0x0708090A0B0C0D0E, 0x0F10111213141516),
    u128(0x060708090A0B0C0D, 0x0E0F101112131415),
    u128(0x05060708090A0B0C, 0x0D0E0F1011121314),
    u128(0x0405060708090A0B, 0x0C0D0E0F10111213),
    u128(0x030405060708090A, 0x0B0C0D0E0F101112),
    u128(0x0203040506070809, 0x0A0B0C0D0E0F1011),
    u128(0x0102030405060708, 0x090A0B0C0D0E0F10),
};

const u128 ppu_recompiler::xmmStvlxMask[] = {
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF00),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFF0000),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFF000000),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF00000000),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFFFF0000000000),
    u128(0xFFFFFFFFFFFFFFFF, 0xFFFF000000000000),
    u128(0xFFFFFFFFFFFFFFFF, 0xFF00000000000000),
    u128(0xFFFFFFFFFFFFFFFF, 0x0000000000000000),
    u128(0xFFFFFFFFFFFFFF00, 0x0000000000000000),
    u128(0xFFFFFFFFFFFF0000, 0x0000000000000000),
    u128(0xFFFFFFFFFF000000, 0x0000000000000000),
    u128(0xFFFFFFFF00000000, 0x0000000000000000),
    u128(0xFFFFFF0000000000, 0x0000000000000000),
    u128(0xFFFF000000000000, 0x0000000000000000),
    u128(0xFF00000000000000, 0x0000000000000000),
};

const u128 ppu_recompiler::xmmStvrxMask[] = {
    u128(0x0000000000000000, 0x0000000000000000),
    u128(0x0000000000000000, 0x00000000000000FF),
    u128(0x0000000000000000, 0x000000000000FFFF),
    u128(0x0000000000000000, 0x0000000000FFFFFF),
    u128(0x0000000000000000, 0x00000000FFFFFFFF),
    u128(0x0000000000000000, 0x000000FFFFFFFFFF),
    u128(0x0000000000000000, 0x0000FFFFFFFFFFFF),
    u128(0x0000000000000000, 0x00FFFFFFFFFFFFFF),
    u128(0x0000000000000000, 0xFFFFFFFFFFFFFFFF),
    u128(0x00000000000000FF, 0xFFFFFFFFFFFFFFFF),
    u128(0x000000000000FFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x0000000000FFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x00000000FFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x000000FFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x0000FFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x00FFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
};

const u128 ppu_recompiler::xmmVsloMask[] = {
    u128(0x0F0E0D0C0B0A0908, 0x0706050403020100),
    u128(0x0E0D0C0B0A090807, 0x06050403020100FF),
    u128(0x0D0C0B0A09080706, 0x050403020100FFFF),
    u128(0x0C0B0A0908070605, 0x0403020100FFFFFF),
    u128(0x0B0A090807060504, 0x03020100FFFFFFFF),
    u128(0x0A09080706050403, 0x020100FFFFFFFFFF),
    u128(0x0908070605040302, 0x0100FFFFFFFFFFFF),
    u128(0x0807060504030201, 0x00FFFFFFFFFFFFFF),
    u128(0x0706050403020100, 0xFFFFFFFFFFFFFFFF),
    u128(0x06050403020100FF, 0xFFFFFFFFFFFFFFFF),
    u128(0x050403020100FFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x0403020100FFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x03020100FFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x020100FFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x0100FFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    u128(0x00FFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
};

ppu_recompiler::ppu_recompiler()
    : m_jit(std::make_shared<asmjit::JitRuntime>())
{
    asmjit::CpuInfo inf;
    inf.detect();

    LOG_SUCCESS(PPU, "PPU Recompiler (ASMJIT) created...");

    PPUJIT_LOGGING(fs::file(fs::get_config_dir() + "PPUJIT.log", fs::rewrite).write(fmt::format("PPU JIT initialization...\n\nTitle: %s\nTitle ID: %s\n\n", Emu.GetTitle().c_str(), Emu.GetTitleID().c_str())));

    // check my sanity
    if (sizeof(bool) != 1) {
        fmt::throw_exception("PPU-Recomp, expected bool to be size of 1");
    }
}


// helper function to handle analyse/compile logic for funccaller/dispatcher
u64 AnalyseCompile(u32 addr, ppu_thread* ppu) {
    if (!ppu->ppu_rec)
    {
        ppu->ppu_rec = fxm::get_always<ppu_recompiler>();
    }

    const auto func = ppu->ppu_db->analyse(addr);

    if (func->can_be_compiled && !func->compiled)
    {
        ppu->ppu_rec->compile(*func, *ppu);
    }
    else {
        fmt::throw_exception("func cant be compiled");
    }

    assert(func->compiled != 0);
    return (u64)func->compiled;
}

// the idea with this function is just to push all callee saved registers to serve as a loader 
//  for recompiled functions, which allows us to ignore calling conventions and jump between
//  compiled blocks and hopefully avoid a stack overflow
void ppu_recompiler::createFuncCaller(ppu_thread& ppu) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_funcCaller)
        return;

    using namespace asmjit;
    X86Assembler as(m_jit.get());
    //RBX RSI RDI RBP R12 R13 R14 R15
    as.push(asmjit::host::rbx);
    as.push(asmjit::host::rsi);
    as.push(asmjit::host::rdi);
    as.push(asmjit::host::rbp);
    as.push(asmjit::host::r12);
    as.push(asmjit::host::r13);
    as.push(asmjit::host::r14);
    as.push(asmjit::host::r15);

    // need to save xmm6-15, 10 registers

    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x18), asmjit::host::xmm6);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x28), asmjit::host::xmm7);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x38), asmjit::host::xmm8);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x48), asmjit::host::xmm9);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x58), asmjit::host::xmm10);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x68), asmjit::host::xmm11);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x78), asmjit::host::xmm12);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x88), asmjit::host::xmm13);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0x98), asmjit::host::xmm14);
    as.movdqa(asmjit::host::oword_ptr(asmjit::host::rsp, -0xa8), asmjit::host::xmm15);

    // xmm registers, 10 * 16, + alignment + plus shadow stack 0x20 = c8
    as.sub(asmjit::host::rsp, 0x0c8);

    //rcx contains function, rdx pputhread*
    as.mov(asmjit::host::rax, asmjit::host::rcx);
    as.mov(asmjit::host::rcx, asmjit::host::rdx);

    asmjit::Label callFunc = as.newLabel();
    asmjit::Label endDispatcher = as.newLabel();
    asmjit::Label compileFunc = as.newLabel();

    as.bind(callFunc);
    // call compiled func
    as.call(asmjit::host::rax);
    // compiled functions need to return address in eax, and *pputhread in rcx

    // check addr of rtn
    // for now just bigger than equal 0x20000000 which signals we have to break

    as.cmp(asmjit::host::eax, 0x020000000);
    as.jae(endDispatcher);

    // save regs we care about
    as.mov(asmjit::host::r12d, asmjit::host::eax); //addr
    as.mov(asmjit::host::r13, asmjit::host::rcx); // pputhread

                                                  // check if we the addr compilied
    as.mov(asmjit::host::edx, asmjit::host::eax);
    as.shr(asmjit::host::edx, 2);

    as.mov(asmjit::host::rax, asmjit::host::qword_ptr_abs((asmjit::Ptr)ppu.ppu_db->funcPointers.data(), asmjit::host::rdx, 3));
    as.test(asmjit::host::rax, asmjit::host::rax);

    as.jz(compileFunc);

    // k looks like we have it, call func, rax and rcx should be good
    as.jmp(callFunc);

    as.bind(compileFunc);

    // mov regs for call
    as.mov(asmjit::host::rcx, asmjit::host::r12);
    as.mov(asmjit::host::rdx, asmjit::host::r13);

    as.mov(asmjit::host::rax, asmjit::imm_ptr(&AnalyseCompile));
    as.call(asmjit::host::rax);

    // restore cpu
    as.mov(asmjit::host::rcx, asmjit::host::r13);

    as.jmp(callFunc);

    as.bind(endDispatcher);

    // get rid of our flags...dont need to for right now, just leave them
    //as.and_(asmjit::host::eax, 0x1FFFFFFF);

    as.add(asmjit::host::rsp, 0x0c8);

    as.movdqa(asmjit::host::xmm15, asmjit::host::oword_ptr(asmjit::host::rsp, -0xa8));
    as.movdqa(asmjit::host::xmm14, asmjit::host::oword_ptr(asmjit::host::rsp, -0x98));
    as.movdqa(asmjit::host::xmm13, asmjit::host::oword_ptr(asmjit::host::rsp, -0x88));
    as.movdqa(asmjit::host::xmm12, asmjit::host::oword_ptr(asmjit::host::rsp, -0x78));
    as.movdqa(asmjit::host::xmm11, asmjit::host::oword_ptr(asmjit::host::rsp, -0x68));
    as.movdqa(asmjit::host::xmm10, asmjit::host::oword_ptr(asmjit::host::rsp, -0x58));
    as.movdqa(asmjit::host::xmm9, asmjit::host::oword_ptr(asmjit::host::rsp, -0x48));
    as.movdqa(asmjit::host::xmm8, asmjit::host::oword_ptr(asmjit::host::rsp, -0x38));
    as.movdqa(asmjit::host::xmm7, asmjit::host::oword_ptr(asmjit::host::rsp, -0x28));
    as.movdqa(asmjit::host::xmm6, asmjit::host::oword_ptr(asmjit::host::rsp, -0x18));

    as.pop(asmjit::host::r15);
    as.pop(asmjit::host::r14);
    as.pop(asmjit::host::r13);
    as.pop(asmjit::host::r12);
    as.pop(asmjit::host::rbp);
    as.pop(asmjit::host::rdi);
    as.pop(asmjit::host::rsi);
    as.pop(asmjit::host::rbx);

    as.ret();

    m_funcCaller = asmjit_cast<ppu_jit_func_caller_t>(as.make());
}

void ppu_recompiler::compile(ppu_rec_function_t& f, ppu_thread& ppu)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (f.compiled || !f.can_be_compiled)
    {
        // return if function already compiled
        return;
    }

    using namespace asmjit;

    PPUDisAsm dis_asm(CPUDisAsm_InterpreterMode);
    dis_asm.offset = reinterpret_cast<u8*>(f.data.data()) - f.addr;

    StringLogger logger;
    logger.addOptions(Logger::Options::kOptionBinaryForm);

    u32 endAddr = f.addr + f.size;

    PPUJIT_LOGGING(std::string log = fmt::format("========== PPU FUNCTION 0x%05x - 0x%05x ==========\n\n", f.addr, endAddr));

    this->m_func = &f;
    X86Assembler assembler(m_jit.get());
    this->a = &assembler;
    PPUJIT_LOGGING(assembler.setLogger(&logger));


    // Initialize variables
    this->cpu = &asmjit::host::rbx;
    this->baseReg = &asmjit::host::rbp;

    // scratch registers, keeping these as rax and rdx helps with 1 operand multiples
    this->addrReg = &asmjit::host::rax;
    this->qr0 = &asmjit::host::rdx;

    this->xr0 = &asmjit::host::xmm0;
    this->xr1 = &asmjit::host::xmm1;

    this->ppu = &ppu;

    for (auto i : f.calledFunctions) {
        // weed out any functions that our outside our block for now
        if (i < endAddr && i >= f.addr) {
            branchLabels.emplace(i, assembler.newLabel());
        }
    }

    // init cell-grps

    gprsUsed = 0;

    // careful with changing this array, some instructions are hardcoded currently
    cellGprs[0] = gpr_link(asmjit::host::rsi);
    // note: rdi is used for Stvlx maskmovdqu, and is hardcoded...dont change this array without looking there
    cellGprs[1] = gpr_link(asmjit::host::rdi);
    cellGprs[2] = gpr_link(asmjit::host::r13);
    cellGprs[3] = gpr_link(asmjit::host::r14);
    cellGprs[4] = gpr_link(asmjit::host::r15);
    cellGprs[5] = gpr_link(asmjit::host::r8);
    cellGprs[6] = gpr_link(asmjit::host::r9);
    cellGprs[7] = gpr_link(asmjit::host::r10);
    cellGprs[8] = gpr_link(asmjit::host::r11);
    cellGprs[9] = gpr_link(asmjit::host::r12);
    cellGprs[10] = gpr_link(asmjit::host::rcx);

    // init xmm's, shared between vpr's and fpr's 
    fprsUsed = 0;
    vprsUsed = 0;

    // currently using xmm0 and xmm1 for scratch
    cellXmms[0] = xmm_link(asmjit::host::xmm2);
    cellXmms[1] = xmm_link(asmjit::host::xmm3);
    cellXmms[2] = xmm_link(asmjit::host::xmm4);
    cellXmms[3] = xmm_link(asmjit::host::xmm5);
    cellXmms[4] = xmm_link(asmjit::host::xmm6);
    cellXmms[5] = xmm_link(asmjit::host::xmm7);
    cellXmms[6] = xmm_link(asmjit::host::xmm8);
    cellXmms[7] = xmm_link(asmjit::host::xmm9);
    cellXmms[8] = xmm_link(asmjit::host::xmm10);
    cellXmms[9] = xmm_link(asmjit::host::xmm11);
    cellXmms[10] = xmm_link(asmjit::host::xmm12);
    cellXmms[11] = xmm_link(asmjit::host::xmm13);
    cellXmms[12] = xmm_link(asmjit::host::xmm14);
    cellXmms[13] = xmm_link(asmjit::host::xmm15);


    // Register label for the function return
    Label end_label = assembler.newLabel();
    this->end = &end_label;

    // Start compilation
    m_pos = f.addr;

    // set cpu register
    a->mov(*cpu, asmjit::host::rcx);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));

    // use this to try to avoid avx transition penatly
    // as long as we dont touch the high bits/ymm we should stay in the right state
    a->vzeroupper();

    for (const u32 op : f.data) {
        // reset 'reg locks'
        gprsUsed = 0;
        fprsUsed = 0;
        vprsUsed = 0;

        // set branch label if necessary
        // if it is a potential jump target, we need to clear registers before going in currently
        this->jumpInternal = nullptr;
        auto jmpCheck = this->branchLabels.find(m_pos);
        if (jmpCheck != this->branchLabels.end()) {
            this->jumpInternal = &jmpCheck->second;
        }
        // Disasm
#ifdef PPUJIT_LOGGING_ON
        dis_asm.dump_pc = m_pos;
        dis_asm.disasm(m_pos);
        //assembler._comment(dis_asm.last_opcode.c_str());
        log += dis_asm.last_opcode.c_str();
        log += '\n';
#endif
        // Recompiler function
        (this->*s_ppu_decoder.decode(op))({ op });

        // Set next position
        m_pos += 4;
    }

    PPUJIT_LOGGING(log += '\n');

    // sanity check
    if (branchLabels.find(m_pos) != branchLabels.end())
        assert(false);

    // clear out registers if we haven't yet
    SaveRegisterState();
    assembler.mov(addrReg->r32(), m_pos);
    assembler.bind(end_label);

    a->mov(asmjit::host::rcx, *cpu);
    a->mov(addrReg->r32(), addrReg->r32());
    assembler.ret();

    f.compiled = asmjit_cast<ppu_jit_func_t>(assembler.make());

    // Add ASMJIT logs
    // this thing kills speed tremendously, putting it before finalize / make 
    // outputs just rpcs3 disassembly, which is fast enough to see code 
#ifdef PPUJIT_LOGGING_ON
    log += logger.getString();
    log += "\n\n\n";

    // Append log file
    fs::file(fs::get_config_dir() + "PPUJIT.log", fs::write + fs::append).write(log);
#endif

    // sanity check
    if (branchLabels.size() != branchLabelsUsed.size())
        assert(false);


    // clear old branch labels as compile class is reused
    this->branchLabels.clear();
    this->branchLabelsUsed.clear();

    ppu.ppu_db->funcPointers[f.addr >> 2] = (u8*)f.compiled;
}

// this currently needs to be at the start of every function
// call saveusedgprs before this function if it is a branch or interp function
inline void ppu_recompiler::CheckSetJumpTarget() {
    if (this->jumpInternal)
    {
        if (branchLabelsUsed.find(m_pos) != branchLabelsUsed.end())
            assert(false);
        branchLabelsUsed.insert(m_pos);
        SaveRegisterState();
        a->bind(*jumpInternal);
    }
}

const asmjit::X86GpReg* ppu_recompiler::GetLoadCellGpr(u32 regNum, bool load) {
    u32 regMask = 1 << regNum;
    // check if we have a register
    for (auto &reg : cellGprs) {
        if (reg.regNum == regNum) {
            if (load && !reg.isLoaded && !reg.isDirty)
                a->mov(*reg.reg, PPU_OFF_64(gpr[regNum]));
            reg.isLoaded = load ? load : reg.isLoaded;
            //reg.isDirty = false;
            gprsUsed |= regMask;
            return reg.reg;
        }
    }

    // k we dont have a register
    for (auto &reg : cellGprs) {
        if (reg.regNum == -1) {
            if (load)
                a->mov(*reg.reg, PPU_OFF_64(gpr[regNum]));
            reg.isLoaded = load;
            reg.isDirty = false;
            gprsUsed |= regMask;
            reg.regNum = regNum;
            return reg.reg;
        }
    }

    // ok we have to forcefully take a register
    for (u32 i = 0; i < 32; ++i) {
        regMask = 1 << i;
        if ((gprsUsed & regMask) == 0) {
            // ok this register isnt used so far this instruction
            for (auto &reg : cellGprs) {
                assert(reg.regNum != -1);
                if (reg.regNum != i)
                    continue;
                if (reg.isDirty)
                    a->mov(PPU_OFF_64(gpr[reg.regNum]), *reg.reg);
                if (load)
                    a->mov(*reg.reg, PPU_OFF_64(gpr[regNum]));
                reg.isDirty = false;
                reg.isLoaded = load;
                reg.regNum = regNum;
                gprsUsed |= (1 << regNum);
                return reg.reg;
            }
        }
    }

    assert(false);
}

void ppu_recompiler::MakeCellGprDirty(u32 regNum) {
    for (auto &reg : cellGprs) {
        if (reg.regNum == regNum) {
            reg.isDirty = true;
            return;
        }
    }
    assert(false);
}

// currently call this any time we jump or are leaving block, interp call, etc
// although we have to bind internal jumps after this 
void ppu_recompiler::SaveUsedCellGprsToMem(bool fullReset) {
    for (auto &reg : cellGprs) {
        if (reg.regNum != -1) {
            if (reg.isDirty)
                a->mov(PPU_OFF_64(gpr[reg.regNum]), *reg.reg);
            if (fullReset) {
                reg.regNum = -1;
                reg.isDirty = false;
                reg.isLoaded = false;
            }
        }
    }
    if (fullReset)
        gprsUsed = 0;
}

// Loads float into xmm reg
// there's a transition penatly if we load it wrong here, but for now im ignoring it
// todo: keep track of precision type currently loaded, possibly force all avx and/or support sse2
const asmjit::X86XmmReg* ppu_recompiler::GetLoadCellFpr(u32 regNum, bool load, bool makeDirty) {
    u32 regMask = 1 << regNum;
    // check if we have a register
    for (auto &reg : cellXmms) {
        if (reg.fprRegNum == regNum) {
            if (load && !reg.isLoaded && !reg.isDirty)
                a->movsd(*reg.reg, PPU_OFF_64(fpr[regNum]));
            reg.isLoaded = load ? load : reg.isLoaded;
            reg.isDirty = makeDirty ? makeDirty : reg.isDirty;
            fprsUsed |= regMask;
            return reg.reg;
        }
    }

    // k we dont have a register, try to take one that isnt used by vpr as well
    for (auto &reg : cellXmms) {
        if (reg.fprRegNum == -1 && reg.vprRegNum == -1) {
            if (load)
                a->movsd(*reg.reg, PPU_OFF_64(fpr[regNum]));

            reg.isLoaded = load;
            reg.isDirty = makeDirty;
            fprsUsed |= regMask;
            reg.fprRegNum = regNum;
            return reg.reg;
        }
    }

    // ok we have to forcefully take a register, for now just take anything
    for (u32 i = 0; i < 32; ++i) {
        regMask = 1 << i;
        if ((fprsUsed & regMask) == 0 || (vprsUsed & regMask) == 0) {
            // ok this register isnt used so far this instruction
            for (auto &reg : cellXmms) {
                assert(reg.fprRegNum != -1 || reg.vprRegNum != -1);
                if (reg.fprRegNum != i && reg.vprRegNum != i)
                    continue;
                if (reg.fprRegNum == i && ((fprsUsed & regMask) == 0)) {
                    if (reg.isDirty)
                        a->movsd(PPU_OFF_64(fpr[reg.fprRegNum]), *reg.reg);
                }
                else if (reg.vprRegNum == i && (vprsUsed & regMask) == 0) {
                    if (reg.isDirty)
                        a->movdqa(PPU_OFF_128(vr[reg.vprRegNum]), *reg.reg);
                    reg.vprRegNum = -1;
                }
                else {
                    continue;
                }
                if (load)
                    a->movsd(*reg.reg, PPU_OFF_64(fpr[regNum]));
                reg.isDirty = makeDirty;
                reg.isLoaded = load;
                reg.fprRegNum = regNum;
                fprsUsed |= regMask;
                return reg.reg;
            }
        }
    }

    assert(false);
}

const asmjit::X86XmmReg* ppu_recompiler::GetLoadCellVpr(u32 regNum, bool load, bool makeDirty) {
    u32 regMask = 1 << regNum;
    // check if we have a register
    for (auto &reg : cellXmms) {
        if (reg.vprRegNum == regNum) {
            if (load && !reg.isLoaded && !reg.isDirty)
                a->movdqa(*reg.reg, PPU_OFF_128(vr[regNum]));
            reg.isLoaded = load ? load : reg.isLoaded;
            reg.isDirty = makeDirty ? makeDirty : reg.isDirty;
            vprsUsed |= regMask;
            return reg.reg;
        }
    }

    // k we dont have a register, try to take one that isnt used by fpr as well
    for (auto &reg : cellXmms) {
        if (reg.fprRegNum == -1 && reg.vprRegNum == -1) {
            if (load)
                a->movdqa(*reg.reg, PPU_OFF_128(vr[regNum]));

            reg.isLoaded = load;
            reg.isDirty = makeDirty;
            vprsUsed |= regMask;
            reg.vprRegNum = regNum;
            return reg.reg;
        }
    }

    // ok we have to forcefully take a register, for now just take anything
    for (u32 i = 0; i < 32; ++i) {
        regMask = 1 << i;
        if ((fprsUsed & regMask) == 0 || (vprsUsed & regMask) == 0) {
            // ok this register isnt used so far this instruction
            for (auto &reg : cellXmms) {
                assert(reg.fprRegNum != -1 || reg.vprRegNum != -1);
                if (reg.fprRegNum != i && reg.vprRegNum != i)
                    continue;
                if (reg.fprRegNum == i && ((fprsUsed & regMask) == 0)) {
                    if (reg.isDirty)
                        a->movsd(PPU_OFF_64(fpr[reg.fprRegNum]), *reg.reg);
                    reg.fprRegNum = -1;
                }
                else if (reg.vprRegNum == i && ((vprsUsed & regMask) == 0)) {
                    if (reg.isDirty)
                        a->movdqa(PPU_OFF_128(vr[reg.vprRegNum]), *reg.reg);
                }
                else {
                    continue;
                }
                if (load)
                    a->movdqa(*reg.reg, PPU_OFF_128(vr[regNum]));
                reg.isDirty = makeDirty;
                reg.isLoaded = load;
                reg.vprRegNum = regNum;
                vprsUsed |= regMask;
                return reg.reg;
            }
        }
    }

    assert(false);
}

// call this after any actual registers, otherwise this will clobber
const asmjit::X86XmmReg* ppu_recompiler::GetSpareCellVpr() {
    // k we dont have a register, try to take one that isnt used by fpr as well
    // good it isnt used, 
    for (auto &reg : cellXmms) {
        if (reg.fprRegNum == -1 && reg.vprRegNum == -1) {
            return reg.reg;
        }
    }
    // ok we have to forcefully take a register, for now just take anything
    for (u32 i = 0; i < 32; ++i) {
        u32 regMask = 1 << i;
        if ((fprsUsed & regMask) == 0 || (vprsUsed & regMask) == 0) {
            // ok this register isnt used so far this instruction
            for (auto &reg : cellXmms) {
                assert(reg.fprRegNum != -1 || reg.vprRegNum != -1);
                if (reg.fprRegNum != i && reg.vprRegNum != i)
                    continue;
                if (reg.fprRegNum == i && ((fprsUsed & regMask) == 0)) {
                    if (reg.isDirty)
                        a->movsd(PPU_OFF_64(fpr[reg.fprRegNum]), *reg.reg);
                    reg.fprRegNum = -1;
                }
                else if (reg.vprRegNum == i && (vprsUsed & regMask) == 0) {
                    if (reg.isDirty)
                        a->movdqa(PPU_OFF_128(vr[reg.vprRegNum]), *reg.reg);
                }
                else {
                    continue;
                }
                reg.isDirty = false;
                reg.isLoaded = false;
                reg.vprRegNum = -1;
                return reg.reg;
            }
        }
    }
    assert(false);
}

void ppu_recompiler::SaveUsedCellXmmsToMem(bool fullReset) {
    for (auto &reg : cellXmms) {
        if (reg.fprRegNum != -1) {
            assert(reg.vprRegNum == -1);
            if (reg.isDirty)
                a->movsd(PPU_OFF_64(fpr[reg.fprRegNum]), *reg.reg);
        }
        else if (reg.vprRegNum != -1) {
            assert(reg.fprRegNum == -1);
            if (reg.isDirty)
                a->movdqa(PPU_OFF_128(vr[reg.vprRegNum]), *reg.reg);
        }

        if (fullReset) {
            reg.fprRegNum = -1;
            reg.vprRegNum = -1;
            reg.isDirty = false;
            reg.isLoaded = false;
        }
    }
    if (fullReset) {
        fprsUsed = 0;
        vprsUsed = 0;
    }
}

void ppu_recompiler::CellVprLockRegisters(u32 reg1, u32 reg2 /*= 32*/, u32 reg3 /*= 32*/, u32 reg4 /*= 32*/) {
    vprsUsed |= 1 << reg1;
    if (reg2 < 32)
        vprsUsed |= 1 << reg2;
    if (reg3 < 32)
        vprsUsed |= 1 << reg3;
    if (reg4 < 32)
        vprsUsed |= 1 << reg4;
}

void ppu_recompiler::CellFprLockRegisters(u32 reg1, u32 reg2 /*= 32*/, u32 reg3 /*= 32*/, u32 reg4 /*= 32*/) {
    fprsUsed |= 1 << reg1;
    if (reg2 < 32)
        fprsUsed |= 1 << reg2;
    if (reg3 < 32)
        fprsUsed |= 1 << reg3;
    if (reg4 < 32)
        fprsUsed |= 1 << reg4;
}

void ppu_recompiler::CellGprLockRegisters(u32 reg1, u32 reg2 /*= 32*/, u32 reg3 /*= 32*/, u32 reg4 /*= 32*/) {
    gprsUsed |= 1 << reg1;
    if (reg2 < 32)
        gprsUsed |= 1 << reg2;
    if (reg3 < 32)
        gprsUsed |= 1 << reg3;
    if (reg4 < 32)
        gprsUsed |= 1 << reg4;
}

void ppu_recompiler::SaveRegisterState(bool fullReset) {
    SaveUsedCellGprsToMem(fullReset);
    SaveUsedCellXmmsToMem(fullReset);
}

// Helper function
// -- op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
void ppu_recompiler::LoadAddrRbRa0(ppu_opcode_t op) {
    if (op.ra != 0)
        CellGprLockRegisters(op.rb, op.ra);
    else
        CellGprLockRegisters(op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(*addrReg, *rb);
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->add(*addrReg, *ra);
    }
}

// bswap intrinsics found here 
// http://www.alfredklomp.com/programming/sse-intrinsics/

// todo: sse2 detect and version, currently just does ssse3 version
void ppu_recompiler::XmmByteSwap32(const asmjit::X86XmmReg& reg) {
    a->pshufb(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[0])));
}

void ppu_recompiler::XmmByteSwap64(const asmjit::X86XmmReg& reg) {
    a->pshufb(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[1])));
}

void ppu_recompiler::XmmByteSwap128(const asmjit::X86XmmReg& reg) {
    a->pshufb(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[2])));
}

void ppu_recompiler::Negate32Bit(const asmjit::X86XmmReg& reg) {
    a->xorps(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[5])));
}

void ppu_recompiler::Negate64Bit(const asmjit::X86XmmReg& reg) {
    a->xorpd(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[3])));
}

void ppu_recompiler::MaskLow5BitsDWord(const asmjit::X86XmmReg& reg) {
    a->andps(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[6])));
}

void ppu_recompiler::Abs64Bit(const asmjit::X86XmmReg& reg) {
    a->andpd(reg, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[4])));
}

// Helper for setting CR Field after a comparison
// Do a cmp right before this and this will take care of the rest
void ppu_recompiler::SetCRFromCmp(u32 field, bool is_signed) {
    // not sure if jumps or setcc is faster, probly setcc

    if (is_signed) {
        a->setl(PPU_OFF_8(cr[field * 4 + 0]));
        a->setg(PPU_OFF_8(cr[field * 4 + 1]));
        a->sete(PPU_OFF_8(cr[field * 4 + 2]));
    }
    else {
        a->setb(PPU_OFF_8(cr[field * 4 + 0]));
        a->seta(PPU_OFF_8(cr[field * 4 + 1]));
        a->sete(PPU_OFF_8(cr[field * 4 + 2]));
    }

    /*asmjit::Label lessThan = a->newLabel();
    asmjit::Label greaterThan = a->newLabel();
    asmjit::Label endBreak = a->newLabel();

    // set flags up here, we jump over the 'clears'
    a->mov(PPU_OFF_8(cr[field * 4 + 0]), 1);
    a->mov(PPU_OFF_8(cr[field * 4 + 1]), 1);
    a->mov(PPU_OFF_8(cr[field * 4 + 2]), 1);
    if (is_signed)
    a->jl(lessThan);
    else
    a->jb(lessThan);
    a->mov(PPU_OFF_8(cr[field * 4 + 0]), 0);
    a->bind(lessThan);
    if (is_signed)
    a->jg(greaterThan);
    else
    a->ja(greaterThan);
    a->mov(PPU_OFF_8(cr[field * 4 + 1]), 0);
    a->bind(greaterThan);
    a->je(endBreak);
    a->mov(PPU_OFF_8(cr[field * 4 + 2]), 0);
    a->bind(endBreak);*/
}

using ppu_inter_func_t = bool(*)(ppu_thread&, ppu_opcode_t);

void ppu_recompiler::InterpreterCall(ppu_opcode_t op) {
    // Commit any gpr's in registers to memory
    SaveRegisterState();
    CheckSetJumpTarget();

    auto gate = [](ppu_thread* _ppu, u32 opcode, ppu_inter_func_t _func) noexcept -> u32
    {
        try
        {
            // TODO: check correctness

            const u32 old_pc = _ppu->cia;

            if (_ppu->check_state())
            {
                return 0x080000000 | _ppu->cia;
            }

            _func(*_ppu, { opcode });

            if (old_pc != _ppu->cia)
            {
                _ppu->cia += 4;
                return 0x20000000 | _ppu->cia;
            }

            _ppu->cia += 4;
            return _ppu->cia;
        }
        catch (...)
        {
            //_ppu->pending_exception = std::current_exception();
            return 0x40000000 | _ppu->cia;
        }
    };

    a->mov(PPU_OFF_32(cia), m_pos);
    a->mov(asmjit::host::rcx, *cpu);
    a->mov(asmjit::host::rdx, asmjit::imm_u(op.opcode));
    a->mov(asmjit::host::r8, asmjit::imm_ptr(asmjit_cast<void*>(s_ppu_interpreter.decode(op.opcode))));

    a->sub(asmjit::host::rsp, 0x28);
    a->call(asmjit::imm_ptr(asmjit_cast<void*, u32(ppu_thread*, u32, ppu_inter_func_t)>(gate)));
    a->add(asmjit::host::rsp, 0x28);

    //a->test(*addrReg, *addrReg);
    //a->jnz(*end);
    a->cmp(*addrReg, 0x020000000);
    a->jae(*end);
}

void ppu_recompiler::FunctionCall(u32 newPC) {
    auto gate = []() noexcept -> void
    {
        //__debugbreak();
        return;
    };

    a->mov(PPU_OFF_32(cia), m_pos);
    //a->mov(asmjit::host::rcx, *cpu);
    //a->mov(asmjit::host::rdx, asmjit::imm_u(op.opcode));
    //a->mov(asmjit::host::r8, asmjit::imm_ptr(asmjit_cast<void*>(s_ppu_interpreter.decode(op.opcode))));

    a->sub(asmjit::host::rsp, 0x28);
    a->call(asmjit::imm_ptr(asmjit_cast<void*, void(void)>(gate)));
    a->add(asmjit::host::rsp, 0x28);

    //a->test(*addrReg, *addrReg);
    //a->jnz(*end);
    //a->cmp(*addrReg, 0x020000000);
    // a->jae(*end);
}

void ppu_recompiler::HACK(ppu_opcode_t op) {
    InterpreterCall(op);
    //ppu_execute_function(ppu, op.opcode & 0x3ffffff);
}
void ppu_recompiler::SC(ppu_opcode_t op) {
    InterpreterCall(op);
}

// -----------------
// GPR --  Add/sub/mul/div
// -----------------

void ppu_recompiler::ADD(ppu_opcode_t op) {
    GPR_ALU
        if (op.oe) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.ra == op.rd || op.rb == op.rd));
    if (op.ra == op.rb) {
        if (op.rd != op.ra) {
            // a and b the same
            // just grab one and shove into rd
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        // if same, this shifts over, or takes register from above
        a->shl(*rd, 1);
    }
    else if (op.rd == op.ra) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->add(*rd, *rb);
    }
    else if (op.rd == op.rb) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->add(*rd, *ra);
    }
    else {
        // all diff
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*rd, *ra);
        a->add(*rd, *rb);
    }

    // ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDIC(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const s64 a = ppu.gpr[op.ra];
    const s64 i = op.simm16;
    const auto r = add64_flags(a, i);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.main & 1) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(addrReg->r64(), asmjit::imm(op.simm16).getInt32());
    a->mov(*qr0, *ra);

    a->mov(*rd, *ra);
    a->add(*rd, *addrReg);

    // carry 
    a->not_(*qr0);
    a->cmp(*addrReg, *qr0);
    a->seta(PPU_OFF_8(xer.ca));

    if (op.main & 1) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDC(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const u64 RB = ppu.gpr[op.rb];
    const auto r = add64_flags(RA, RB);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((RA >> 63 == RB >> 63) && (RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/

    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(*qr0, *ra);
    a->mov(*addrReg, *rb);

    a->mov(*rd, *qr0);
    a->add(*rd, *addrReg);

    // carry
    a->not_(*qr0);
    a->cmp(*addrReg, *qr0);
    a->seta(PPU_OFF_8(xer.ca));

    if (op.oe) {
        assert(false);
    }
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDI(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    //ppu.gpr[op.rd] = op.ra ? ((s64)ppu.gpr[op.ra] + op.simm16) : op.simm16;
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *ra);
        a->add(rd->r64(), asmjit::imm(op.simm16).getInt32());
    }
    else {
        a->mov(rd->r64(), asmjit::imm(op.simm16).getInt64());
    }
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDIS(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    //ppu.gpr[op.rd] = op.ra ? ((s64)ppu.gpr[op.ra] + (op.simm16 << 16)) : (op.simm16 << 16);
    // same as addi, just 'shift' the immediate
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    if (op.ra && op.simm16 != 0) {
        if (op.ra != op.rd) {
            // different register
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        // add immediate to rd (possibly other register from above)
        a->add(rd->r64(), asmjit::imm(op.simm16 << 16).getInt32());
    }
    else if (op.ra) {
        // simple mov ra -> rd
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *ra);
    }
    else {
        // "lis" 
        a->mov(rd->r64(), asmjit::imm(op.simm16 << 16).getInt32());
    }
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDZE(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const auto r = add64_flags(RA, 0, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((RA >> 63 == 0) && (RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    // addrReg is rax, so we should be ok here

    a->mov(*qr0, *ra);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();
    if (op.ra != op.rd)
        a->mov(*rd, *ra);
    a->adc(*rd, 0);

    // carry
    a->movzx(*addrReg, PPU_OFF_8(xer.ca));
    a->add(*qr0, *addrReg);
    a->cmp(*qr0, *addrReg);
    a->setb(PPU_OFF_8(xer.ca));

    if (op.oe) {
        assert(false);
    }
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::ADDE(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const u64 RB = ppu.gpr[op.rb];
    const auto r = add64_flags(RA, RB, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((RA >> 63 == RB >> 63) && (RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    //stealin basereg
    a->mov(*baseReg, *ra);
    a->add(*baseReg, *rb);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();

    a->mov(*qr0, *ra);
    a->mov(*addrReg, *rb);

    a->mov(*rd, *qr0);
    a->adc(*rd, *addrReg);

    // carry
    a->cmp(*baseReg, *qr0);
    a->setb(qr0->r8());

    a->movzx(*addrReg, PPU_OFF_8(xer.ca));
    a->add(*baseReg, *addrReg);
    a->cmp(*baseReg, *addrReg);
    a->setb(baseReg->r8());

    a->or_(qr0->r8(), baseReg->r8());
    a->mov(PPU_OFF_8(xer.ca), qr0->r8());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }
    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::ADDME(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const s64 RA = ppu.gpr[op.ra];
    const auto r = add64_flags(RA, ~0ull, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((u64(RA) >> 63 == 1) && (u64(RA) >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();

    a->mov(*qr0, *ra);

    a->mov(*rd, *qr0);
    a->adc(*rd, 0xFFFFFFFF);

    // carry
    a->mov(*addrReg, *qr0);
    a->add(*addrReg, 0xFFFFFFFF);

    a->cmp(*addrReg, *qr0);
    a->setb(qr0->r8());

    a->movzx(*baseReg, PPU_OFF_8(xer.ca));
    a->add(*addrReg, *baseReg);
    a->cmp(*addrReg, *baseReg);
    a->setb(addrReg->r8());

    a->or_(qr0->r8(), addrReg->r8());
    a->mov(PPU_OFF_8(xer.ca), qr0->r8());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }
    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}


void ppu_recompiler::SUBFIC(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 a = ppu.gpr[op.ra];
    const s64 i = op.simm16;
    const auto r = add64_flags(~a, i, 1);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;*/

    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    a->mov(*qr0, *ra);
    a->mov(*baseReg, *ra);
    a->neg(*qr0);
    a->mov(*rd, *qr0);
    a->add(*rd, *addrReg);

    // carry
    a->not_(*qr0);
    a->cmp(*addrReg, *qr0);
    a->seta(qr0->r8());

    a->test(*baseReg, *baseReg);
    a->setz(addrReg->r8());
    a->or_(addrReg->r8(), qr0->r8());
    a->mov(PPU_OFF_8(xer.ca), addrReg->r8());

    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::SUBF(ppu_opcode_t op) {
    GPR_ALU
        if (op.oe) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    /*const u64 RA = ppu.gpr[op.ra];
    const u64 RB = ppu.gpr[op.rb];
    ppu.gpr[op.rd] = RB - RA;*/
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, ((op.rd == op.ra || op.rd == op.rb) && (op.rb != op.ra)));
    if (op.rb == op.ra) {
        // a and b the same, just zero
        a->xor_(*rd, *rd);
    }
    else if (op.rd == op.ra) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->neg(*rd);
        a->add(*rd, *rb);
    }
    else if (op.rd == op.rb) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->sub(*rd, *ra);
    }
    else {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *rb);
        a->sub(*rd, *ra);
    }

    // ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::SUBFE(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const u64 RB = ppu.gpr[op.rb];
    const auto r = add64_flags(~RA, RB, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((~RA >> 63 == RB >> 63) && (~RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/

    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(*qr0, *ra);
    a->not_(*qr0);

    //stealin basereg
    a->mov(*baseReg, *rb);
    a->add(*baseReg, *qr0);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();

    a->mov(*addrReg, *rb);
    a->mov(*rd, *rb);
    a->adc(*rd, *qr0);

    // carry
    a->cmp(*baseReg, *qr0);
    a->setb(qr0->r8());

    a->movzx(*addrReg, PPU_OFF_8(xer.ca));
    a->add(*baseReg, *addrReg);
    a->cmp(*baseReg, *addrReg);
    a->setb(baseReg->r8());

    a->or_(baseReg->r8(), qr0->r8());
    a->mov(PPU_OFF_8(xer.ca), baseReg->r8());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::SUBFC(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const u64 RB = ppu.gpr[op.rb];
    const auto r = add64_flags(~RA, RB, 1);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((~RA >> 63 == RB >> 63) && (~RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(*qr0, *ra);
    a->mov(*baseReg, *ra);
    a->mov(*addrReg, *rb);
    a->mov(*rd, *addrReg);
    a->neg(*qr0);

    a->add(*rd, *qr0);

    // carry 
    //stealin basereg
    a->not_(*qr0);
    a->cmp(*addrReg, *qr0);
    a->seta(addrReg->r8());

    a->test(*baseReg, *baseReg);
    a->setz(qr0->r8());
    a->or_(qr0->r8(), addrReg->r8());

    a->mov(PPU_OFF_8(xer.ca), qr0->r8());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::SUBFME(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const auto r = add64_flags(~RA, ~0ull, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((~RA >> 63 == 1) && (~RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();

    a->mov(*qr0, *ra);
    a->not_(*qr0);

    a->mov(*rd, *qr0);
    a->adc(*rd, 0xFFFFFFFF);

    // carry
    a->mov(*addrReg, *qr0);
    a->add(*addrReg, 0xFFFFFFFF);

    a->cmp(*addrReg, *qr0);
    a->setb(qr0->r8());

    a->movzx(*baseReg, PPU_OFF_8(xer.ca));
    a->add(*addrReg, *baseReg);
    a->cmp(*addrReg, *baseReg);
    a->setb(addrReg->r8());

    a->or_(qr0->r8(), addrReg->r8());
    a->mov(PPU_OFF_8(xer.ca), qr0->r8());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::SUBFZE(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u64 RA = ppu.gpr[op.ra];
    const auto r = add64_flags(~RA, 0, ppu.CA);
    ppu.gpr[op.rd] = r.result;
    ppu.CA = r.carry;
    if (op.oe) ppu.SetOV((~RA >> 63 == 0) && (~RA >> 63 != ppu.gpr[op.rd] >> 63));
    if (op.rc) ppu.SetCR<s64>(0, r.result, 0);*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->xor_(*addrReg, *addrReg);
    a->mov(asmjit::host::ah, PPU_OFF_8(xer.ca));
    a->sahf();

    a->mov(*qr0, *ra);
    a->not_(*qr0);
    a->mov(*rd, *qr0);

    a->adc(*rd, 0);

    // carry 

    a->movzx(*addrReg, PPU_OFF_8(xer.ca));
    a->add(*qr0, *addrReg);
    a->cmp(*qr0, *addrReg);
    a->setb(PPU_OFF_8(xer.ca));

    if (op.oe) {
        assert(false);
    }
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::NEG(ppu_opcode_t op) {
    GPR_ALU
        if (op.oe) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const u64 RA = ppu.gpr[op.ra];
    //ppu.gpr[op.rd] = 0 - RA;
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.rd == op.ra));
    if (op.ra != op.rd) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *ra);
    }
    a->neg(*rd);

    // ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULLI(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rd, op.ra);
    if (op.simm16 == 0) {
        // easy
        const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
        a->xor_(*rd, *rd);
        MakeCellGprDirty(op.rd);
        return;
    }

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.ra == op.rd));

    if (op.ra != op.rd) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *ra);
    }

    if (op.simm16 == -1) {
        a->neg(*rd);
        MakeCellGprDirty(op.rd);
        return;
    }

    a->imul(*rd, asmjit::imm(op.simm16));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULLD(ppu_opcode_t op) {
    GPR_ALU
        if (op.oe) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, ((op.rd == op.ra || op.rd == op.rb)));
    if (op.ra == op.rb) {
        if (op.rd != op.ra) {
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        a->imul(*rd, *rd);
    }
    else if (op.rd == op.ra) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->imul(*rd, *rb);
    }
    else if (op.rd == op.rb) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->imul(*rd, *ra);
    }
    else {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*rd, *rb);
        a->imul(*rd, *ra);
    }

    // ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}
void ppu_recompiler::MULLW(ppu_opcode_t op) {
    GPR_ALU
        if (op.oe) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.gpr[op.rd] = (s64)((s64)(s32)ppu.gpr[op.ra] * (s64)(s32)ppu.gpr[op.rb]);
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    if (op.rd == op.ra) {
        a->movsxd(*qr0, rb->r32());
        a->movsxd(*rd, rd->r32());
    }
    else if (op.rd == op.rb) {
        a->movsxd(*qr0, ra->r32());
        a->movsxd(*rd, rd->r32());
    }
    else {
        a->movsxd(*qr0, ra->r32());
        a->movsxd(*rd, rb->r32());
    }
    a->imul(*rd, *qr0);

    // ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULHW(ppu_opcode_t op) {
    GPR_ALU
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //s32 a = (s32)ppu.gpr[op.ra];
    //s32 b = (s32)ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = ((s64)a * (s64)b) >> 32;
    //if (op.rc) ppu.SetCR(0, false, false, false, ppu.SO);
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(asmjit::host::eax, ra->r32());
    a->imul(rb->r32());

    // high bits in edx
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    // top bits are 'undefined' but seem to just be sign extended
    a->movsxd(*rd, asmjit::host::edx);
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULHWU(ppu_opcode_t op) {
    GPR_ALU
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //u32 a = (u32)ppu.gpr[op.ra];
    //u32 b = (u32)ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = ((u64)a * (u64)b) >> 32;
    //if (op.rc) ppu.SetCR(0, false, false, false, ppu.SO);

    // mul uses rax and rdx for result
    // source/dest goes in rax
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(asmjit::host::eax, ra->r32());
    a->mul(rb->r32());

    // high bits in edx
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(rd->r32(), asmjit::host::edx);
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULHD(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    //ppu.gpr[op.rd] = MULH64(ppu.gpr[op.ra], ppu.gpr[op.rb]);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(asmjit::host::rax, *ra);
    a->imul(*rb);

    // high bits in rdx
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*rd, asmjit::host::rdx);

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::MULHDU(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    //ppu.gpr[op.rd] = UMULH64(ppu.gpr[op.ra], ppu.gpr[op.rb]);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(asmjit::host::rax, *ra);
    a->mul(*rb);

    // high bits in rdx
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*rd, asmjit::host::rdx);

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::DIVWU(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const u32 RA = (u32)ppu.gpr[op.ra];
    const u32 RB = (u32)ppu.gpr[op.rb];
    ppu.gpr[op.rd] = RB == 0 ? 0 : RA / RB;
    if (op.oe) ppu.SetOV(RB == 0);
    if (op.rc) ppu.SetCR(0, false, false, false, ppu.SO);*/
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    asmjit::Label skip = a->newLabel();
    a->xor_(*addrReg, *addrReg);

    a->test(rb->r32(), rb->r32());
    a->jz(skip);

    a->mov(*addrReg, *ra);
    a->xor_(*qr0, *qr0);

    a->div(rb->r32());

    a->bind(skip);

    a->mov(rd->r32(), addrReg->r32());


    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->mov(PPU_OFF_8(cr[0]), 0);
        a->mov(PPU_OFF_8(cr[1]), 0);
        a->mov(PPU_OFF_8(cr[2]), 0);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::DIVW(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    asmjit::Label skip = a->newLabel();
    a->xor_(*addrReg, *addrReg);

    a->test(rb->r32(), rb->r32());
    a->jz(skip);

    a->mov(addrReg->r32(), ra->r32());

    a->cdq();
    a->idiv(rb->r32());

    a->bind(skip);

    a->mov(rd->r32(), addrReg->r32());

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::DIVDU(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    //const u64 RA = ppu.gpr[op.ra];
    //const u64 RB = ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = RB == 0 ? 0 : RA / RB;
    //if (op.oe) ppu.SetOV(RB == 0);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    asmjit::Label skip = a->newLabel();
    a->xor_(*addrReg, *addrReg);

    a->test(*rb, *rb);
    a->jz(skip);

    a->mov(*addrReg, *ra);
    a->xor_(*qr0, *qr0);

    a->div(*rb);

    a->bind(skip);

    a->mov(*rd, *addrReg);

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::DIVD(ppu_opcode_t op) {
    GPR_ALU
        CheckSetJumpTarget();
    /*const s64 RA = ppu.gpr[op.ra];
    const s64 RB = ppu.gpr[op.rb];
    const bool o = RB == 0 || ((u64)RA == (1ULL << 63) && RB == -1);
    ppu.gpr[op.rd] = o ? 0 : RA / RB;
    if (op.oe) ppu.SetOV(o);
    if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.rd], 0);*/
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    asmjit::Label skip = a->newLabel();
    a->xor_(*addrReg, *addrReg);

    a->test(*rb, *rb);
    a->jz(skip);

    // check 0x8000... / -1
    // todo: this check will be positive for lots of other values,
    //          unknown if it matters

    a->mov(*qr0, 0x01);

    a->neg(*rb);
    a->and_(*qr0, *rb);
    a->neg(*rb);
    a->shl(*qr0, 63);
    a->and_(*qr0, *ra);
    a->test(*qr0, *qr0);
    a->jnz(skip);

    a->mov(*addrReg, *ra);

    a->cqo();
    a->idiv(*rb);

    a->bind(skip);

    a->mov(*rd, *addrReg);

    if (op.oe) {
        assert(false);
    }

    if (op.rc) {
        a->cmp(*rd, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.rd);
}

// -----------------
// GPR - Bit Manipulation
// ----------------- 

void ppu_recompiler::ANDI(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] & op.uimm16;
    //ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));

    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }

    a->mov(qr0->r32(), asmjit::imm_u(op.uimm16).getUInt32());
    a->and_(*ra, *qr0);

    a->cmp(*ra, 0);
    SetCRFromCmp(0, true);

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::ANDIS(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] & ((u64)op.uimm16 << 16);
    //ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));

    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }

    a->mov(qr0->r32(), asmjit::imm_u(op.uimm16 << 16).getUInt32());
    a->and_(*ra, *qr0);

    a->cmp(*ra, 0);
    SetCRFromCmp(0, true);

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::AND(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] & ppu.gpr[op.rb];
    if (op.ra == op.rs && op.rs == op.rb) {
        a->nop();
        return;
    }

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rb || op.ra == op.rs));

    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->and_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->and_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->and_(*ra, *rb);
    }

    //ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);
    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::ANDC(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] & ~ppu.gpr[op.rb];

    // i think asmjit is only using the 32bit version of this?
    //a->andn(ra->r64(), rb->r64(), rs->r64());
    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rb || op.ra == op.rs));
    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->not_(*ra);
        a->and_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->not_(*rb);
        a->and_(*ra, *rb);
        a->not_(*rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rb);
        a->not_(*ra);
        a->and_(*ra, *rs);
    }

    // ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::ORC(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] | ~ppu.gpr[op.rb];
    CellGprLockRegisters(op.rs, op.ra, op.rb);

    if (op.ra == op.rs && op.rs == op.rb) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
        a->xor_(*ra, *ra);
        a->not_(*ra);
        MakeCellGprDirty(op.ra);
        return;
    }
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));

    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->not_(*ra);
        a->or_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->not_(*rb);
        a->or_(*ra, *rb);
        a->not_(*rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rb);
        a->not_(*ra);
        a->or_(*ra, *rs);
    }

    // ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::OR(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] | ppu.gpr[op.rb];
    CellGprLockRegisters(op.rs, op.ra, op.rb);

    if (op.rs == op.rb) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rb);
        MakeCellGprDirty(op.ra);
        return;
    }

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));
    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->or_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->or_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->or_(*ra, *rb);
    }

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::ORI(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] | op.uimm16;
    if (op.ra == 0 && op.rs == 0 && op.uimm16 == 0) {
        a->nop();
        return;
    }
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));
    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }
    a->mov(qr0->r32(), asmjit::imm_u(op.uimm16).getUInt32());
    a->or_(*ra, *qr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::ORIS(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = ppu.gpr[op.rs] | ((u64)op.uimm16 << 16);
    if (op.ra == op.rs && op.uimm16 == 0) {
        a->nop();
        return;
    }
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));
    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }
    a->mov(qr0->r32(), asmjit::imm_u(op.uimm16 << 16).getUInt32());
    a->or_(*ra, *qr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::XOR(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));

    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->xor_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->xor_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->xor_(*ra, *rb);
    }

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::XORI(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    if (op.rs == 0 && op.ra == 0 && op.uimm16 == 0) {
        a->nop();
        return;
    }

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));
    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }
    a->xor_(ra->r16(), asmjit::imm_u(op.uimm16).getUInt16());
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::XORIS(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    if (op.rs == 0 && op.ra == 0 && op.uimm16 == 0) {
        // noop;
        a->nop();
        return;
    }

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, (op.ra == op.rs));
    if (op.ra != op.rs) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(*ra, *rs);
    }
    a->mov(qr0->r32(), asmjit::imm_u(op.uimm16 << 16).getUInt32());
    a->xor_(*ra, *qr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::NAND(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();

    CellGprLockRegisters(op.rs, op.ra, op.rb);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));
    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->and_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->and_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->and_(*ra, *rb);
    }
    a->not_(*ra);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::NOR(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));
    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->or_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->or_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->or_(*ra, *rb);
    }
    a->not_(*ra);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::EQV(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, ((op.ra == op.rb) || (op.ra == op.rs)));
    if (op.ra == op.rb) {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->xor_(*ra, *rs);
    }
    else if (op.ra == op.rs) {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->xor_(*ra, *rb);
    }
    else {
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->mov(*ra, *rs);
        a->xor_(*ra, *rb);
    }
    a->not_(*ra);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::EXTSB(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->movsx(ra->r64(), rs->r8Lo());

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::EXTSH(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->movsx(ra->r64(), rs->r16());

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::EXTSW(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);

    a->movsxd(*ra, rs->r32());

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::CNTLZW(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    a->lzcnt(ra->r32(), rs->r32());

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::CNTLZD(ppu_opcode_t op) {
    GPR_BIT
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    a->lzcnt(*ra, *rs);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

// -----------------
// GPR - Shifts
// ----------------- 

void ppu_recompiler::SRW(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = (ppu.gpr[op.rs] & 0xffffffff) >> (ppu.gpr[op.rb] & 0x3f);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    // todo: this probly can be done with 64 bit shift

    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x03f);
    a->mov(addrReg->r32(), rs->r32());

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::cl, qr0->r8());

    a->shr(addrReg->r32(), asmjit::host::cl);

    // get high bit and shift down to force full shift out
    a->and_(asmjit::host::cl, 0x20);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->shr(addrReg->r32(), asmjit::host::cl);
    a->shr(addrReg->r32(), asmjit::host::cl);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::SLW(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = u32(ppu.gpr[op.rs] << (ppu.gpr[op.rb] & 0x3f));
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    // todo: this probly can be done with 64 bit shift
    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x03f);
    a->mov(addrReg->r32(), rs->r32());

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }
    a->mov(asmjit::host::cl, qr0->r8());

    a->shl(addrReg->r32(), asmjit::host::cl);

    // get high bit and shift down to force full shift out
    a->and_(asmjit::host::cl, 0x20);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->shl(addrReg->r32(), asmjit::host::cl);
    a->shl(addrReg->r32(), asmjit::host::cl);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

// todo: thoughts, would xmm register be better here? xmm allows full shift 
void ppu_recompiler::SRD(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x07f);
    a->mov(*addrReg, *rs);

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::cl, qr0->r8());
    a->shr(*addrReg, asmjit::host::cl);

    // get high bit and shift down to force full shift out
    a->and_(asmjit::host::cl, 0x40);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->shr(*addrReg, asmjit::host::cl);
    a->shr(*addrReg, asmjit::host::cl);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

// todo: thoughts, would xmm register be better here? xmm allows full shift 
void ppu_recompiler::SLD(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x07f);
    a->mov(*addrReg, *rs);

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }
    a->mov(asmjit::host::cl, qr0->r8());
    a->shl(*addrReg, asmjit::host::cl);

    // get high bit and shift down to force full shift out
    a->and_(asmjit::host::cl, 0x40);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->shl(*addrReg, asmjit::host::cl);
    a->shl(*addrReg, asmjit::host::cl);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::SRAW(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    /*s32 RS = (s32)ppu.gpr[op.rs];
    u8 shift = ppu.gpr[op.rb] & 63;
    if (shift > 31)
    {
    ppu.gpr[op.ra] = 0 - (RS < 0);
    ppu.CA = (RS < 0);
    }
    else
    {
    ppu.gpr[op.ra] = RS >> shift;
    ppu.CA = (RS < 0) && ((ppu.gpr[op.ra] << shift) != RS);
    }

    if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);*/

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x03f);
    a->mov(*addrReg, *rs);

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::cl, qr0->r8());
    a->shl(*addrReg, 32);
    a->sar(*addrReg, asmjit::host::cl);

    // ca bit and finalize
    a->mov(qr0->r32(), addrReg->r32());
    a->shr(*addrReg, 32);
    a->test(qr0->r32(), addrReg->r32());
    a->setnz(PPU_OFF_8(xer.ca));

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->movsxd(*ra, addrReg->r32());

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::SRAWI(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    /*s32 RS = (u32)ppu.gpr[op.rs];
    ppu.gpr[op.ra] = RS >> op.sh32;
    ppu.CA = (RS < 0) && ((u32)(ppu.gpr[op.ra] << op.sh32) != RS);

    if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);*/
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    if (op.sh32 == 0) {
        a->movsxd(*ra, rs->r32());
        a->mov(PPU_OFF_8(xer.ca), 0);
    }
    else {
        // mask is luckily the same for x86 and ppc here
        a->mov(addrReg->r32(), rs->r32());
        // store
        if (op.ra != op.rs)
            a->mov(ra->r32(), addrReg->r32());
        a->sar(ra->r32(), op.sh32);
        a->shl(addrReg->r32(), 32 - op.sh32);
        a->test(addrReg->r32(), ra->r32());
        a->setnz(PPU_OFF_8(xer.ca));

        a->movsxd(*ra, ra->r32());
    }


    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::SRAD(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    /*s64 RS = ppu.gpr[op.rs];
    u8 shift = ppu.gpr[op.rb] & 127;
    if (shift > 63)
    {
    ppu.gpr[op.ra] = 0 - (RS < 0);
    ppu.CA = (RS < 0);
    }
    else
    {
    ppu.gpr[op.ra] = RS >> shift;
    ppu.CA = (RS < 0) && ((ppu.gpr[op.ra] << shift) != RS);
    }

    if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);*/

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(*qr0, *rb);
    a->and_(qr0->r8(), 0x07f);
    a->mov(*addrReg, *rs);

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::cl, qr0->r8());
    a->sar(*addrReg, asmjit::host::cl);

    // get high bit and shift down to force full shift out
    a->and_(asmjit::host::cl, 0x40);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->sar(*addrReg, asmjit::host::cl);
    a->sar(*addrReg, asmjit::host::cl);

    // ca bit and finalize
    a->mov(asmjit::host::cl, qr0->r8());
    a->mov(*qr0, *addrReg);
    a->shl(*qr0, asmjit::host::cl);

    // bleh...
    a->and_(asmjit::host::cl, 0x40);
    a->shr(asmjit::host::cl, 1);

    // probly a better way to do this, but its needed for full shift
    a->shl(*qr0, asmjit::host::cl);
    a->shl(*qr0, asmjit::host::cl);

    // todo: dont do this, if rs was loaded into rcx this generates a double load...lovely
    rs = GetLoadCellGpr(op.rs, true);
    a->cmp(*qr0, *rs);
    a->setne(qr0->r8());
    a->shl(*qr0, 63);
    a->test(*qr0, *addrReg);
    a->setnz(PPU_OFF_8(xer.ca));

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::SRADI(ppu_opcode_t op) {
    GPR_SHIFT
        CheckSetJumpTarget();
    /*	auto sh = op.sh64;
    s64 RS = ppu.gpr[op.rs];
    ppu.gpr[op.ra] = RS >> sh;
    ppu.CA = (RS < 0) && ((ppu.gpr[op.ra] << sh) != RS);

    if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);*/
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    if (op.sh64 == 0) {
        if (op.ra != op.rs)
            a->mov(*ra, *rs);
        a->mov(PPU_OFF_8(xer.ca), 0);
    }
    else {
        // mask is luckily the same for x86 and ppc here
        a->mov(*addrReg, *rs);
        // store
        if (op.ra != op.rs)
            a->mov(*ra, *addrReg);
        a->sar(*ra, op.sh64);
        a->shl(*addrReg, 64 - op.sh64);
        a->test(*addrReg, *ra);
        a->setnz(PPU_OFF_8(xer.ca));
    }

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

// -----------------
// GPR - Rotates
// ----------------- 

void ppu_recompiler::RLWIMI(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    const u64 mask = ppu_rotate_mask(32 + op.mb32, 32 + op.me32);
    //ppu.gpr[op.ra] = (ppu.gpr[op.ra] & ~mask) | (dup32(rol32(u32(ppu.gpr[op.rs]), op.sh32)) & mask);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);

    a->mov(qr0->r32(), rs->r32());
    a->rol(qr0->r32(), op.sh32);
    a->mov(addrReg->r32(), qr0->r32());
    a->shl(*addrReg, 32);
    a->or_(*qr0, *addrReg);

    a->mov(*addrReg, mask);

    a->and_(*qr0, *addrReg);

    a->not_(*addrReg);
    a->and_(*ra, *addrReg);

    a->or_(*ra, *qr0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::RLWINM(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = dup32(rol32(u32(ppu.gpr[op.rs]), op.sh32)) & ppu_rotate_mask(32 + op.mb32, 32 + op.me32);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);
    const u64 mask = ppu_rotate_mask(32 + op.mb32, 32 + op.me32);
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(qr0->r32(), rs->r32());
    a->rol(qr0->r32(), op.sh32);
    a->mov(addrReg->r32(), qr0->r32());
    a->shl(*addrReg, 32);
    a->or_(*qr0, *addrReg);

    a->mov(*ra, *qr0);

    a->mov(*addrReg, mask);
    a->and_(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLWNM(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = dup32(rol32(u32(ppu.gpr[op.rs]), ppu.gpr[op.rb] & 0x1f)) & ppu_rotate_mask(32 + op.mb32, 32 + op.me32);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(32 + op.mb32, 32 + op.me32);
    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(addrReg->r32(), rs->r32());
    a->mov(*qr0, *rb);

    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::rcx, *qr0);
    a->and_(asmjit::host::rcx, 0x1f);

    a->rol(addrReg->r32(), asmjit::host::cl);
    a->mov(qr0->r32(), addrReg->r32());
    a->shl(*qr0, 32);
    a->or_(*addrReg, *qr0);

    a->mov(*qr0, mask);
    a->and_(*addrReg, *qr0);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);
    a->mov(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDICL(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = rol64(ppu.gpr[op.rs], op.sh64) & ppu_rotate_mask(op.mbe64, 63);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(op.mbe64, 63);
    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    if (op.ra != op.rs) {
        a->mov(*ra, *rs);
    }

    if (op.sh64 != 0)
        a->rol(*ra, op.sh64);

    a->mov(*addrReg, asmjit::imm_u(mask));

    a->and_(*ra, *addrReg);

    assert(op.sh64 < 64);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDICR(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = rol64(ppu.gpr[op.rs], op.sh64) & ppu_rotate_mask(0, op.mbe64);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(0, op.mbe64);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    if (op.ra != op.rs) {
        a->mov(*ra, *rs);
    }

    a->rol(*ra, op.sh64);

    a->mov(*addrReg, mask);

    a->and_(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDIC(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = rol64(ppu.gpr[op.rs], op.sh64) & ppu_rotate_mask(op.mbe64, op.sh64 ^ 63);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(op.mbe64, op.sh64 ^ 63);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    if (op.ra != op.rs) {
        a->mov(*ra, *rs);
    }

    a->rol(*ra, op.sh64);

    a->mov(*addrReg, mask);

    a->and_(*ra, *addrReg);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDIMI(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();

    const u64 mask = ppu_rotate_mask(op.mbe64, op.sh64 ^ 63);
    //ppu.gpr[op.ra] = (ppu.gpr[op.ra] & ~mask) | (rol64(ppu.gpr[op.rs], op.sh64) & mask);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);

    a->mov(*qr0, *rs);
    a->rol(*qr0, op.sh64);

    a->mov(*addrReg, mask);

    a->and_(*qr0, *addrReg);

    a->not_(*addrReg);
    a->and_(*ra, *addrReg);

    a->or_(*ra, *qr0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDCL(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = rol64(ppu.gpr[op.rs], ppu.gpr[op.rb] & 0x3f) & ppu_rotate_mask(op.mbe64, 63);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(op.mbe64, 63);

    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);

    a->mov(*qr0, *rb);
    a->mov(*addrReg, *rs);

    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::rcx, *qr0);
    a->and_(asmjit::host::rcx, 0x03f);

    a->rol(*addrReg, asmjit::host::cl);

    a->mov(*qr0, mask);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);
    a->and_(*ra, *qr0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::RLDCR(ppu_opcode_t op) {
    GPR_ROTATE
        CheckSetJumpTarget();
    //ppu.gpr[op.ra] = rol64(ppu.gpr[op.rs], ppu.gpr[op.rb] & 0x3f) & ppu_rotate_mask(0, op.mbe64);
    //if (op.rc) ppu.SetCR<s64>(0, ppu.gpr[op.ra], 0);

    const u64 mask = ppu_rotate_mask(0, op.mbe64);
    CellGprLockRegisters(op.rs, op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);

    a->mov(*qr0, *rb);
    a->mov(*addrReg, *rs);

    if (cellGprs[10].regNum != -1) {
        if (cellGprs[10].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    a->mov(asmjit::host::rcx, *qr0);
    a->and_(asmjit::host::rcx, 0x03f);

    a->rol(*addrReg, asmjit::host::cl);

    a->mov(*qr0, mask);

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, false);

    a->mov(*ra, *addrReg);
    a->and_(*ra, *qr0);

    if (op.rc) {
        a->cmp(*ra, 0);
        SetCRFromCmp(0, true);
    }

    MakeCellGprDirty(op.ra);
}

// -----------------
// Load/Store -- GPR
// -----------------

void ppu_recompiler::LBZ(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.gpr[op.rd] = vm::read8(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.rd == op.ra));
    if (op.ra && op.simm16 != 0) {
        if (op.ra != op.rd) {
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *rd);
    }
    else if (op.ra) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->movzx(*rd, PPU_PS3_OFF_8(addrReg));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LBZU(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();

    /*const u64 addr = ppu.gpr[op.ra] + op.simm16;
    ppu.gpr[op.rd] = vm::read8(vm::cast(addr, HERE));
    ppu.gpr[op.ra] = addr;*/
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.simm16 != 0) {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, *ra);
    }
    a->movzx(*rd, PPU_PS3_OFF_8(addrReg));
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LBZUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read8(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(*addrReg, *rb);
    a->add(*addrReg, *ra);

    a->movzx(*rd, PPU_PS3_OFF_8(addrReg));
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LBZX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read8(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->movzx(*rd, PPU_PS3_OFF_8(addrReg));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LHZ(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.gpr[op.rd] = vm::read16(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.rd == op.ra));
    if (op.ra && op.simm16 != 0) {
        if (op.ra != op.rd) {
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *rd);
    }
    else if (op.ra) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->movzx(*rd, PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    MakeCellGprDirty(op.rd);
}
void ppu_recompiler::LHZU(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.simm16 != 0) {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, *ra);
    }

    a->movzx(*rd, PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LHA(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.gpr[op.rd] = (s64)(s16)vm::read16(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.ra == op.rd));
    if (op.ra && op.simm16 != 0) {
        if (op.ra != op.rd) {
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *rd);
    }
    else if (op.ra) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movsx(*rd, rd->r16());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LHAU(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //ppu.gpr[op.rd] = (s64)(s16)vm::read16(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.simm16 != 0) {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, *ra);
    }

    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movsx(*rd, rd->r16());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LHAUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = (s64)(s16)vm::read16(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->mov(*addrReg, ra->r32());
    if (op.ra == op.rb) {
        a->shl(*addrReg, 1);
    }
    else {
        const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
        a->add(*addrReg, *rb);
    }

    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movsx(*rd, rd->r16());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LHAX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = (s64)(s16)vm::read16(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movsx(*rd, rd->r16());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LHZX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read16(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);

    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movzx(*rd, rd->r16());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LHZUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read16(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->mov(*addrReg, rb->r32());
    a->add(*addrReg, ra->r32());

    a->mov(rd->r16(), PPU_PS3_OFF_16(addrReg));
    a->ror(rd->r16(), 8);
    a->movzx(*rd, rd->r16());
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LHBRX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::_ref<le_t<u16>>(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->movzx(*rd, PPU_PS3_OFF_16(addrReg));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LWBRX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::_ref<le_t<u32>>(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LWZ(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.gpr[op.rd] = vm::read32(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, (op.ra == op.rd));
    if (op.ra && op.simm16 != 0) {
        if (op.ra != op.rd) {
            const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
            a->mov(*rd, *ra);
        }
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *rd);
    }
    else if (op.ra) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LWZU(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //ppu.gpr[op.rd] = vm::read32(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.simm16 != 0) {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
        a->add(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, *ra);
    }

    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LWZX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read32(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LWA(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = (op.simm16 & ~3) + (op.ra ? ppu.gpr[op.ra] : 0);
    //ppu.gpr[op.rd] = (s64)(s32)vm::read32(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*addrReg, asmjit::imm(op.simm16 & ~3).getInt32());
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->add(*addrReg, *ra);
    }
    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());
    a->movsxd(*rd, rd->r32());
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LWAX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = (s64)(s32)vm::read32(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());
    a->movsxd(*rd, rd->r32());
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LWARX(ppu_opcode_t op) {
    GPR_LOAD
        //todo, reservation call
        InterpreterCall(op);
}

void ppu_recompiler::LWZUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read32(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, *rb);

    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LWAUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = (s64)(s32)vm::read32(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra, op.rb);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, *rb);

    a->mov(rd->r32(), PPU_PS3_OFF_32(addrReg));
    a->bswap(rd->r32());
    a->movsxd(*rd, rd->r32());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LD(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();

    //	const u64 addr = (op.simm16 & ~3) + (op.ra ? ppu.gpr[op.ra] : 0);
    // ppu.gpr[op.rd] = vm::read64(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd, op.ra);
    s32 newImm = op.simm16 & ~3;
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
        if (newImm != 0) {
            a->mov(qr0->r64(), asmjit::imm(newImm));
            a->add(*addrReg, *qr0);
        }
    }
    else {
        a->mov(*addrReg, asmjit::imm(newImm));
    }
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*rd, PPU_PS3_OFF_64(addrReg));
    a->bswap(*rd);
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LDBRX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::_ref<le_t<u64>>(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*rd, PPU_PS3_OFF_64(addrReg));
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LDX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read64(vm::cast(addr, HERE));
    CellGprLockRegisters(op.rd);

    LoadAddrRbRa0(op);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    a->mov(*rd, PPU_PS3_OFF_64(addrReg));
    a->bswap(*rd);
    MakeCellGprDirty(op.rd);
}

void ppu_recompiler::LDU(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + (op.simm16 & ~3);
    //ppu.gpr[op.rd] = vm::read64(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rd, op.ra);
    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.simm16 != 0) {
        a->mov(*addrReg, asmjit::imm(op.simm16 & ~3).getInt32());
        a->add(*addrReg, *ra);
    }
    else {
        a->mov(*addrReg, *ra);
    }

    a->mov(*rd, PPU_PS3_OFF_64(addrReg));
    a->bswap(*rd);
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LDUX(ppu_opcode_t op) {
    GPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.gpr[op.rd] = vm::read64(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.rd, op.ra, op.rb);

    const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->mov(*addrReg, *ra);
    a->add(*addrReg, *rb);
    a->mov(ra->r32(), addrReg->r32());
    a->mov(*rd, PPU_PS3_OFF_64(addrReg));
    a->bswap(*rd);
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.rd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LDARX(ppu_opcode_t op) {
    GPR_LOAD
        // todo: res call
        InterpreterCall(op);
}

void ppu_recompiler::LMW(ppu_opcode_t op) {
    GPR_LOAD
        InterpreterCall(op);
}

void ppu_recompiler::LSWX(ppu_opcode_t op) {
    GPR_LOAD
        // bleh
        InterpreterCall(op);
}

void ppu_recompiler::LSWI(ppu_opcode_t op) {
    GPR_LOAD
        // bleh
        InterpreterCall(op);
}

void ppu_recompiler::STB(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //vm::write8(vm::cast(addr, HERE), (u8)ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
        if (op.simm16 != 0) {
            a->add(*addrReg, asmjit::imm(op.simm16).getInt32());
        }
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->mov(PPU_PS3_OFF_8(addrReg), rs->r8Lo());
}

void ppu_recompiler::STBX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::write8(vm::cast(addr, HERE), (u8)ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(PPU_PS3_OFF_8(addrReg), rs->r8Lo());
}

void ppu_recompiler::STBUX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::write8(vm::cast(addr, HERE), (u8)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, *rb);

    a->mov(PPU_PS3_OFF_8(addrReg), rs->r8Lo());
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STBU(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //vm::write8(vm::cast(addr, HERE), (u8)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(*addrReg, *ra);
    if (op.simm16 != 0) {
        a->add(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    a->mov(PPU_PS3_OFF_8(addrReg), rs->r8Lo());
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STH(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //vm::write16(vm::cast(addr, HERE), (u16)ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, ra->r32());
        if (op.simm16 != 0) {
            a->add(*addrReg, asmjit::imm(op.simm16));
        }
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16));
    }
    a->ror(rs->r16(), 8);
    a->mov(PPU_PS3_OFF_16(addrReg), rs->r16());
    a->ror(rs->r16(), 8);
}
void ppu_recompiler::STHU(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //vm::write16(vm::cast(addr, HERE), (u16)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(*addrReg, ra->r32());
    if (op.simm16 != 0) {
        a->add(*addrReg, asmjit::imm(op.simm16));
    }
    a->ror(rs->r16(), 8);
    a->mov(PPU_PS3_OFF_16(addrReg), rs->r16());
    a->ror(rs->r16(), 8);
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::STHX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::write16(vm::cast(addr, HERE), (u16)ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->ror(rs->r16(), 8);
    a->mov(PPU_PS3_OFF_16(addrReg), rs->r16());
    a->ror(rs->r16(), 8);
}
void ppu_recompiler::STHUX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::write16(vm::cast(addr, HERE), (u16)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->mov(*addrReg, ra->r32());
    a->add(*addrReg, rb->r32());

    a->ror(rs->r16(), 8);
    a->mov(PPU_PS3_OFF_16(addrReg), rs->r16());
    a->ror(rs->r16(), 8);
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}
void ppu_recompiler::STHBRX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::_ref<le_t<u16>>(vm::cast(addr, HERE)) = (u16)ppu.gpr[op.rs];

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(PPU_PS3_OFF_16(addrReg), rs->r16());
}

void ppu_recompiler::STW(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //vm::write32(vm::cast(addr, HERE), (u32)ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, ra->r32());
        if (op.simm16 != 0) {
            a->add(*addrReg, asmjit::imm(op.simm16));
        }
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16));
    }
    a->bswap(rs->r32());
    a->mov(PPU_PS3_OFF_32(addrReg), rs->r32());
    a->bswap(rs->r32());
}

void ppu_recompiler::STWU(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //vm::write32(vm::cast(addr, HERE), (u32)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, op.simm16);
    a->bswap(rs->r32());
    a->mov(PPU_PS3_OFF_32(addrReg), rs->r32());
    if (op.ra != op.rs) {
        a->bswap(rs->r32());
    }
    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);

}

void ppu_recompiler::STWUX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::write32(vm::cast(addr, HERE), (u32)ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, *rb);
    a->bswap(rs->r32());
    a->mov(PPU_PS3_OFF_32(addrReg), rs->r32());

    if (op.ra != op.rs)
        a->bswap(rs->r32());

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STWCX(ppu_opcode_t op) {
    GPR_STORE
        // todo: res call
        InterpreterCall(op);
}

void ppu_recompiler::STWX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::write32(vm::cast(addr, HERE), (u32)ppu.gpr[op.rs];

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->bswap(rs->r32());
    a->mov(PPU_PS3_OFF_32(addrReg), rs->r32());
    a->bswap(rs->r32());
}

void ppu_recompiler::STWBRX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::_ref<le_t<u32>>(vm::cast(addr, HERE)) = (u32)ppu.gpr[op.rs];

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(PPU_PS3_OFF_32(addrReg), rs->r32());
}

void ppu_recompiler::STD(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = (op.simm16 & ~3) + (op.ra ? ppu.gpr[op.ra] : 0);
    //vm::write64(vm::cast(addr, HERE), ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs, op.ra);
    a->mov(*addrReg, asmjit::imm(op.simm16 & ~3));
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->add(*addrReg, ra->r32());
    }
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->bswap(*rs);
    a->mov(PPU_PS3_OFF_64(addrReg), *rs);
    a->bswap(*rs);
}

void ppu_recompiler::STDU(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + (op.simm16 & ~3);
    //vm::write64(vm::cast(addr, HERE), ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->mov(*addrReg, *ra);
    a->add(*addrReg, asmjit::imm(op.simm16 & ~3));

    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->bswap(*rs);
    a->mov(PPU_PS3_OFF_64(addrReg), *rs);

    if (op.rs != op.ra) {
        a->bswap(*rs);
    }

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STDUX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::write64(vm::cast(addr, HERE), ppu.gpr[op.rs]);
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.rs, op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    a->mov(*addrReg, ra->r32());
    a->add(*addrReg, rb->r32());

    a->bswap(*rs);
    a->mov(PPU_PS3_OFF_64(addrReg), *rs);
    a->bswap(*rs);

    a->mov(*ra, *addrReg);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STDX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::write64(vm::cast(addr, HERE), ppu.gpr[op.rs]);

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);

    a->bswap(*rs);
    a->mov(PPU_PS3_OFF_64(addrReg), *rs);
    a->bswap(*rs);
}

void ppu_recompiler::STDCX(ppu_opcode_t op) {
    GPR_STORE
        // todo: res call
        InterpreterCall(op);
}

void ppu_recompiler::STDBRX(ppu_opcode_t op) {
    GPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::_ref<le_t<u64>>(vm::cast(addr, HERE)) = ppu.gpr[op.rs];

    CellGprLockRegisters(op.rs);
    LoadAddrRbRa0(op);
    const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
    a->mov(PPU_PS3_OFF_64(addrReg), *rs);
}

void ppu_recompiler::STSWI(ppu_opcode_t op) {
    GPR_STORE
        //todo: loop
        InterpreterCall(op);
}

void ppu_recompiler::STSWX(ppu_opcode_t op) {
    GPR_STORE
        //todo: loop
        InterpreterCall(op);
}

void ppu_recompiler::STMW(ppu_opcode_t op) {
    GPR_STORE
        //todo: loop
        InterpreterCall(op);
}

// -----------------
// Load/Store -- FPR
// -----------------

void ppu_recompiler::LFS(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.fpr[op.frd] = vm::_ref<f32>(vm::cast(addr, HERE));

    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
        a->add(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->movss(*frd, PPU_PS3_OFF_32(addrReg));
    XmmByteSwap32(*frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::LFSU(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //ppu.fpr[op.frd] = vm::_ref<f32>(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->add(*ra, asmjit::imm(op.simm16));

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->movss(*frd, PPU_PS3_OFF_32(ra));
    XmmByteSwap32(*frd);
    a->cvtss2sd(*frd, *frd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LFSX(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.fpr[op.frd] = vm::_ref<f32>(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->movss(*frd, PPU_PS3_OFF_32(addrReg));
    XmmByteSwap32(*frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::LFSUX(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.fpr[op.frd] = vm::_ref<f32>(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;

    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->add(*ra, *rb);
    a->movss(*frd, PPU_PS3_OFF_32(ra));

    XmmByteSwap32(*frd);
    a->cvtss2sd(*frd, *frd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LFD(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //ppu.fpr[op.frd] = vm::_ref<f64>(vm::cast(addr, HERE));

    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
        a->add(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->movsd(*frd, PPU_PS3_OFF_64(addrReg));
    XmmByteSwap64(*frd);
}

void ppu_recompiler::LFDU(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //ppu.fpr[op.frd] = vm::_ref<f64>(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;

    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->add(*ra, asmjit::imm(op.simm16).getInt32());

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->movsd(*frd, PPU_PS3_OFF_64(ra));
    XmmByteSwap64(*frd);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::LFDX(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.fpr[op.frd] = vm::_ref<f64>(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);

    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->movsd(*frd, PPU_PS3_OFF_64(addrReg));
    XmmByteSwap64(*frd);
}
void ppu_recompiler::LFDUX(ppu_opcode_t op) {
    FPR_LOAD
        CheckSetJumpTarget();

    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //ppu.fpr[op.frd] = vm::_ref<f64>(vm::cast(addr, HERE));
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    CellFprLockRegisters(op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->add(*ra, *rb);

    a->movsd(*frd, PPU_PS3_OFF_64(ra));
    XmmByteSwap64(*frd);
    MakeCellGprDirty(op.ra);
}

// todo: haven't looked into how much rounding will affect double -> single conversions
// single precision floats 
void ppu_recompiler::STFS(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //vm::_ref<f32>(vm::cast(addr, HERE)) = static_cast<float>(ppu.fpr[op.frs]);

    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->mov(*addrReg, *ra);
        a->add(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    else {
        a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    }
    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);

    a->xorps(*xr0, *xr0);
    a->cvtsd2ss(*xr0, *frs);

    // bswap vs load+phufb?.or xop?

    //XmmByteSwap32(*xr0);
    //a->movss(PPU_PS3_OFF_32(addrReg), *xr0);

    a->movd(qr0->r32(), *xr0);
    a->bswap(qr0->r32());
    a->mov(PPU_PS3_OFF_32(addrReg), qr0->r32());
}

void ppu_recompiler::STFSX(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();

    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::_ref<f32>(vm::cast(addr, HERE)) = static_cast<float>(ppu.fpr[op.frs]);

    LoadAddrRbRa0(op);

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->cvtsd2ss(*xr0, *frs);
    XmmByteSwap32(*xr0);
    a->movss(PPU_PS3_OFF_32(addrReg), *xr0);
}

void ppu_recompiler::STFSUX(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::_ref<f32>(vm::cast(addr, HERE)) = static_cast<float>(ppu.fpr[op.frs]);
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);

    a->add(*ra, *rb);

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->cvtsd2ss(*xr0, *frs);
    XmmByteSwap32(*xr0);
    a->movss(PPU_PS3_OFF_32(ra), *xr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STFSU(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //vm::_ref<f32>(vm::cast(addr, HERE)) = static_cast<float>(ppu.fpr[op.frs]);
    //ppu.gpr[op.ra] = addr;
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->add(*ra, asmjit::imm(op.simm16).getInt32());

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->cvtsd2ss(*xr0, *frs);
    XmmByteSwap32(*xr0);
    a->movss(PPU_PS3_OFF_32(ra), *xr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STFD(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + op.simm16 : op.simm16;
    //vm::_ref<f64>(vm::cast(addr, HERE)) = ppu.fpr[op.frs];

    a->mov(*addrReg, asmjit::imm(op.simm16).getInt32());
    if (op.ra != 0) {
        const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
        a->add(*addrReg, *ra);
    }

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->movsd(*xr0, *frs);
    XmmByteSwap64(*xr0);
    a->movsd(PPU_PS3_OFF_64(addrReg), *xr0);
}

void ppu_recompiler::STFDU(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + op.simm16;
    //vm::_ref<f64>(vm::cast(addr, HERE)) = ppu.fpr[op.frs];
    //ppu.gpr[op.ra] = addr;
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    a->add(*ra, asmjit::imm(op.simm16).getInt32());

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->movsd(*xr0, *frs);
    XmmByteSwap64(*xr0);
    a->movsd(PPU_PS3_OFF_64(ra), *xr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STFDX(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::_ref<f64>(vm::cast(addr, HERE)) = ppu.fpr[op.frs];

    LoadAddrRbRa0(op);

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->movsd(*xr0, *frs);
    XmmByteSwap64(*xr0);
    a->movsd(PPU_PS3_OFF_64(addrReg), *xr0);
}

void ppu_recompiler::STFDUX(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = ppu.gpr[op.ra] + ppu.gpr[op.rb];
    //vm::_ref<f64>(vm::cast(addr, HERE)) = ppu.fpr[op.frs];
    //ppu.gpr[op.ra] = addr;
    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);

    a->add(*ra, *rb);

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->movsd(*xr0, *frs);
    XmmByteSwap64(*xr0);
    a->movsd(PPU_PS3_OFF_64(ra), *xr0);
    MakeCellGprDirty(op.ra);
}

void ppu_recompiler::STFIWX(ppu_opcode_t op) {
    FPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //vm::write32(vm::cast(addr, HERE), (u32&)ppu.fpr[op.frs]);

    LoadAddrRbRa0(op);

    CellFprLockRegisters(op.frs);
    const asmjit::X86XmmReg* frs = GetLoadCellFpr(op.frs, true, false);
    a->xorps(*xr0, *xr0);
    a->movss(*xr0, *frs);
    XmmByteSwap32(*xr0);
    a->movss(PPU_PS3_OFF_32(addrReg), *xr0);
    //a->cvtsd2si(qr0->r32(), *frs);
    //a->bswap(qr0->r32());
    //a->mov(PPU_PS3_OFF_32(addrReg), qr0->r32());
}

// -----------------
// FPR - Move/sign manip
// -----------------

void ppu_recompiler::FMR(ppu_opcode_t op) {
    FPR_MOVESIGN
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.frb];
    //if (op.rc) ppu.SetCR(1, ppu.FG, ppu.FL, ppu.FE, ppu.FU);
    if (op.frd == op.frb) {
        a->nop();
    }
    else {
        CellFprLockRegisters(op.frb, op.frd);
        const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
        const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
        a->xorps(*frd, *frd);
        a->movsd(*frd, *frb);
    }
}

void ppu_recompiler::FNEG(ppu_opcode_t op) {
    FPR_MOVESIGN
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = -ppu.fpr[op.frb];
    //if (op.rc) ppu.SetCR(1, ppu.FG, ppu.FL, ppu.FE, ppu.FU);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    if (op.frd != op.frb) {
        const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
        a->xorps(*frd, *frd);
        a->movsd(*frd, *frb);
    }

    Negate64Bit(*frd);
}

void ppu_recompiler::FABS(ppu_opcode_t op) {
    FPR_MOVESIGN
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = fabs(ppu.fpr[op.frb]);
    //if (op.rc) ppu.SetCR(1, ppu.FG, ppu.FL, ppu.FE, ppu.FU);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    if (op.frd != op.frb) {
        const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
        a->xorps(*frd, *frd);
        a->movsd(*frd, *frb);
    }
    Abs64Bit(*frd);
}

void ppu_recompiler::FNABS(ppu_opcode_t op) {
    FPR_MOVESIGN
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    if (op.frd != op.frb) {
        const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
        a->xorps(*frd, *frd);
        a->movsd(*frd, *frb);
    }
    Abs64Bit(*frd);
    Negate64Bit(*frd);
}

// -----------------
// FPR - Arithmetic
// -----------------

// Compared to ps3autotests, almost all the floating points things are wrong 
// when dealing with infinity, -infinity or Nan's, so keep that in mind

// todo: verify rounding for single -> double conversions

// todo: support sse?

void ppu_recompiler::FADD(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.fra] + ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vaddsd(*frd, *fra, *frb);
}


void ppu_recompiler::FADDS(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] + ppu.fpr[op.frb]);

    CellFprLockRegisters(op.frb, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vaddsd(*frd, *fra, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FSUB(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.fra] - ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vsubsd(*frd, *fra, *frb);
}

void ppu_recompiler::FSUBS(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] - ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd, op.fra);

    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vsubsd(*frd, *fra, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FMUL(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.fra] * ppu.fpr[op.frc];

    CellFprLockRegisters(op.frc, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vmulsd(*frd, *fra, *frc);
}

void ppu_recompiler::FMULS(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] * ppu.fpr[op.frc]);
    CellFprLockRegisters(op.frc, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vmulsd(*frd, *fra, *frc);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

// todo: divide by zero? what happens?
void ppu_recompiler::FDIV(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.fra] / ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vdivsd(*frd, *fra, *frb);
}

void ppu_recompiler::FDIVS(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] / ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vdivsd(*frd, *fra, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FSQRTS(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(sqrt(ppu.fpr[op.frb]));
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->sqrtsd(*frd, *frb);
    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FSQRT(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = sqrt(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->sqrtsd(*frd, *frb);
}

void ppu_recompiler::FRSQRTE(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = 1.0 / sqrt(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->cvtsd2ss(*frd, *frb);
    a->rsqrtss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FRES(ppu_opcode_t op) {
    FPR_ALU
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(1.0 / ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->rcpss(*frd, *frb);
    a->cvtss2sd(*frd, *frd);
}

// -----------------
// FPR - Rounding / Conversions
// -----------------

void ppu_recompiler::FRSP(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->cvtsd2ss(*frd, *frb);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FCTIW(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //(s32&)ppu.fpr[op.frd] = lrint(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->cvtsd2si(*qr0, *frb);
    a->movq(*frd, *qr0);
}

void ppu_recompiler::FCTIWZ(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //(s32&)ppu.fpr[op.frd] = static_cast<s32>(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->roundsd(*frd, *frb, 0b00000011);
    a->cvtsd2si(*qr0, *frd);
    a->movq(*frd, *qr0);
}

void ppu_recompiler::FCTID(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //(s64&)ppu.fpr[op.frd] = llrint(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->cvtsd2si(*qr0, *frb);
    a->movq(*frd, *qr0);
}

void ppu_recompiler::FCTIDZ(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //(s64&)ppu.fpr[op.frd] = static_cast<s64>(ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->roundsd(*frd, *frb, 0b00000011);
    a->cvtsd2si(*qr0, *frb);
    a->movq(*frd, *qr0);
}

void ppu_recompiler::FCFID(ppu_opcode_t op) {
    FPR_ROUNDING
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = static_cast<double>((s64&)ppu.fpr[op.frb]);
    CellFprLockRegisters(op.frb, op.frd);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->movq(*qr0, *frb);
    a->cvtsi2sd(*frd, *qr0);
}

// -----------------
// FPR - fma type instructions
// -----------------

// todo: sse?
// cheating and using fma4 but the equivalent is commented in fma3

void ppu_recompiler::FMADD(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //ppu.fpr[op.frd] = ppu.fpr[op.fra] * ppu.fpr[op.frc] + ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfmaddsd(*frd, *fra, *frc, *frb);

    /*if (op.frd == op.fra) {
    a->vfmadd132sd(*frd, *frb, *frc);
    }
    else if (op.frd == op.frb) {
    a->vfmadd231sd(*frd, *fra, *frc);
    }
    else if (op.frd == op.frc) {
    a->vfmadd213sd(*frd, *fra, *frb);
    }
    else {
    a->movsd(*frd, *fra);
    a->vfmadd132sd(*frd, *frb, *frc);
    }*/
}

void ppu_recompiler::FMADDS(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] * ppu.fpr[op.frc] + ppu.fpr[op.frb]);

    // same as fmadd, but with a convert
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfmaddsd(*frd, *fra, *frc, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FMSUB(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //ppu.fpr[op.frd] = ppu.fpr[op.fra] * ppu.fpr[op.frc] - ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfmsubsd(*frd, *fra, *frc, *frb);
}

void ppu_recompiler::FMSUBS(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(ppu.fpr[op.fra] * ppu.fpr[op.frc] - ppu.fpr[op.frb]);

    // same as fmsub, but with a convert
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfmsubsd(*frd, *fra, *frc, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

// fnmadd is different than x86 fused-not muladd 
// so these are 'backwards' on purpose

void ppu_recompiler::FNMADD(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = -(ppu.fpr[op.fra] * ppu.fpr[op.frc]) - ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfnmsubsd(*frd, *fra, *frc, *frb);
}

void ppu_recompiler::FNMADDS(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(-(ppu.fpr[op.fra] * ppu.fpr[op.frc]) - ppu.fpr[op.frb]);

    // same as fnmadd, but with a convert
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfnmsubsd(*frd, *fra, *frc, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

void ppu_recompiler::FNMSUB(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();

    //ppu.fpr[op.frd] = -(ppu.fpr[op.fra] * ppu.fpr[op.frc]) + ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfnmaddsd(*frd, *fra, *frc, *frb);
}

void ppu_recompiler::FNMSUBS(ppu_opcode_t op) {
    FPR_FMA
        if (op.rc) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = f32(-(ppu.fpr[op.fra] * ppu.fpr[op.frc]) + ppu.fpr[op.frb]);

    // same as fnmsub, but with a convert
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);

    a->vfnmaddsd(*frd, *fra, *frc, *frb);

    a->cvtsd2ss(*frd, *frd);
    a->cvtss2sd(*frd, *frd);
}

// -----------------
// Comparisons - GPR
// -----------------

void ppu_recompiler::CMP(ppu_opcode_t op) {
    GPR_CMP
        CheckSetJumpTarget();
    /*if (op.l10)
    {
    ppu.SetCR<s64>(op.crfd, ppu.gpr[op.ra], ppu.gpr[op.rb]);
    }
    else
    {
    ppu.SetCR<s32>(op.crfd, u32(ppu.gpr[op.ra]), u32(ppu.gpr[op.rb]));
    }*/
    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.l10) {
        a->cmp(*ra, *rb);
    }
    else {
        a->cmp(ra->r32(), rb->r32());
    }
    SetCRFromCmp(op.crfd, true);
}

void ppu_recompiler::CMPL(ppu_opcode_t op) {
    GPR_CMP
        CheckSetJumpTarget();
    /*if (op.l10)
    {
    ppu.SetCR<u64>(op.crfd, ppu.gpr[op.ra], ppu.gpr[op.rb]);
    }
    else
    {
    ppu.SetCR<u32>(op.crfd, u32(ppu.gpr[op.ra]), u32(ppu.gpr[op.rb]));
    }*/
    CellGprLockRegisters(op.ra, op.rb);
    const asmjit::X86GpReg* rb = GetLoadCellGpr(op.rb, true);
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.l10) {
        a->cmp(*ra, *rb);
    }
    else {
        a->cmp(ra->r32(), rb->r32());
    }
    SetCRFromCmp(op.crfd, false);
}

void ppu_recompiler::CMPI(ppu_opcode_t op) {
    GPR_CMP
        CheckSetJumpTarget();
    /*if (op.l10)
    {
    ppu.SetCR<s64>(op.crfd, ppu.gpr[op.ra], op.simm16);
    }
    else
    {
    ppu.SetCR<s32>(op.crfd, u32(ppu.gpr[op.ra]), op.simm16);
    }*/
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.l10) {
        a->cmp(ra->r64(), asmjit::imm(op.simm16).getInt32());
    }
    else {
        a->cmp(ra->r32(), asmjit::imm(op.simm16).getInt32());
    }
    SetCRFromCmp(op.crfd, true);
}

void ppu_recompiler::CMPLI(ppu_opcode_t op) {
    GPR_CMP
        CheckSetJumpTarget();
    /*if (op.l10)
    {
    ppu.SetCR<u64>(op.crfd, ppu.gpr[op.ra], op.uimm16);
    }
    else
    {
    ppu.SetCR<u32>(op.crfd, u32(ppu.gpr[op.ra]), op.uimm16);
    }*/
    const asmjit::X86GpReg* ra = GetLoadCellGpr(op.ra, true);
    if (op.l10) {
        a->mov(qr0->r32(), asmjit::imm_u(op.uimm16).getUInt32());
        a->cmp(*ra, *qr0);
    }
    else {
        a->cmp(ra->r32(), asmjit::imm_u(op.uimm16).getUInt32());
    }
    SetCRFromCmp(op.crfd, false);
}

// -----------------
// Comparisons - FPR
// -----------------

// todo: use cmpss for branchless?
void ppu_recompiler::FCMPU(ppu_opcode_t op) {
    FPR_CMP
        CheckSetJumpTarget();
    //const f64 a = ppu.fpr[op.fra];
    //const f64 b = ppu.fpr[op.frb];
    //ppu.FG = a > b;
    //ppu.FL = a < b;
    //ppu.FE = a == b;
    //ppu.FU = a != a || b != b;
    //ppu.SetCR(op.crfd, ppu.FL, ppu.FG, ppu.FE, ppu.FU);
    CellFprLockRegisters(op.frb, op.fra);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    a->comisd(*fra, *frb);
    // float flags use 'unsigned' version
    SetCRFromCmp(op.crfd, false);
}

void ppu_recompiler::FCMPO(ppu_opcode_t op) {
    FPR_CMP
        FCMPU(op);
}

// todo: sse?
// also cheating and using XOP VPCMOV instr

void ppu_recompiler::FSEL(ppu_opcode_t op) {
    FPR_CMP
        if (op.rc) {
            //todo, flags
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //ppu.fpr[op.frd] = ppu.fpr[op.fra] >= 0.0 ? ppu.fpr[op.frc] : ppu.fpr[op.frb];
    CellFprLockRegisters(op.frb, op.frd, op.fra, op.frc);
    const asmjit::X86XmmReg* fra = GetLoadCellFpr(op.fra, true, false);
    const asmjit::X86XmmReg* frb = GetLoadCellFpr(op.frb, true, false);
    const asmjit::X86XmmReg* frc = GetLoadCellFpr(op.frc, true, false);
    const asmjit::X86XmmReg* frd = GetLoadCellFpr(op.frd, false, true);
    a->xorps(*xr0, *xr0);

    a->vcmpsd(*xr0, *fra, *xr0, asmjit::imm_u(5)); // not less than 0

    a->vpcmov(*frd, *frc, *frb, *xr0);

    // something like this should work for sse without avx
    /*c->xorps(*xw0, *xw0);
    c->cmpsd(*xw0, *fra, asmjit::imm_u(2));
    c->movq(*xw1, *xw0);
    // false check
    c->andnpd(*xw0, *frb);
    // true
    c->andpd(*xw1, *frc);
    // merge
    c->orpd(*xw0, *xw1);
    c->movsd(*frd, *xw0);
    */
}

// -----------------
// VPR - Load
// -----------------

void ppu_recompiler::LVEBX(ppu_opcode_t op) {
    VPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //ppu.vr[op.vd]._u8[15 - (addr & 0xf)] = vm::read8(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->and_(*addrReg, asmjit::imm(~0xfull).getInt32());
    a->vmovdqa(*vd, PPU_PS3_OFF_128(addrReg));
    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVEHX(ppu_opcode_t op) {
    VPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~1ULL;
    //ppu.vr[op.vd]._u16[7 - ((addr >> 1) & 0x7)] = vm::read16(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->and_(*addrReg, asmjit::imm(~0xfull).getInt32());
    a->vmovdqa(*vd, PPU_PS3_OFF_128(addrReg));
    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVEWX(ppu_opcode_t op) {
    VPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~3ULL;
    //ppu.vr[op.vd]._u32[3 - ((addr >> 2) & 0x3)] = vm::read32(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->and_(*addrReg, asmjit::imm(~0xfull).getInt32());
    a->vmovdqa(*vd, PPU_PS3_OFF_128(addrReg));
    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVX(ppu_opcode_t op) {
    VPR_LOAD
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~0xfull;
    //ppu.vr[op.vd] = vm::_ref<v128>(vm::cast(addr, HERE));

    LoadAddrRbRa0(op);
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->and_(*addrReg, asmjit::imm(~0xfull).getInt32());
    a->vmovdqa(*vd, PPU_PS3_OFF_128(addrReg));
    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVXL(ppu_opcode_t op) {
    VPR_LOAD
        LVX(op);
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~0xfull;
    //ppu.vr[op.vd] = vm::_ref<v128>(vm::cast(addr, HERE));
}

// -----------------
// VPR - Load Shifts
// -----------------

void ppu_recompiler::LVLX(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
        //const u32 eb = addr & 0xf;
        //ppu.vr[op.vd].clear();
        //for (u32 i = 0; i < 16u - eb; ++i) ppu.vr[op.vd]._u8[15 - i] = vm::read8(vm::cast(addr + i, HERE));

        CheckSetJumpTarget();

    LoadAddrRbRa0(op);
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->mov(*qr0, *addrReg);
    a->and_(*qr0, 0x0f);  // eb
                          // this is unaligned on purpose, theres a chance we get an unaligned address
    a->movups(*vd, PPU_PS3_OFF_128(addrReg));

    // just load the whole register in, and 'and' with mask to clear needed
    a->vpand(*vd, *vd, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmStvlxMask), *qr0));

    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVLXL(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        LVLX(op);
}

void ppu_recompiler::LVRX(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
        //const u8 eb = addr & 0xf;
        //
        //ppu.vr[op.vd].clear();
        //for (u32 i = 16 - eb; i < 16; ++i) ppu.vr[op.vd]._u8[15 - i] = vm::read8(vm::cast(addr + i - 16, HERE));
        CheckSetJumpTarget();

    LoadAddrRbRa0(op);
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->mov(*qr0, *addrReg);
    a->and_(qr0->r32(), 0x0f);  // eb
    a->sub(*addrReg, *qr0);
    a->movups(*vd, PPU_PS3_OFF_128(addrReg));

    // just load the whole register in, and 'and' with mask to clear needed
    a->vpand(*vd, *vd, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmStvrxMask), *qr0));

    XmmByteSwap128(*vd);
}

void ppu_recompiler::LVRXL(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        LVRX(op);
}

// 'rotate' shift

void ppu_recompiler::LVSL(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        CheckSetJumpTarget();

    LoadAddrRbRa0(op);
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->and_(addrReg->r32(), asmjit::imm_u(0x0f));  // eb
    a->vmovdqa(*vd, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmLvslShift), *addrReg));
}

void ppu_recompiler::LVSR(ppu_opcode_t op) {
    VPR_LOAD_SHIFT
        CheckSetJumpTarget();

    LoadAddrRbRa0(op);
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->and_(addrReg->r32(), asmjit::imm_u(0x0f));  // eb
    a->vmovdqa(*vd, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmLvsrShift), *addrReg));
}

// -----------------
// VPR - Store
// -----------------

void ppu_recompiler::STVEBX(ppu_opcode_t op) {
    VPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
    //const u8 eb = addr & 0xf;
    //vm::write8(vm::cast(addr, HERE), ppu.vr[op.vs]._u8[15 - eb]);
    CellVprLockRegisters(op.vs);

    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);

    LoadAddrRbRa0(op);

    a->mov(qr0->r32(), *addrReg); // qw0 is eb
    a->sub(qr0->r32(), 0xfffffff1);
    a->and_(qr0->r32(), asmjit::imm(0xf));

    // all 1's to zero register from shuffle, probly don't need this but it 
    // probly kills dependency, so im leaving it
    a->pcmpeqw(*xr0, *xr0);
    a->movd(*xr0, qr0->r32());

    // move byte we want from vs into low spot of xr0
    // todo: verify, do we need to bswap anything before this?
    a->vpshufb(*xr0, *vs, *xr0);

    a->pextrb(PPU_PS3_OFF_8(addrReg), *xr0, 0);
}

void ppu_recompiler::STVEHX(ppu_opcode_t op) {
    VPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~1ULL;
    //const u8 eb = (addr & 0xf) >> 1;
    //vm::write16(vm::cast(addr, HERE), ppu.vr[op.vs]._u16[7 - eb]);
    CellVprLockRegisters(op.vs);
    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);

    LoadAddrRbRa0(op);

    a->mov(qr0->r32(), *addrReg); // qw0 is eb
    a->and_(*addrReg, asmjit::imm(~1));
    // valid values are 0-14, we flip to get 15-1 for x86
    a->xor_(qr0->r32(), 0xf);
    a->and_(qr0->r32(), asmjit::imm(0xf));

    // minus from low, this should deal with the bswap for us as well
    a->mov(qr0->r8Hi(), qr0->r8Lo());
    a->sub(qr0->r8Lo(), 1);

    a->pcmpeqw(*xr0, *xr0);
    a->movd(*xr0, qr0->r32());

    a->vpshufb(*xr0, *vs, *xr0);
    a->pextrw(PPU_PS3_OFF_8(addrReg), *xr0, 0);
}

void ppu_recompiler::STVEWX(ppu_opcode_t op) {
    VPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~3ULL;
    //const u8 eb = (addr & 0xf) >> 2;
    //vm::write32(vm::cast(addr, HERE), ppu.vr[op.vs]._u32[3 - eb]);
    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vs);
    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);

    a->mov(qr0->r32(), *addrReg);
    a->and_(*addrReg, asmjit::imm(~3));

    // need to flip from low to high index
    // this *might* be wrong, i think this inst should support *any* byte number, but here we are just taking the flipped high bits, oops
    //a->xor_(qr0->r32(), 0x0C);
    //a->and_(qr0->r32(), 0x0C);
    // flip from 0-12 to 15-3, then to 12-0
    a->xor_(qr0->r32(), 0x0F);
    a->and_(qr0->r32(), 0x0F);
    a->sub(qr0->r32(), 0x3);

    // just going to generate mask in runtime,
    a->xorps(*xr0, *xr0);
    a->imul(qr0->r32(), 0x01010101);
    // this is 'flipped' to ignore bswap
    a->add(qr0->r32(), 0x00010203);
    a->movd(*xr0, qr0->r32());
    a->vpshufb(*xr0, *vs, *xr0);
    a->movd(PPU_PS3_OFF_32(addrReg), *xr0);
}

void ppu_recompiler::STVX(ppu_opcode_t op) {
    VPR_STORE
        CheckSetJumpTarget();
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~0xfull;
    //vm::_ref<v128>(vm::cast(addr, HERE)) = ppu.vr[op.vs];

    LoadAddrRbRa0(op);

    a->and_(*addrReg, asmjit::imm(~0xf));
    CellVprLockRegisters(op.vs);
    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);
    a->vmovdqa(*xr0, *vs);
    XmmByteSwap128(*xr0);
    a->vmovdqa(PPU_PS3_OFF_128(addrReg), *xr0);

}
void ppu_recompiler::STVXL(ppu_opcode_t op) {
    VPR_STORE
        STVX(op);
    //const u64 addr = (op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb]) & ~0xfull;
    //vm::_ref<v128>(vm::cast(addr, HERE)) = ppu.vr[op.vs];
}

void ppu_recompiler::STVLX(ppu_opcode_t op) {
    VPR_STORE
        /*const u64 addr = op.ra ? ppu.gpr[op.ra] + ppu.gpr[op.rb] : ppu.gpr[op.rb];
        const u32 eb = addr & 0xf;

        for (u32 i = 0; i < 16u - eb; ++i) vm::write8(vm::cast(addr + i, HERE), ppu.vr[op.vs]._u8[15 - i]);*/
        CheckSetJumpTarget();

    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vs);
    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);

    a->mov(*qr0, *addrReg);

    a->and_(qr0->r32(), 0xf);  // eb
    a->vmovdqa(*xr0, *vs);
    XmmByteSwap128(*xr0);
    // this ones pretty brutal, i can't find a better/similar solution/instruction than maskmovdqu 
    // also, there's probly a way to create this mask dynamicly, just get all 1's then clear number of low bytes of 'eb'
    a->vmovdqa(*xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmStvlxMask), *qr0));

    // with avx2, we don't need to do this store, but amd doesnt support avx2 yet, sooo
    // we have to 'store' rdi if we are (very likely) using it
    // todo: make a function to force store arbitrary regs
    if (cellGprs[1].regNum != -1) {
        if (cellGprs[1].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[1].regNum]), asmjit::x86::rdi);
        cellGprs[1].regNum = -1;
        cellGprs[1].isDirty = false;
        cellGprs[1].isLoaded = false;
    }
    a->mov(asmjit::x86::rdi, *baseReg);
    a->add(asmjit::x86::rdi, *addrReg);
    a->vmaskmovdqu(*xr0, *xr1);
}
void ppu_recompiler::STVLXL(ppu_opcode_t op) {
    VPR_STORE
        STVLX(op);
}

// this one is similar to stvlx but with different mask
void ppu_recompiler::STVRX(ppu_opcode_t op) {
    VPR_STORE
        CheckSetJumpTarget();

    LoadAddrRbRa0(op);

    CellVprLockRegisters(op.vs);
    const asmjit::X86XmmReg* vs = GetLoadCellVpr(op.vs, true, false);

    a->mov(*qr0, *addrReg);

    a->and_(qr0->r32(), 0xf);  // eb

                               // we have to minus eb from addr to offset
    a->sub(*addrReg, *qr0);

    a->vmovdqa(*xr0, *vs);
    XmmByteSwap128(*xr0);
    a->vmovdqa(*xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmStvrxMask), *qr0));

    if (cellGprs[1].regNum != -1) {
        if (cellGprs[1].isDirty)
            a->mov(PPU_OFF_64(gpr[cellGprs[1].regNum]), asmjit::x86::rdi);
        cellGprs[1].regNum = -1;
        cellGprs[1].isDirty = false;
        cellGprs[1].isLoaded = false;
    }
    a->mov(asmjit::x86::rdi, *baseReg);
    a->add(asmjit::x86::rdi, *addrReg);
    a->vmaskmovdqu(*xr0, *xr1);
}
void ppu_recompiler::STVRXL(ppu_opcode_t op) {
    VPR_STORE
        STVRX(op);
}

// -----------------
// VPR - Integer Add
// -----------------

// dont use xop
void ppu_recompiler::VADDCUW(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    //const auto a = ppu.vr[op.va].vi;
    //const auto b = ppu.vr[op.vb].vi;
    //ppu.vr[op.vd].vi = _mm_srli_epi32(_mm_cmpgt_epi32(_mm_xor_si128(b, _mm_set1_epi32(0x80000000)), _mm_xor_si128(a, _mm_set1_epi32(0x7fffffff))), 31);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // unsigned packed 32 bit int add, put carry bit in dest
    a->vpaddd(*xr0, *vb, *va); // sum = vb + va
    a->vpcomud(*xr0, *xr0, *vb, 0); // is sum less than vb?, if so all of xr0 will be 1's, else 0

                                    // ssse3 / avx, ppc wants either 1 or 0, not -1 or 0
    a->vpabsd(*vd, *xr0);
}

void ppu_recompiler::VADDSBS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddsb(*vd, *va, *vb);
}

void ppu_recompiler::VADDSHS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddsw(*vd, *va, *vb);
}

void ppu_recompiler::VADDSWS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vxorpd(*xr0, *va, *vb);
    a->movdqa(*xr1, *va);
    a->vpaddd(*vd, *va, *vb);

    a->vxorpd(*xr1, *xr1, *vd);
    a->vpandn(*xr0, *xr0, *xr1);
    a->vpsrad(*xr0, *xr0, 31);
    a->vxorpd(*xr1, *xr1, *vd);
    a->vpsrld(*xr1, *xr1, 31);
    a->vpsrld(*xr0, *xr0, 1);
    a->vpaddd(*xr1, *xr0, *xr1);
    a->vpslld(*xr0, *xr0, 1);
    //sse41
    a->vpblendvb(*vd, *vd, *xr1, *xr0);
}

void ppu_recompiler::VADDUBM(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddb(*vd, *va, *vb);
}

void ppu_recompiler::VADDUBS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddusb(*vd, *va, *vb);
}

void ppu_recompiler::VADDUHM(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = v128::add16(ppu.vr[op.va], ppu.vr[op.vb]);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddw(*vd, *va, *vb);
}

void ppu_recompiler::VADDUHS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_adds_epu16(ppu.vr[op.va].vi, ppu.vr[op.vb].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddusw(*vd, *va, *vb);
}

void ppu_recompiler::VADDUWM(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = v128::add32(ppu.vr[op.va], ppu.vr[op.vb]);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpaddd(*vd, *va, *vb);
}

void ppu_recompiler::VADDUWS(ppu_opcode_t op) {
    VPR_INT_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // overflow if a+b < (a | b)
    a->vpor(*xr0, *va, *vb);
    a->vpaddd(*vd, *vb, *va);

    // xop, cant use cmpgtd here, well, maybe, but we need to abs each one
    a->vpcomud(*xr0, *xr0, *vd, 2);
    a->vpor(*vd, *vd, *xr0);
}

// -----------------
// VPR - Integer Sub
// -----------------


// this uses xop and sse4
void ppu_recompiler::VSUBCUW(ppu_opcode_t op) {
    VPR_INT_SUB
        //auto& d = ppu.vr[op.vd];
        //const auto& a = ppu.vr[op.va];
        //const auto& b = ppu.vr[op.vb];

        //for (uint w = 0; w < 4; w++)
        //{
        //d._u32[w] = a._u32[w] < b._u32[w] ? 0 : 1;
        //}

        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpcomud(*vd, *va, *vb, 3); // bit backwards, cause we need inverse, if va is gte vb, then 1, else 0

                                  // ssse3 / avx ,ppc wants 1 and 0 not -1 and 0;
    a->vpabsd(*vd, *vd);
}

void ppu_recompiler::VSUBSBS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubsb(*vd, *va, *vb);
}

void ppu_recompiler::VSUBSHS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubsw(*vd, *va, *vb);
}

void ppu_recompiler::VSUBSWS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // this one turned out a bit big...bleh

    a->vxorpd(*xr0, *va, *vb);
    a->movdqa(*xr1, *va);
    a->vpsubd(*vd, *va, *vb);
    a->vxorpd(*xr1, *xr1, *vd);
    a->vpand(*xr0, *xr0, *xr1);
    a->vpsrad(*xr0, *xr0, 31);
    a->vxorpd(*xr1, *xr1, *vd);
    a->vpsrld(*xr1, *xr1, 31);
    a->vpsrld(*xr0, *xr0, 1);
    a->vpaddd(*xr1, *xr0, *xr1);
    a->vpslld(*xr0, *xr0, 1);
    //sse41
    a->vpblendvb(*vd, *vd, *xr1, *xr0);
}

void ppu_recompiler::VSUBUBM(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    //ppu.vr[op.vd] = v128::sub8(ppu.vr[op.va], ppu.vr[op.vb]);
    a->vpsubb(*vd, *va, *vb);
}

void ppu_recompiler::VSUBUBS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubusb(*vd, *va, *vb);
}

void ppu_recompiler::VSUBUHM(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubw(*vd, *va, *vb);
}

void ppu_recompiler::VSUBUHS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubusw(*vd, *va, *vb);
}

void ppu_recompiler::VSUBUWM(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsubd(*vd, *va, *vb);
}

void ppu_recompiler::VSUBUWS(ppu_opcode_t op) {
    VPR_INT_SUB
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vmovdqa(*xr0, *va);
    a->vpsubd(*vd, *va, *vb);

    // xop, need unsigned comparison
    a->vpcomud(*xr0, *vd, *xr0, 2); // is diff greater than a?

    a->vpandn(*vd, *xr0, *vd);
}

// -----------------
// VPR - Integer Mul
// -----------------

void ppu_recompiler::VMULESB(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_mullo_epi16(_mm_srai_epi16(ppu.vr[op.va].vi, 8), _mm_srai_epi16(ppu.vr[op.vb].vi, 8));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpsraw(*xr0, *va, 8);
    a->vpsraw(*xr1, *vb, 8);
    a->vpmullw(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULESH(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_madd_epi16(_mm_srli_epi32(ppu.vr[op.va].vi, 16), _mm_srli_epi32(ppu.vr[op.vb].vi, 16));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsrad(*xr0, *va, 16);
    a->vpsrad(*xr1, *vb, 16);
    a->vpmulld(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULEUB(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_mullo_epi16(_mm_srli_epi16(ppu.vr[op.va].vi, 8), _mm_srli_epi16(ppu.vr[op.vb].vi, 8));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsrlw(*xr0, *va, 8);
    a->vpsrlw(*xr1, *vb, 8);
    a->vpmullw(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULEUH(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    //const auto a = ppu.vr[op.va].vi;
    //const auto b = ppu.vr[op.vb].vi;
    //const auto ml = _mm_mullo_epi16(a, b);
    //const auto mh = _mm_mulhi_epu16(a, b);
    //ppu.vr[op.vd].vi = _mm_or_si128(_mm_srli_epi32(ml, 16), _mm_and_si128(mh, _mm_set1_epi32(0xffff0000)));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // high bits 
    a->vpsrld(*xr0, *va, 16);
    a->vpsrld(*xr1, *vb, 16);

    // multiple together
    a->vpmulld(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULOSB(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    // ppu.vr[op.vd].vi = _mm_mullo_epi16(_mm_srai_epi16(_mm_slli_epi16(ppu.vr[op.va].vi, 8), 8), _mm_srai_epi16(_mm_slli_epi16(ppu.vr[op.vb].vi, 8), 8));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpsllw(*xr0, *va, 8);
    a->vpsllw(*xr1, *vb, 8);

    a->vpsraw(*xr0, *xr0, 8);
    a->vpsraw(*xr1, *xr1, 8);
    a->vpmullw(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULOSH(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    //const auto mask = _mm_set1_epi32(0x0000ffff);
    //ppu.vr[op.vd].vi = _mm_madd_epi16(_mm_and_si128(ppu.vr[op.va].vi, mask), _mm_and_si128(ppu.vr[op.vb].vi, mask));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpslld(*xr0, *va, 16);
    a->vpslld(*xr1, *vb, 16);

    a->vpsrad(*xr0, *xr0, 16);
    a->vpsrad(*xr1, *xr1, 16);

    a->vpmulld(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMULOUB(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    // unsigned multiply low byte of each 16 bit (half)word
    // output 16 bit
    a->vpsllw(*xr0, *va, 8);
    a->vpsllw(*xr1, *vb, 8);
    a->vpsrlw(*xr0, *xr0, 8);
    a->vpsrlw(*xr1, *xr1, 8);
    a->vpmullw(*vd, *xr1, *xr0);
}

void ppu_recompiler::VMULOUH(ppu_opcode_t op) {
    VPR_INT_MUL
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    // unsigned multipy low 16 bits of each 32 bit
    a->vpslld(*xr0, *va, 16);
    a->vpslld(*xr1, *vb, 16);
    a->vpsrld(*xr0, *xr0, 16);
    a->vpsrld(*xr1, *xr1, 16);
    a->vpmulld(*vd, *xr0, *xr1);
}

// -----------------
// VPR - Integer Mul Add
// -----------------

// todo: dont use xop for these

void ppu_recompiler::VMHADDSHS(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    //const auto m = _mm_or_si128(_mm_srli_epi16(_mm_mullo_epi16(a, b), 15), _mm_slli_epi16(_mm_mulhi_epi16(a, b), 1));
    //const auto s = _mm_cmpeq_epi16(m, _mm_set1_epi16(-0x8000)); // detect special case (positive 0x8000)
    //ppu.vr[op.vd].vi = _mm_adds_epi16(_mm_adds_epi16(_mm_xor_si128(m, s), c), _mm_srli_epi16(s, 15));
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpmullw(*xr0, *va, *vb);
    a->vpmulhw(*xr1, *va, *vb);
    a->vpsllw(*xr1, *xr1, 1);
    a->vpsrlw(*xr0, *xr0, 15);

    a->vpor(*xr0, *xr0, *xr1);

    a->vpcmpeqw(*xr1, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[16])));

    a->vpxor(*xr0, *xr0, *xr1);
    a->vpaddsw(*xr0, *xr0, *vc);
    a->vpsrlw(*xr1, *xr1, 15);
    a->vpaddsw(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMHRADDSHS(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    // va * vb + 0x0000 4000 + vc
    // 16 bit to 16 bit
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->xorps(*xr0, *xr0);
    a->vpmulhrsw(*xr0, *va, *vb);
    a->vpaddsw(*vd, *xr0, *vc);
}

void ppu_recompiler::VMLADDUHM(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_add_epi16(_mm_mullo_epi16(ppu.vr[op.va].vi, ppu.vr[op.vb].vi), ppu.vr[op.vc].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // todo: need avx2 to do this easily with full 256 byte register

    // sse41

    a->vpmovzxwd(*xr0, *va); // low 16 bit
    a->vpmovzxwd(*xr1, *vb);

    a->vpmulld(*xr0, *xr0, *xr1); // low 32 bit mul result

    a->vpmovzxwd(*xr1, *vc);
    a->vpaddd(*xr0, *xr0, *xr1); // low 32 bit 

    a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[15]))); // low 16 bit packed low

                                                                                         // todo: could optimize this to not use spare if vd not equal to a b c
    const asmjit::X86XmmReg* spare = GetSpareCellVpr();

    // move high bits to low
    a->vpshufd(*spare, *va, 0x0e);
    a->vpshufd(*xr1, *vb, 0x0e);

    a->vpmovzxwd(*spare, *spare);
    a->vpmovzxwd(*xr1, *xr1);

    a->vpmulld(*xr1, *xr1, *spare); // high 32 bit mul result

    a->vpshufd(*spare, *vc, 0x0e);
    a->vpmovzxwd(*spare, *spare);
    a->vpaddd(*xr1, *xr1, *spare); // low 32 bit

    a->vpshufb(*xr1, *xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[14]))); // low 16 bit packed high

    a->vpor(*vd, *xr0, *xr1);
}

void ppu_recompiler::VMSUMMBM(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // need another spare, bleh
    const asmjit::X86XmmReg* spare = GetSpareCellVpr();

    // copying interp
    a->vpsraw(*xr0, *va, 8);
    a->vpsrlw(*xr1, *vb, 8);
    a->vpmaddwd(*spare, *xr0, *xr1); // sh

    a->vpsllw(*xr0, *va, 8);
    a->vpsraw(*xr0, *xr0, 8); // al

    a->vpsllw(*xr1, *vb, 8);
    a->vpsrlw(*xr1, *xr1, 8); // bl

    a->vpmaddwd(*xr0, *xr0, *xr1); // sl

    a->vpaddd(*vd, *spare, *vc);
    a->vpaddd(*vd, *vd, *xr0);
}

void ppu_recompiler::VMSUMSHM(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpmadcswd(*vd, *va, *vb, *vc);
}

void ppu_recompiler::VMSUMSHS(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpmadcsswd(*vd, *va, *vb, *vc);
}

void ppu_recompiler::VMSUMUBM(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    const asmjit::X86XmmReg* spare = GetSpareCellVpr();

    // copying interp
    a->vpsrlw(*xr0, *va, 8);
    a->vpsrlw(*xr1, *vb, 8);
    a->vpmaddwd(*spare, *xr0, *xr1); // sh

    a->vpsllw(*xr0, *va, 8);
    a->vpsrlw(*xr0, *xr0, 8); // al

    a->vpsllw(*xr1, *vb, 8);
    a->vpsrlw(*xr1, *xr1, 8); // bl

    a->vpmaddwd(*xr0, *xr0, *xr1); // sl

    a->vpaddd(*vd, *spare, *vc);
    a->vpaddd(*vd, *vd, *xr0);
}

void ppu_recompiler::VMSUMUHM(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    //const auto ml = _mm_mullo_epi16(a, b); // low results
    //const auto mh = _mm_mulhi_epu16(a, b); // high results
    //const auto ls = _mm_add_epi32(_mm_srli_epi32(ml, 16), _mm_and_si128(ml, _mm_set1_epi32(0x0000ffff)));
    //const auto hs = _mm_add_epi32(_mm_slli_epi32(mh, 16), _mm_and_si128(mh, _mm_set1_epi32(0xffff0000)));
    //ppu.vr[op.vd].vi = _mm_add_epi32(_mm_add_epi32(c, ls), hs);

    const asmjit::X86XmmReg* spare = GetSpareCellVpr();

    a->vpmullw(*xr0, *va, *vb);

    a->vpsrld(*xr1, *xr0, 16);
    a->vpslld(*xr0, *xr0, 16);
    a->vpsrld(*xr0, *xr0, 16);

    a->vpaddd(*spare, *xr0, *xr1);

    a->vpmulhuw(*xr0, *va, *vb);

    a->vpslld(*xr1, *xr0, 16);

    a->vpsrld(*xr0, *xr0, 16);
    a->vpslld(*xr0, *xr0, 16);

    a->vpaddd(*xr0, *xr0, *xr1);

    a->vpaddd(*vd, *vc, *spare);
    a->vpaddd(*vd, *vd, *xr0);
}

void ppu_recompiler::VMSUMUHS(ppu_opcode_t op) {
    VPR_INT_MUL_ADD
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    const asmjit::X86XmmReg* spare = GetSpareCellVpr();

    // downside is we have to check saturation after every addition -_-
    // theres probly a better way to do this....
    a->vpmullw(*xr0, *va, *vb);

    a->vpsrld(*xr1, *xr0, 16);
    a->vpslld(*xr0, *xr0, 16);
    a->vpsrld(*xr0, *xr0, 16);

    a->vpor(*spare, *xr0, *xr1); // sat check
    a->vpaddd(*xr0, *xr0, *xr1);
    a->vpcomud(*spare, *xr0, *spare, 0);
    a->vpor(*spare, *spare, *xr0);

    a->vpor(*xr1, *vc, *spare); // sat check
    a->vpaddd(*vd, *vc, *spare);
    a->vpcomud(*xr1, *vd, *xr1, 0);
    a->vpor(*vd, *vd, *xr1);

    a->vpmulhuw(*xr0, *va, *vb);

    a->vpslld(*xr1, *xr0, 16);

    a->vpsrld(*xr0, *xr0, 16);
    a->vpslld(*xr0, *xr0, 16);

    a->vpor(*spare, *xr0, *xr1); // sat check
    a->vpaddd(*xr0, *xr0, *xr1);
    a->vpcomud(*spare, *xr0, *spare, 0);
    a->vpor(*xr0, *xr0, *spare);

    a->vpor(*xr1, *vd, *xr0); // sat check
    a->vpaddd(*vd, *vd, *xr0);
    a->vpcomud(*xr1, *vd, *xr1, 0);
    a->vpor(*vd, *vd, *xr1);

}

// -----------------
// VPR - Integer Sum Across
// -----------------

// todo: these all suck currently, come back to them

void ppu_recompiler::VSUMSWS(ppu_opcode_t op) {
    VPR_INT_SUM_ACROSS
        InterpreterCall(op); return;
    CheckSetJumpTarget();
    //unpack 32 bit vars to 64 bit vars
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);
    a->vpmovsxdq(*xr0, *vb);
    a->vpmovsxdq(*xr1, *va);
    //a->pmulld
}

void ppu_recompiler::VSUM2SWS(ppu_opcode_t op) {
    InterpreterCall(op);
}

void ppu_recompiler::VSUM4SBS(ppu_opcode_t op) {
    InterpreterCall(op);
}

void ppu_recompiler::VSUM4SHS(ppu_opcode_t op) {
    InterpreterCall(op);
}

void ppu_recompiler::VSUM4UBS(ppu_opcode_t op) {
    InterpreterCall(op);
}

// -----------------
// VPR - Integer Average
// -----------------

// so close, except this wants signed, and x86 is unsigned

void ppu_recompiler::VAVGSB(ppu_opcode_t op) {
    VPR_INT_AVG
        InterpreterCall(op); return;
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    // add saturate bytes
    a->vpcmpeqb(*xr0, *xr0, *xr0);
    a->vpabsb(*xr1, *xr0);
    a->vpaddsb(*xr1, *xr1, *va); // add 1 to operand
    a->vpaddsb(*vd, *xr1, *vb); // add saturate
    a->vpshab(*vd, *vd, *xr0); // shift right 1 arithmic

}

void ppu_recompiler::VAVGSH(ppu_opcode_t op) {
    InterpreterCall(op);
}

// no 32 bit avg, boo
void ppu_recompiler::VAVGSW(ppu_opcode_t op) {
    InterpreterCall(op);
}

void ppu_recompiler::VAVGUB(ppu_opcode_t op) {
    VPR_INT_AVG
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpavgb(*vd, *va, *vb);
}

void ppu_recompiler::VAVGUH(ppu_opcode_t op) {
    VPR_INT_AVG
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpavgw(*vd, *va, *vb);
}
// no 32 bit avg, boo
void ppu_recompiler::VAVGUW(ppu_opcode_t op) {
    VPR_INT_AVG
        InterpreterCall(op); return;
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false);

    a->vpmovzxdq(*xr0, *va);
    a->vpmovzxdq(*xr1, *vb);
    //a->vpaddq(*vd, *xr0, *);
}

// -----------------
// VPR - Integer Max/Min
// -----------------

void ppu_recompiler::VMAXSB(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxsb(*vd, *va, *vb);
}

void ppu_recompiler::VMAXSH(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxsw(*vd, *va, *vb);
}

void ppu_recompiler::VMAXSW(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxsd(*vd, *va, *vb);
}

void ppu_recompiler::VMAXUB(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxub(*vd, *va, *vb);
}

void ppu_recompiler::VMAXUH(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxuw(*vd, *va, *vb);
}

void ppu_recompiler::VMAXUW(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpmaxud(*vd, *va, *vb);
}

void ppu_recompiler::VMINSB(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminsb(*vd, *va, *vb);
}

void ppu_recompiler::VMINSH(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminsw(*vd, *va, *vb);
}

void ppu_recompiler::VMINSW(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminsd(*vd, *va, *vb);
}

void ppu_recompiler::VMINUB(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminub(*vd, *va, *vb);
}

void ppu_recompiler::VMINUH(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminuw(*vd, *va, *vb);
}

void ppu_recompiler::VMINUW(ppu_opcode_t op) {
    VPR_INT_MAX_MIN
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpminud(*vd, *va, *vb);
}

// -----------------
// VPR - Integer Compare
// -----------------

void ppu_recompiler::VCMPEQUB(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpcmpeqb(*vd, *va, *vb);
    /*a->movdqa(*xr0, *va);
    a->pcmpeqb(*xr0, *vb);
    a->movdqa(*vd, *xr0);*/
}
void ppu_recompiler::VCMPEQUH(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpcmpeqw(*vd, *va, *vb);
    /*c->movdqa(*xw0, *va);
    c->pcmpeqw(*xw0, *vb);
    c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPEQUW(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_epi8((ppu.vr[op.vd] = v128::eq32(ppu.vr[op.va], ppu.vr[op.vb])).vi);
    //if (op.oe) ppu.SetCR(6, rmask == 0xffff, false, rmask == 0, false);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpcmpeqd(*vd, *va, *vb);
    /*c->movdqa(*xw0, *va);
    c->pcmpeqd(*xw0, *vb);
    c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTSB(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_epi8(ppu.vr[op.vd].vi = _mm_cmpgt_epi8(ppu.vr[op.va].vi, ppu.vr[op.vb].vi));
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomb(*vd, *va, *vb, asmjit::imm_u(2)); // gt

                                                /* untested sse
                                                c->movdqa(*xw0, *va);
                                                c->movdqa(*xw1, *va);

                                                c->pcmpgtb(*xw0, *vb);
                                                c->pcmpeqb(*xw1, *vb);
                                                c->por(*xw0, *xw1);

                                                c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTSH(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomw(*vd, *va, *vb, asmjit::imm_u(2)); //gt
                                                /* untested sse
                                                c->movdqa(*xw0, *va);
                                                c->movdqa(*xw1, *va);
                                                c->pcmpgtw(*xw0, *vb);
                                                c->pcmpeqw(*xw1, *vb);
                                                c->por(*xw0, *xw1);
                                                c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTSW(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomd(*vd, *va, *vb, asmjit::imm_u(2));
    /* untested sse
    c->movdqa(*xw0, *va);
    c->movdqa(*xw1, *va);
    c->pcmpgtd(*xw0, *vb);
    c->pcmpeqd(*xw1, *vb);
    c->por(*xw0, *xw1);
    c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTUB(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomub(*vd, *va, *vb, asmjit::imm_u(2));
    /*c->movdqa(*xw0, *va);
    c->pcmpgtb(*xw0, *vb);
    c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTUH(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomuw(*vd, *va, *vb, asmjit::imm_u(2));
    /*c->movdqa(*xw0, *va);
    c->pcmpgtw(*xw0, *vb);
    c->movdqa(*vd, *xw0);*/
}
void ppu_recompiler::VCMPGTUW(ppu_opcode_t op) {
    VPR_INT_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_epi8(ppu.vr[op.vd].vi = sse_cmpgt_epu32(ppu.vr[op.va].vi, ppu.vr[op.vb].vi));
    //if (op.oe) ppu.SetCR(6, rmask == 0xffff, false, rmask == 0, false);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpcomud(*vd, *va, *vb, asmjit::imm_u(2));
    /*c->movdqa(*xw0, *va);
    c->pcmpgtd(*xw0, *vb);
    c->movdqa(*vd, *xw0);*/
}

// -----------------
// VPR - Bitwise logical
// -----------------

void ppu_recompiler::VAND(ppu_opcode_t op) {
    VPR_BIT_LOGICAL
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpand(*vd, *va, *vb);
}
void ppu_recompiler::VANDC(ppu_opcode_t op) {
    VPR_BIT_LOGICAL
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpandn(*vd, *vb, *va);
}
void ppu_recompiler::VNOR(ppu_opcode_t op) {
    VPR_BIT_LOGICAL
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = ~(ppu.vr[op.va] | ppu.vr[op.vb]);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpor(*vd, *va, *vb);
    a->pcmpeqw(*xr0, *xr0); //all 1's
    a->vpxor(*vd, *vd, *xr0);
}
void ppu_recompiler::VOR(ppu_opcode_t op) {
    VPR_BIT_LOGICAL
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = ppu.vr[op.va] | ppu.vr[op.vb];
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpor(*vd, *va, *vb);
}
void ppu_recompiler::VXOR(ppu_opcode_t op) {
    VPR_BIT_LOGICAL
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = ppu.vr[op.va] ^ ppu.vr[op.vb];
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpxor(*vd, *va, *vb);
}

// -----------------
// VPR - Bitwise Rotate
// -----------------

void ppu_recompiler::VRLB(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // bottom 3 are all that matter, we also need this to stay positive for rotb to stay a left shift
    // xop
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[9])));
    a->vprotb(*vd, *va, *xr0);
}

void ppu_recompiler::VRLH(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[10])));
    a->vprotw(*vd, *va, *xr0);
}

void ppu_recompiler::VRLW(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[11])));
    a->vprotd(*vd, *va, *xr0);
}

void ppu_recompiler::VSL(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // k brutal, the idea is to extract the shift amount, then 
    //  exract the register into 2 64 bit gpr's, then take care of the shifting

    // first thing, we need low 3 bits of vb...technically we should *check* that all low bits are the same, but w/e
    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    // rcx holds our shift amount
    a->movd(asmjit::x86::rcx, *vb);
    a->and_(asmjit::x86::cl, 0x07);
    a->movq(*qr0, *va);

    // using 'addrReg' to hold mask, the idea is to get it all 1's, then shr to get 0's and 'not' to get mask
    a->mov(*addrReg, asmjit::imm(-1));
    a->shr(*addrReg, asmjit::x86::cl);
    a->not_(*addrReg);

    // need *another* scratch, take baseReg for now, and restore it at the end of register
    // this holds our 'shifted bits' to transfer over
    a->mov(*baseReg, *qr0);
    a->and_(*baseReg, *addrReg);

    // shift qr0 and store it back
    a->shl(*qr0, asmjit::x86::cl);
    // avoid clobbering the register if va and vd are the same
    if (op.va != op.vd)
        a->movq(*vd, *qr0);
    else
        a->pinsrq(*vd, *qr0, 0);

    // not the mask and rotate the bits we need 
    a->not_(*addrReg);
    a->rol(*addrReg, asmjit::x86::cl);
    a->rol(*baseReg, asmjit::x86::cl);

    a->pextrq(*qr0, *va, 1);
    a->shl(*qr0, asmjit::x86::cl);
    a->and_(*qr0, *addrReg);
    a->or_(*qr0, *baseReg);

    a->pinsrq(*vd, *qr0, 1);

    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

void ppu_recompiler::VSLB(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[9])));
    a->vpshlb(*vd, *va, *xr0);
}

void ppu_recompiler::VSLDOI(ppu_opcode_t op) {
    // luckily this one is an immediate
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // just need to adjust numbering to change from ppc to x86
    a->vpalignr(*vd, *va, *vb, (15 - op.vsh) + 1);
}

void ppu_recompiler::VSLH(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[10])));
    a->vpshlw(*vd, *va, *xr0);
}

void ppu_recompiler::VSLO(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // extract out the shift number and pshufb by that amount
    a->vmovd(*qr0, *vb);
    a->and_(qr0->r32(), 0x0f);
    a->vpshufb(*vd, *va, asmjit::host::oword_ptr_abs(asmjit::Ptr(xmmVsloMask), *qr0));
}

void ppu_recompiler::VSLW(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[11])));
    a->vpshld(*vd, *va, *xr0);
}

void ppu_recompiler::VSR(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    // k brutal, the idea is to extract the shift amount, then 
    //  exract the register into 2 64 bit gpr's, then take care of the shifting

    // first thing, we need low 3 bits of vb...technically we should *check* that all low bits are the same, but w/e
    // k we need 'rcx' as gpr shifts are based on CL register
    if (cellGprs[10].regNum != -1) {
        a->mov(PPU_OFF_64(gpr[cellGprs[10].regNum]), asmjit::x86::rcx);
        cellGprs[10].regNum = -1;
        cellGprs[10].isDirty = false;
        cellGprs[10].isLoaded = false;
    }

    // rcx holds our shift amount
    a->movd(asmjit::x86::rcx, *vb);
    a->and_(asmjit::x86::cl, 0x07);
    a->pextrq(*qr0, *va, 1); // am i missing an easier way to get high 64 bits of an xmm register?

                             // using 'addrReg' to hold mask, the idea is to get it all 1's, then shr to get 0's and 'not' to get mask
    a->mov(*addrReg, asmjit::imm(-1));
    a->shl(*addrReg, asmjit::x86::cl);
    a->not_(*addrReg);

    // brutal, need *another* scratch, take baseReg for now, and restore it at the end of register
    // this holds our 'shifted bits' to transfer over
    a->mov(*baseReg, *qr0);
    a->and_(*baseReg, *addrReg);

    // shift qr0 and store it back
    a->shr(*qr0, asmjit::x86::cl);
    a->pinsrq(*vd, *qr0, 1);

    // not the mask and rotate the bits we need 
    a->not_(*addrReg);
    a->ror(*addrReg, asmjit::x86::cl);
    a->ror(*baseReg, asmjit::x86::cl);

    a->movq(*qr0, *va);
    a->shr(*qr0, asmjit::x86::cl);
    a->and_(*qr0, *addrReg);
    a->or_(*qr0, *baseReg);

    // can't use movq as it will clear vd
    a->pinsrq(*vd, *qr0, 0);

    a->mov(*baseReg, asmjit::imm((u64)vm::ps3::_ptr<u32>(0)));
}

//ssse3 
void ppu_recompiler::VSRAB(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[9])));
    a->vpsignb(*xr0, *xr0, *xr1);

    a->vpshab(*vd, *va, *xr0);
}

void ppu_recompiler::VSRAH(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[10])));
    a->vpsignw(*xr0, *xr0, *xr1);

    a->vpshaw(*vd, *va, *xr0);
}

void ppu_recompiler::VSRAW(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[11])));
    a->vpsignd(*xr0, *xr0, *xr1);

    a->vpshad(*vd, *va, *xr0);
}

void ppu_recompiler::VSRB(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[9])));
    a->vpsignb(*xr0, *xr0, *xr1);

    a->vpshlb(*vd, *va, *xr0);
}

void ppu_recompiler::VSRH(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[10])));
    a->vpsignw(*xr0, *xr0, *xr1);

    a->vpshlw(*vd, *va, *xr0);
}

void ppu_recompiler::VSRW(ppu_opcode_t op) {
    VPR_BIT_ROTATE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // need to negate vb,
    a->pcmpeqw(*xr1, *xr1);
    a->vpand(*xr0, *vb, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[11])));
    a->vpsignd(*xr0, *xr0, *xr1);

    a->vpshld(*vd, *va, *xr0);
}

void ppu_recompiler::VSRO(ppu_opcode_t op) {
    InterpreterCall(op);
}

// -----------------
// VPR - Floating point arithmetic
// -----------------

void ppu_recompiler::VADDFP(ppu_opcode_t op) {
    VPR_FP_ALU
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = v128::addfs(ppu.vr[op.va], ppu.vr[op.vb]);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vaddps(*vd, *va, *vb);
}

void ppu_recompiler::VSUBFP(ppu_opcode_t op) {
    VPR_FP_ALU
        CheckSetJumpTarget();
    //ppu.vr[op.vd] = v128::subfs(ppu.vr[op.va], ppu.vr[op.vb]);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vsubps(*vd, *va, *vb);
}

void ppu_recompiler::VMADDFP(ppu_opcode_t op) {
    //VPR_FP_ALU
    CheckSetJumpTarget();
    //ppu.vr[op.vd].vf = _mm_add_ps(_mm_mul_ps(ppu.vr[op.va].vf, ppu.vr[op.vc].vf), ppu.vr[op.vb].vf);
    CellVprLockRegisters(op.va, op.vb, op.vc, op.vd);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vfmaddps(*vd, *va, *vc, *vb);

    //a->vmulps(*xr0, *va, *vc);
    //a->vaddps(*vd, *xr0, *vb);

    // nan check
    /*a->vmovaps(*xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmFloatConstData[3])));
    a->vpcmpeqd(*xr0, *vd, *xr0);
    a->vpslld(*xr0, *xr0, 31);
    a->vxorps(*vd, *vd, *xr0);*/
}

void ppu_recompiler::VNMSUBFP(ppu_opcode_t op) {
    VPR_FP_ALU
        CheckSetJumpTarget();
    //	ppu.vr[op.vd].vf = _mm_sub_ps(ppu.vr[op.vb].vf, _mm_mul_ps(ppu.vr[op.va].vf, ppu.vr[op.vc].vf));
    CellVprLockRegisters(op.va, op.vb, op.vc, op.vd);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    //a->vfmsubps(*vd, *va, *vc, *vb);

    a->vmulps(*xr0, *va, *vc);
    a->vsubps(*vd, *xr0, *vb);

    a->vmovaps(*xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[5])));
    a->vxorps(*vd, *vd, *xr1);

    /*if (op.va == op.vd) {
    a->vfnmadd132ps(*vd, *vb, *vc);
    }
    else if (op.vb == op.vd) {
    a->vfnmadd231ps(*vd, *va, *vc);
    }
    else if (op.vc == op.vd) {
    a->vfnmadd213ps(*vd, *va, *vb);
    }
    else {
    // all diff
    a->movaps(*vd, *va);
    a->vfnmadd132ps(*vd, *vb, *vc);
    }*/
}

void ppu_recompiler::VMAXFP(ppu_opcode_t op) {
    VPR_FP_ALU
        CheckSetJumpTarget();
    CellVprLockRegisters(op.va, op.vb, op.vd);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vcmpps(*xr0, *va, *vb, 3); // cmp unordered...to fix nan
    a->vmaxps(*vd, *va, *vb);
    a->vpor(*vd, *vd, *xr0);
}

void ppu_recompiler::VMINFP(ppu_opcode_t op) {
    VPR_FP_ALU
        CheckSetJumpTarget();
    CellVprLockRegisters(op.va, op.vb, op.vd);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vcmpps(*xr0, *va, *vb, 3); // cmp unordered...to fix nan
    a->vminps(*vd, *va, *vb);
    a->vpor(*vd, *vd, *xr0);
}

// -----------------
// VPR - Floating point conversions
// -----------------

void ppu_recompiler::VCTSXS(ppu_opcode_t op) {
    VPR_FP_CONVERT
        CheckSetJumpTarget();
    //const auto scaled = _mm_mul_ps(ppu.vr[op.vb].vf, g_ppu_scale_table[op.vuimm]);
    //ppu.vr[op.vd].vi = _mm_xor_si128(_mm_cvttps_epi32(scaled), _mm_castps_si128(_mm_cmpge_ps(scaled, _mm_set1_ps(0x80000000))));
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    float fuimm = static_cast<float>(std::exp2((float)op.vuimm));
    asmjit::Imm fimm;
    fimm.setFloat(fuimm);

    a->xorps(*xr0, *xr0);
    a->mov(*qr0, fimm);
    a->movd(*xr0, *qr0);
    a->vshufps(*xr0, *xr0, *xr0, 0);
    a->vmulps(*xr0, *xr0, *vb);

    a->vcvttps2dq(*vd, *xr0);

    // x86 gives us 0x8000 0000 on unrepresentable int, in both large and small
    // ppc gives 0x7fff ffff on too large, 0x8000 0000 on too small
    // so check if its too big, then let x86 set 0x8000, and xor with 1's to get 7fff
    a->vmovaps(*xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[17])));
    a->vcmpps(*xr1, *xr1, *xr0, 1); // cmpl...xr1 has max int representable by int

    a->vpxor(*vd, *vd, *xr1);
}

void ppu_recompiler::VCTUXS(ppu_opcode_t op) {
    VPR_FP_CONVERT
        InterpreterCall(op); return;
    CheckSetJumpTarget();
    //const auto scaled1 = _mm_max_ps(_mm_mul_ps(ppu.vr[op.vb].vf, g_ppu_scale_table[op.vuimm]), _mm_set1_ps(0.0f));
    //const auto scaled2 = _mm_and_ps(_mm_sub_ps(scaled1, _mm_set1_ps(0x80000000)), _mm_cmpge_ps(scaled1, _mm_set1_ps(0x80000000)));
    //ppu.vr[op.vd].vi = _mm_or_si128(_mm_or_si128(_mm_cvttps_epi32(scaled1), _mm_cvttps_epi32(scaled2)), _mm_castps_si128(_mm_cmpge_ps(scaled1, _mm_set1_ps(0x100000000))));

    /*int FloatToInt(float x)
    {
    unsigned e = (0x7F + 31) - ((*(unsigned*)&x & 0x7F800000) >> 23);
    unsigned m = 0x80000000 | (*(unsigned*)&x << 8);
    return int((m >> e) & -(e < 32));
    }*/

    FunctionCall(0);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    float fuimm = static_cast<float>(std::exp2((float)op.vuimm));
    asmjit::Imm fimm;
    fimm.setFloat(fuimm);

    a->xorps(*xr0, *xr0);
    a->mov(*qr0, fimm);
    a->movd(*xr0, *qr0);
    a->vshufps(*xr0, *xr0, *xr0, 0);
    a->vmulps(*vd, *xr0, *vb);
    // if neg just 0 the float to account for 0 sat
    a->vpsrad(*xr0, *vd, 31);
    a->vpandn(*vd, *xr0, *vd);
    a->vmovaps(*xr0, *vd); //xr0 and vd have our float

    a->vmovaps(*xr1, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmFloatConstData[0]))); // 0x9e
    a->vandps(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmFloatConstData[1]))); // 0x7f800000
    a->vpsrld(*xr0, *xr0, 23);
    a->vpsubd(*xr1, *xr1, *xr0); // xr1 has e

                                 // Saturate to 0
    a->vxorps(*xr0, *xr0, *xr0);
    a->vpcmpgtd(*xr0, *xr0, *xr1);
    a->vpandn(*xr1, *xr0, *xr1);

    a->vmovaps(*xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[5]))); // high bit set
    a->vpslld(*vd, *vd, 8);
    a->vpor(*vd, *vd, *xr0); // vd has m

                             // xop
    a->vpcmpeqw(*xr0, *xr0, *xr0);
    a->vpsignd(*xr1, *xr1, *xr0);
    a->vpshld(*vd, *vd, *xr1);

    a->vmovaps(*xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmFloatConstData[2]))); // 32
    a->vpcmpgtd(*xr0, *xr0, *xr1);

    a->vpand(*vd, *xr0, *vd);
}

void ppu_recompiler::VCFSX(ppu_opcode_t op) {
    VPR_FP_CONVERT
        //InterpreterCall(op); return;
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vf = _mm_mul_ps(_mm_cvtepi32_ps(ppu.vr[op.vb].vi), g_ppu_scale_table[0 - op.vuimm]);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);


    float fuimm = static_cast<float>(std::exp2((float)op.vuimm));
    asmjit::Imm fimm;
    fimm.setFloat(fuimm);

    a->vcvtdq2ps(*vd, *vb);
    a->xorps(*xr0, *xr0);

    a->xorps(*xr0, *xr0);
    a->mov(*qr0, fimm);
    a->movd(*xr0, *qr0);
    a->vshufps(*xr0, *xr0, *xr0, 0);

    a->vdivps(*vd, *vd, *xr0);
}

void ppu_recompiler::VCFUX(ppu_opcode_t op) {
    VPR_FP_CONVERT
        //InterpreterCall(op); return;
        CheckSetJumpTarget();
    //const auto b = ppu.vr[op.vb].vi;
    //const auto fix = _mm_and_ps(_mm_castsi128_ps(_mm_srai_epi32(b, 31)), _mm_set1_ps(0x80000000));
    //ppu.vr[op.vd].vf = _mm_mul_ps(_mm_add_ps(_mm_cvtepi32_ps(_mm_and_si128(b, _mm_set1_epi32(0x7fffffff))), fix), g_ppu_scale_table[0 - op.vuimm]);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    float fuimm = static_cast<float>(std::exp2((float)op.vuimm));
    asmjit::Imm fimm;
    fimm.setFloat(fuimm);

    a->vcvtdq2ps(*vd, *vb);
    a->xorps(*xr0, *xr0);

    a->xorps(*xr0, *xr0);
    a->mov(*qr0, fimm);
    a->movd(*xr0, *qr0);
    a->vshufps(*xr0, *xr0, *xr0, 0);

    a->vdivps(*vd, *vd, *xr0);
}

// -----------------
// VPR - Floating point rounding
// -----------------

void ppu_recompiler::VRFIM(ppu_opcode_t op) {
    VPR_FP_ROUND
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vroundps(*vd, *vb, 0x1 | 0x8); // to neg inf, no exception
}

void ppu_recompiler::VRFIN(ppu_opcode_t op) {
    VPR_FP_ROUND
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vroundps(*vd, *vb, 0x0 | 0x8); // to nearest , no exception
}

void ppu_recompiler::VRFIP(ppu_opcode_t op) {
    VPR_FP_ROUND
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vroundps(*vd, *vb, 0x2 | 0x8); // to pos inf, no exception
}

void ppu_recompiler::VRFIZ(ppu_opcode_t op) {
    VPR_FP_ROUND
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vroundps(*vd, *vb, 0x3 | 0x8); // to pos inf, no exception
}

// -----------------
// VPR - Floating point compare
// -----------------

void ppu_recompiler::VCMPBFP(ppu_opcode_t op) {
    VPR_FP_CMP
        if (op.rc) {
            InterpreterCall(op);
        }
    CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd, op.va);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vxorps(*xr0, *xr0, *xr0);
    a->vcmpps(*xr0, *va, *vb, 0x6); // 'not less than or equal'  
    a->vpabsd(*xr0, *xr0);

    const asmjit::X86XmmReg* spare;
    if (op.va == op.vd) {
        spare = GetSpareCellVpr();
    }

    a->movaps(*xr1, *vb);
    if (spare)
        a->vpslld(*spare, *xr0, 31);
    else
        a->vpslld(*vd, *xr0, 31);

    // negate b for this next one
    Negate32Bit(*xr1);
    a->vcmpps(*xr1, *xr1, *va, 0x6); // 'not less than or equal', flipped
    a->vpabsd(*xr1, *xr1);
    a->vpslld(*xr1, *xr1, 30);

    if (spare)
        a->vpor(*vd, *spare, *xr1);
    else
        a->vpor(*vd, *vd, *xr1);

}

void ppu_recompiler::VCMPEQFP(ppu_opcode_t op) {
    VPR_FP_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_ps(ppu.vr[op.vd].vf = _mm_cmpeq_ps(ppu.vr[op.va].vf, ppu.vr[op.vb].vf));
    //if (op.oe) ppu.SetCR(6, rmask == 0xf, false, rmask == 0, false);
    CellVprLockRegisters(op.vb, op.vd, op.va);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vcmpps(*vd, *va, *vb, 0);
}

void ppu_recompiler::VCMPGEFP(ppu_opcode_t op) {
    VPR_FP_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_ps(ppu.vr[op.vd].vf = _mm_cmpge_ps(ppu.vr[op.va].vf, ppu.vr[op.vb].vf));
    //if (op.oe) ppu.SetCR(6, rmask == 0xf, false, rmask == 0, false);
    CellVprLockRegisters(op.vb, op.vd, op.va);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vcmpps(*vd, *va, *vb, 5);// technically 'not less than' but w/e
}

void ppu_recompiler::VCMPGTFP(ppu_opcode_t op) {
    VPR_FP_CMP
        if (op.oe) {
            InterpreterCall(op);
            return;
        }
    CheckSetJumpTarget();
    //const auto rmask = _mm_movemask_ps(ppu.vr[op.vd].vf = _mm_cmpgt_ps(ppu.vr[op.va].vf, ppu.vr[op.vb].vf));
    //if (op.oe) ppu.SetCR(6, rmask == 0xf, false, rmask == 0, false);
    CellVprLockRegisters(op.vb, op.vd, op.va);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vcmpps(*vd, *va, *vb, 6); // technically 'not less than or equal' but w/e
}

// -----------------
// VPR - Floating point estimates
// -----------------

void ppu_recompiler::VEXPTEFP(ppu_opcode_t op) {
    VPR_FP_EST
        InterpreterCall(op);
}

void ppu_recompiler::VLOGEFP(ppu_opcode_t op) {
    VPR_FP_EST
        InterpreterCall(op);
}

void ppu_recompiler::VREFP(ppu_opcode_t op) {
    VPR_FP_EST
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vf = _mm_rcp_ps(ppu.vr[op.vb].vf);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->rcpps(*vd, *vb);
}

void ppu_recompiler::VRSQRTEFP(ppu_opcode_t op) {
    VPR_FP_EST
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vf = _mm_rsqrt_ps(ppu.vr[op.vb].vf);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->rsqrtps(*vd, *vb);
}

// -----------------
// VPR - Splats
// -----------------

void ppu_recompiler::VSPLTB(ppu_opcode_t op) {
    VPR_SPLAT
        CheckSetJumpTarget();
    /*auto& d = ppu.vr[op.vd];
    u8 byte = ppu.vr[op.vb]._u8[15 - op.vuimm];

    for (uint b = 0; b < 16; b++)
    {
    d._u8[b] = byte;
    }*/
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->xorps(*xr0, *xr0);
    // palignr gives us the byte in low
    a->vpalignr(*xr1, *vb, *vb, 15 - op.vuimm);
    // 'splat' low to all bytes
    a->vpshufb(*vd, *xr1, *xr0);
}

void ppu_recompiler::VSPLTH(ppu_opcode_t op) {
    VPR_SPLAT
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->pcmpeqw(*xr0, *xr0);
    // palignr gives us the 16 bit in low
    a->vpalignr(*xr1, *vb, *vb, 15 - ((op.vuimm * 2) + 1));
    a->pabsw(*xr0, *xr0); // create mask of 0x1010.....
    a->psllw(*xr0, 8);

    a->vpshufb(*vd, *xr1, *xr0);
}

void ppu_recompiler::VSPLTW(ppu_opcode_t op) {
    //VPR_SPLAT
    CheckSetJumpTarget();
    /*auto& d = ppu.vr[op.vd];

    u32 word = ppu.vr[op.vb]._u32[3 - op.vuimm];

    for (uint w = 0; w < 4; w++)
    {
    d._u32[w] = word;
    }*/
    u32 wordNum = op.vuimm & 0x3;
    u8 mask = 0;
    switch (wordNum) {
    case 2:
        mask = 0x55;
        break;
    case 1:
        mask = 0xaa;
        break;
    case 0:
        mask = 0xff;
        break;
    case 3:
    default:
        mask = 0;
        break;
    }
    CellVprLockRegisters(op.vb, op.vd);
    CellVprLockRegisters(op.vb, op.vd);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->vpshufd(*vd, *vb, mask);
}

void ppu_recompiler::VSPLTISB(ppu_opcode_t op) {
    VPR_SPLAT
        CheckSetJumpTarget();
    /*auto& d = ppu.vr[op.vd];
    const s8 imm = op.vsimm;

    for (uint b = 0; b < 16; b++)
    {
    d._u8[b] = imm;
    }*/
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    a->xor_(*qr0, *qr0);
    a->mov(qr0->r8(), op.vsimm);
    a->xorps(*xr1, *xr1);
    a->xorps(*xr0, *xr0);
    a->movd(*xr1, *qr0);
    a->vpshufb(*vd, *xr1, *xr0);
}

void ppu_recompiler::VSPLTISH(ppu_opcode_t op) {
    VPR_SPLAT
        CheckSetJumpTarget();
    /*auto& d = ppu.vr[op.vd];
    const s16 imm = op.vsimm;

    for (uint h = 0; h < 8; h++)
    {
    d._u16[h] = imm;
    }*/
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // 'splat' the 8bit mask to 64 bit, then unpack and sign extend to 16 bits
    s64 mask = (u8)(s8)op.vsimm * 0x0101010101010101;
    a->xorps(*xr0, *xr0);
    a->mov(*qr0, mask);
    a->movq(*xr0, *qr0);
    a->vpmovsxbw(*vd, *xr0);
}

void ppu_recompiler::VSPLTISW(ppu_opcode_t op) {
    VPR_SPLAT
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);

    // 'splat' the 8bit mask to 32 bit, then unpack and sign extend to 32 bits
    s32 mask = (u8)(s8)op.vsimm * 0x01010101;
    a->xorps(*xr0, *xr0);
    a->mov(qr0->r32(), mask);
    a->movd(*xr0, qr0->r32());
    a->vpmovsxbd(*vd, *xr0);
}

// -----------------
// VPR - Permutes
// -----------------

void ppu_recompiler::VPERM(ppu_opcode_t op) {
    VPR_PERM_SEL
        CheckSetJumpTarget();
    /*const auto index = _mm_andnot_si128(ppu.vr[op.vc].vi, _mm_set1_epi8(0x1f));
    const auto mask = _mm_cmpgt_epi8(index, _mm_set1_epi8(0xf));
    const auto sa = _mm_shuffle_epi8(ppu.vr[op.va].vi, index);
    const auto sb = _mm_shuffle_epi8(ppu.vr[op.vb].vi, index);
    ppu.vr[op.vd].vi = _mm_or_si128(_mm_and_si128(mask, sa), _mm_andnot_si128(mask, sb));*/
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpandn(*xr0, *vc, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[11])));
    a->vpperm(*vd, *vb, *va, *xr0);
}

void ppu_recompiler::VSEL(ppu_opcode_t op) {
    VPR_PERM_SEL
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb, op.vc);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vc = GetLoadCellVpr(op.vc, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpcmov(*vd, *vb, *va, *vc);
}

// -----------------
// VPR - Merges
// -----------------

void ppu_recompiler::VMRGHB(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_unpackhi_epi8(ppu.vr[op.vb].vi, ppu.vr[op.va].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpckhbw(*vd, *vb, *va);
}

void ppu_recompiler::VMRGHH(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_unpackhi_epi16(ppu.vr[op.vb].vi, ppu.vr[op.va].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpckhwd(*vd, *vb, *va);
}

void ppu_recompiler::VMRGHW(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpckhdq(*vd, *vb, *va);
}

void ppu_recompiler::VMRGLB(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpcklbw(*vd, *vb, *va);
}

void ppu_recompiler::VMRGLH(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpcklwd(*vd, *vb, *va);
}

void ppu_recompiler::VMRGLW(ppu_opcode_t op) {
    VPR_MERGE
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpunpckldq(*vd, *vb, *va);
}

// -----------------
// VPR - Pack
// -----------------

void ppu_recompiler::VPKPX(ppu_opcode_t op) {
    // yea no, come back to this later
    VPR_PACK
        InterpreterCall(op);
}

void ppu_recompiler::VPKSHSS(ppu_opcode_t op) {
    // cool beans, these are in  sse2/avx
    VPR_PACK
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_packs_epi16(ppu.vr[op.vb].vi, ppu.vr[op.va].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpacksswb(*vd, *vb, *va);
}

void ppu_recompiler::VPKSHUS(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    //ppu.vr[op.vd].vi = _mm_packus_epi16(ppu.vr[op.vb].vi, ppu.vr[op.va].vi);
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpackuswb(*vd, *vb, *va);
}

void ppu_recompiler::VPKSWSS(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpackssdw(*vd, *vb, *va);
}

void ppu_recompiler::VPKSWUS(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpackusdw(*vd, *vb, *va);
}

// cheating and using xop
void ppu_recompiler::VPKUHUM(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // just a plain pack basically, nothing fancy
    a->vpperm(*vd, *vb, *va, asmjit::host::ptr_abs(asmjit::Ptr(&xmmConstData[7])));
}

void ppu_recompiler::VPKUHUS(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // you may think packuswb is the more correct answer here....its not, thats signed, we need all unsigned
    // using xop... if high bits have anything, (greater than 0) saturate and store
    a->xorps(*xr1, *xr1);
    // checks to make sure we don't clobber registers
    if (op.va == op.vb) {
        // k, va and vb the same
        a->vpsrlw(*xr0, *va, 8);
        a->vpcomub(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusb(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[12]))); // low bytes packed high
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[13]))); // low bytes packed low
        a->vpor(*vd, *vd, *xr0);
    }
    else if (op.vd == op.va) {
        a->vpsrlw(*xr0, *va, 8);
        a->vpcomub(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusb(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[12]))); // low bytes packed high

        a->vpsrlw(*xr0, *vb, 8);
        a->vpcomub(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusb(*xr0, *xr0, *vb); // low bytes now 'saturated' if need be
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[13]))); // low bytes packed low

        a->vpor(*vd, *vd, *xr0);
    }
    else {
        a->vpsrlw(*xr0, *vb, 8);
        a->vpcomub(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusb(*xr0, *xr0, *vb); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[13]))); // low bytes packed low

        a->vpsrlw(*xr0, *va, 8);
        a->vpcomub(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusb(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[12]))); // low bytes packed high

        a->vpor(*vd, *vd, *xr0);
    }
}

void ppu_recompiler::VPKUWUM(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    a->vpperm(*vd, *vb, *va, asmjit::host::ptr_abs(asmjit::Ptr(&xmmConstData[8])));
}

void ppu_recompiler::VPKUWUS(ppu_opcode_t op) {
    VPR_PACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.va, op.vb);
    const asmjit::X86XmmReg* va = GetLoadCellVpr(op.va, true, false);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // you may thing vpackusdw works here, it doesnt really, as its signed saturate, we need unsigned saturate
    // using xop... if high bits have anything, (greater than 0) saturate and store
    a->xorps(*xr1, *xr1);
    // checks to make sure we don't clobber registers
    if (op.va == op.vb) {
        // k, va and vb the same
        a->vpsrld(*xr0, *va, 16);
        a->vpcomuw(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusw(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[14]))); // low 16 bits packed high
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[15]))); // low 16 bits packed low
        a->vpor(*vd, *vd, *xr0);
    }
    else if (op.vd == op.va) {
        a->vpsrld(*xr0, *va, 16);
        a->vpcomuw(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusw(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[14]))); // low 16 bits packed high

        a->vpsrld(*xr0, *vb, 8);
        a->vpcomuw(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusw(*xr0, *xr0, *vb); // low bytes now 'saturated' if need be
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[15]))); // low 16 bits packed low

        a->vpor(*vd, *vd, *xr0);
    }
    else {
        a->vpsrld(*xr0, *vb, 16);
        a->vpcomuw(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusw(*xr0, *xr0, *vb); // low bytes now 'saturated' if need be
        a->vpshufb(*vd, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[15]))); // low 16 bits packed low

        a->vpsrld(*xr0, *va, 16);
        a->vpcomuw(*xr0, *xr0, *xr1, 0x2); // are high bytes greater than 0?
        a->vpaddusw(*xr0, *xr0, *va); // low bytes now 'saturated' if need be
        a->vpshufb(*xr0, *xr0, asmjit::host::oword_ptr_abs(asmjit::Ptr(&xmmConstData[14]))); // low 16 bits packed high

        a->vpor(*vd, *vd, *xr0);
    }
}

// -----------------
// VPR - Unpack
// -----------------

void ppu_recompiler::VUPKHPX(ppu_opcode_t op) {
    VPR_UNPACK
        InterpreterCall(op);
}

void ppu_recompiler::VUPKHSB(ppu_opcode_t op) {
    VPR_UNPACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // move high bits down
    a->vpalignr(*vd, *vb, *vb, 8);
    // unpack low to high signed , sse41
    a->vpmovsxbw(*vd, *vd);
}

void ppu_recompiler::VUPKHSH(ppu_opcode_t op) {
    VPR_UNPACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // move high bits down
    a->vpalignr(*vd, *vb, *vb, 8);
    // unpack low to high signed , sse41
    a->vpmovsxwd(*vd, *vd);
}

void ppu_recompiler::VUPKLPX(ppu_opcode_t op) {
    VPR_UNPACK
        InterpreterCall(op);
}

void ppu_recompiler::VUPKLSB(ppu_opcode_t op) {
    VPR_UNPACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // unpack low to high signed , sse41
    a->vpmovsxbw(*vd, *vb);
}

void ppu_recompiler::VUPKLSH(ppu_opcode_t op) {
    VPR_UNPACK
        CheckSetJumpTarget();
    CellVprLockRegisters(op.vd, op.vb);
    const asmjit::X86XmmReg* vb = GetLoadCellVpr(op.vb, true, false);
    const asmjit::X86XmmReg* vd = GetLoadCellVpr(op.vd, false, true);
    // unpack low to high signed , sse41
    a->vpmovsxwd(*vd, *vb);
}

// -----------------
// Branching
// -----------------

// todo: only save full register stack if we are jumping outside the block?

void ppu_recompiler::B(ppu_opcode_t op) {
    GPR_BRANCH
        SaveRegisterState();
    CheckSetJumpTarget();
    // set lk if needed 
    if (op.lk) {
        a->mov(PPU_OFF_64(lr), asmjit::imm_u(m_pos + 4).getUInt64());
    }

    u32 newPC = (op.aa ? 0 : m_pos) + op.bt24;
    // check if we have a jump target for branch, aka, its in our block
    auto i = branchLabels.find(newPC);
    if (i != branchLabels.end()) {
        a->jmp(i->second);
    }
    else {
        // check if we can 'block link'
        ppu_jit_func_t func;
        if ((newPC >> 2) < MAX_FUNC_PTRS) {
            func = (ppu_jit_func_t)ppu->ppu_db->funcPointers[newPC >> 2];
        }
        if (func) {
            // hard link 
            a->mov(asmjit::host::rcx, *cpu);
            if (op.lk) {
                a->sub(asmjit::host::rsp, 0x28);
                a->call(asmjit::Ptr(func));
                a->add(asmjit::host::rsp, 0x28);
                a->cmp(addrReg->r32(), m_pos + 4);
                a->jne(*end);
            }
            else
                a->jmp(asmjit::Ptr(func));
        }
        else {
            // grab memory address of link 'to be' and check, if we have it, jump
            a->mov(asmjit::host::rax, asmjit::host::qword_ptr_abs((asmjit::Ptr)&ppu->ppu_db->funcPointers[newPC >> 2]));
            asmjit::Label jmpOver = a->newLabel();
            a->test(asmjit::host::rax, asmjit::host::rax);
            a->jnz(jmpOver);

            // if we dont have a block ptr , bail
            a->mov(addrReg->r32(), asmjit::imm_u(newPC).getUInt32());
            a->jmp(*end);

            a->bind(jmpOver);
            a->mov(asmjit::host::rcx, *cpu);

            if (op.lk) {
                a->sub(asmjit::host::rsp, 0x28);
                a->call(asmjit::host::rax);
                a->add(asmjit::host::rsp, 0x28);
                a->cmp(addrReg->r32(), m_pos + 4);
                a->jne(*end);
            }
            else
                a->jmp(asmjit::host::rax);
        }
    }
}

void ppu_recompiler::BC(ppu_opcode_t op) {
    GPR_BRANCH
        SaveRegisterState();
    CheckSetJumpTarget();

    const bool bo0 = (op.bo & 0x10) != 0;
    const bool bo1 = (op.bo & 0x08) != 0;
    const bool bo2 = (op.bo & 0x04) != 0;
    const bool bo3 = (op.bo & 0x02) != 0;

    const u32 newPC = (op.aa ? 0 : m_pos) + op.bt14;

    auto i = branchLabels.find(newPC);
    // block link check
    bool internalBranch = false;
    ppu_jit_func_t func;
    asmjit::Label* jumpTarget;
    asmjit::Label bEnd = a->newLabel();
    if (i != branchLabels.end()) {
        jumpTarget = &i->second;
        internalBranch = true;
    }
    else {
        jumpTarget = &bEnd;
        if ((newPC >> 2) < MAX_FUNC_PTRS) {
            func = (ppu_jit_func_t)ppu->ppu_db->funcPointers[newPC >> 2];
        }
    }

    if (op.lk) {
        a->mov(PPU_OFF_64(lr), asmjit::imm_u(m_pos + 4).getUInt64());
    }

    if (!bo2) {
        //decrement and test ctr
        a->mov(*qr0, PPU_OFF_64(ctr));
        a->sub(*qr0, 1);
        a->test(*qr0, *qr0);
        a->mov(PPU_OFF_64(ctr), *qr0);

        if (!bo3) {
            // branch if not zero
            if (internalBranch)
                a->jnz(*jumpTarget);
            else
                a->jz(*jumpTarget);
        }
        else {
            // branch if zero
            if (internalBranch)
                a->jz(*jumpTarget);
            else
                a->jnz(*jumpTarget);
        }
    }

    if (!bo0) {
        // test cr bit
        a->cmp(PPU_OFF_8(cr[op.bi]), asmjit::imm_u(bo1 ? 1 : 0).getUInt8());
        if (internalBranch)
            a->je(*jumpTarget);
        else
            a->jne(*jumpTarget);
    }
    if (!internalBranch) {
        if (func) {
            // hard link 
            a->mov(asmjit::host::rcx, *cpu);
            if (op.lk) {
                a->sub(asmjit::host::rsp, 0x28);
                a->call(asmjit::Ptr(func));
                a->add(asmjit::host::rsp, 0x28);
                a->cmp(*addrReg, m_pos + 4);
                a->jne(*end);
            }
            else
                a->jmp(asmjit::Ptr(func));

        }
        else {
            // grab memory address of link 'to be' and check, if we have it, jump
            a->mov(asmjit::host::rax, asmjit::host::qword_ptr_abs((asmjit::Ptr)&ppu->ppu_db->funcPointers[newPC >> 2]));
            asmjit::Label jmpOver = a->newLabel();
            a->test(asmjit::host::rax, asmjit::host::rax);
            a->jnz(jmpOver);

            // if we dont have a block ptr , bail
            a->mov(*addrReg, asmjit::imm_u(newPC).getUInt32());
            a->jmp(*end);

            a->bind(jmpOver);
            a->mov(asmjit::host::rcx, *cpu);
            if (op.lk) {
                a->sub(asmjit::host::rsp, 0x28);
                a->call(asmjit::host::rax);
                a->add(asmjit::host::rsp, 0x28);
                a->cmp(*addrReg, m_pos + 4);
                a->jne(*end);
            }
            else
                a->jmp(asmjit::host::rax);
        }
    }

    a->bind(bEnd);
}

void ppu_recompiler::BCLR(ppu_opcode_t op) {
    GPR_BRANCH
        CheckSetJumpTarget();

    const bool bo0 = (op.bo & 0x10) != 0;
    const bool bo1 = (op.bo & 0x08) != 0;
    const bool bo2 = (op.bo & 0x04) != 0;
    const bool bo3 = (op.bo & 0x02) != 0;

    /*ppu.CTR -= (bo2 ^ true);

    const bool ctr_ok = bo2 | ((ppu.CTR != 0) ^ bo3);
    const bool cond_ok = bo0 | (ppu.cr[op.bi] ^ (bo1 ^ true));

    if (ctr_ok && cond_ok)
    {
    const u32 nextLR = ppu.PC + 4;
    ppu.PC = ppu_branch_target(0, (u32)ppu.LR) - 4;
    if (op.lk) ppu.LR = nextLR;
    }*/

    if (op.opcode == ppu_instructions::implicts::BLR()) {
        // easy case
        SaveRegisterState(true);

        a->mov(asmjit::host::rax, PPU_OFF_64(lr));
        a->mov(addrReg->r32(), addrReg->r32());
        a->and_(addrReg->r32(), asmjit::imm(~0x3).getInt32());
        a->mov(asmjit::host::rcx, *cpu);
        a->ret();
        return;
    }

    if (op.lk) {
        a->mov(PPU_OFF_64(lr), asmjit::imm_u(m_pos + 4).getUInt64());
    }

    // qr0 holds ctr, with low 1 if true
    // trying out branchless here
    if (!bo2) {
        // dec ctr
        a->mov(*qr0, PPU_OFF_64(ctr));
        a->sub(*qr0, 1);
        a->test(*qr0, *qr0);
        a->mov(PPU_OFF_64(ctr), *qr0);
        a->test(*qr0, *qr0);
        if (!bo3) {
            // branch if not zero
            // so if not zero, set 1
            a->setnz(qr0->r8Lo());
        }
        else {
            // branch if zero
            a->setz(qr0->r8Lo());
        }
    }
    else {
        a->mov(qr0->r8Lo(), asmjit::imm_u(0x01));
    }

    // using addrReg, as we wont need it until we jump
    if (!bo0) {
        //a->cmp(PPU_OFF_8(cr[op.bi]), asmjit::imm_u(bo1 ? 1 : 0).getUInt8());
        a->xor_(*addrReg, *addrReg);
        a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.bi]));
        a->test(addrReg->r8Lo(), addrReg->r8Lo());
        if (bo1) {
            // branch if cr == 1
            a->setnz(addrReg->r8Lo());
        }
        else {
            a->setz(addrReg->r8Lo());
        }
    }
    else {
        a->mov(addrReg->r8Lo(), asmjit::imm_u(0x01));
    }

    // k now test qr0 and addrReg, and jump if both 1
    a->test(qr0->r8Lo(), addrReg->r8Lo());

    // jump over state saving things if not needed
    asmjit::Label skipRtn = a->newLabel();
    a->jz(skipRtn);

    SaveRegisterState(false);

    a->mov(asmjit::host::rax, PPU_OFF_64(lr));
    a->mov(addrReg->r32(), addrReg->r32());
    a->and_(addrReg->r32(), asmjit::imm(~0x3).getInt32());
    a->mov(asmjit::host::rcx, *cpu);
    a->ret();

    a->bind(skipRtn);
}

void ppu_recompiler::BCCTR(ppu_opcode_t op) {
    GPR_BRANCH
        CheckSetJumpTarget();

    /*if (op.bo & 0x10 || ppu.cr[op.bi] == ((op.bo & 0x8) != 0))
    {
    const u32 nextLR = ppu.PC + 4;
    ppu.PC = ppu_branch_target(0, (u32)ppu.CTR) - 4;
    if (op.lk) ppu.LR = nextLR;
    }*/

    if (op.lk) {
        a->mov(PPU_OFF_64(lr), asmjit::imm_u(m_pos + 4).getUInt64());
    }

    if (op.bo & 0x10) {
        // save state if rtn
        SaveRegisterState(true);

        a->mov(asmjit::host::rax, PPU_OFF_64(ctr));
        a->mov(addrReg->r32(), addrReg->r32());
        a->and_(addrReg->r32(), asmjit::imm(~0x3).getInt32());
        // just store cpu in rcx, will be used eventually
        a->mov(asmjit::host::rcx, *cpu);
        a->ret();
        return;
    }

    a->xor_(*qr0, *qr0);
    a->mov(qr0->r8Lo(), PPU_OFF_8(cr[op.bi]));
    a->test(qr0->r8Lo(), (u8)((op.bo & 0x8) != 0));
    asmjit::Label skipRtn = a->newLabel();
    // avoid long jump to end
    a->jne(skipRtn);

    // just dump dirty registers
    SaveRegisterState(false);

    a->mov(asmjit::host::rax, PPU_OFF_64(ctr));
    a->mov(addrReg->r32(), addrReg->r32());
    a->and_(addrReg->r32(), asmjit::imm(~0x3).getInt32());
    a->mov(asmjit::host::rcx, *cpu);
    a->ret();

    a->bind(skipRtn);
}

// -----------------
// CR Special instructions
// ----------------- 
void ppu_recompiler::CRNOR(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = (ppu.cr[op.crba] | ppu.cr[op.crbb]) ^ true;
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->or_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->xor_(addrReg->r8Lo(), 1);
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CRANDC(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = ppu.cr[op.crba] & (ppu.cr[op.crbb] ^ true);
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->xor_(addrReg->r8Lo(), 1);
    a->and_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CRXOR(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = ppu.cr[op.crba] ^ ppu.cr[op.crbb];
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->xor_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CRNAND(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = (ppu.cr[op.crba] & ppu.cr[op.crbb]) ^ true;
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->and_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->xor_(addrReg->r8Lo(), 1);
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CRAND(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = ppu.cr[op.crba] & ppu.cr[op.crbb];
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->and_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CREQV(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = (ppu.cr[op.crba] ^ ppu.cr[op.crbb]) ^ true;
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->xor_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->xor_(addrReg->r8Lo(), 1);
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CRORC(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = ppu.cr[op.crba] | (ppu.cr[op.crbb] ^ true);
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->xor_(addrReg->r8Lo(), 1);
    a->or_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::CROR(ppu_opcode_t op) {
    CR_SPECIAL
        CheckSetJumpTarget();
    //ppu.cr[op.crbd] = ppu.cr[op.crba] | ppu.cr[op.crbb];
    a->xor_(*addrReg, *addrReg);
    a->mov(addrReg->r8Lo(), PPU_OFF_8(cr[op.crba]));
    a->or_(addrReg->r8Lo(), PPU_OFF_8(cr[op.crbb]));
    a->mov(PPU_OFF_8(cr[op.crbd]), addrReg->r8Lo());
}

void ppu_recompiler::MCRF(ppu_opcode_t op) {
    CR_SPECIAL
        //todo: unsure
        InterpreterCall(op);
}
// -----------------
// Misc / System Registers
// ----------------- 

void ppu_recompiler::TDI(ppu_opcode_t op) {
    // leave traps for now
    InterpreterCall(op);
}
void ppu_recompiler::TD(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::TW(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::TWI(ppu_opcode_t op) {
    // let interp handle trap
    InterpreterCall(op);
}
void ppu_recompiler::DCBZ(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::DCBST(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DCBF(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DCBTST(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DSTST(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DCBT(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DCBI(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::ICBI(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::MFVSCR(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
    throw std::runtime_error("MFVSCR" HERE);
}
void ppu_recompiler::MTVSCR(ppu_opcode_t op) {
    CheckSetJumpTarget();
    LOG_WARNING(PPU, "MTVSCR");
    a->nop();
}
void ppu_recompiler::MTFSB1(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MCRFS(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MTFSB0(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MTFSFI(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MFFS(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MTFSF(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MFTB(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MTSPR(ppu_opcode_t op) {
    MF_SPECIAL
        //CheckSetJumpTarget();

        const u32 n = (op.spr >> 5) | ((op.spr & 0x1f) << 5);
    switch (n)
    {
    case 0x001:
    {
        /*const u64 value = ppu.gpr[op.rs];
        ppu.SO = (value & 0x80000000) != 0;
        ppu.OV = (value & 0x40000000) != 0;
        ppu.CA = (value & 0x20000000) != 0;
        ppu.XCNT = value & 0x7f;*/
        InterpreterCall(op);
        return;
    }
    case 0x008:
    {
        // i have no idea currently why we can't save this back, my assumption is theres a bad cast somewhere
        InterpreterCall(op); return;
        CheckSetJumpTarget();
        //ppu.LR = ppu.gpr[op.rs]; 
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(PPU_OFF_64(lr), *rs);
        return;
    }
    case 0x009:
    {
        CheckSetJumpTarget();
        //ppu.CTR = ppu.gpr[op.rs]; 
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(PPU_OFF_64(ctr), *rs);
        return;
    }
    case 0x100:
    {
        CheckSetJumpTarget();
        //ppu.VRSAVE = (u32)ppu.gpr[op.rs];
        const asmjit::X86GpReg* rs = GetLoadCellGpr(op.rs, true);
        a->mov(PPU_OFF_32(vrsave), rs->r32());
        return;
    }
    }
    fmt::throw_exception("MTSPR 0x%x" HERE, n);
}
void ppu_recompiler::MFSPR(ppu_opcode_t op) {
    MF_SPECIAL
        const u32 n = (op.spr >> 5) | ((op.spr & 0x1f) << 5);

    switch (n)
    {
    case 0x001:
        //ppu.gpr[op.rd] = u32{ ppu.SO } << 31 | ppu.OV << 30 | ppu.CA << 29 | ppu.XCNT; 
        InterpreterCall(op);
        return;
    case 0x008:
    {
        CheckSetJumpTarget();
        //ppu.gpr[op.rd] = ppu.LR;
        const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
        a->mov(*rd, PPU_OFF_64(lr));
        MakeCellGprDirty(op.rd);
        return;
    }
    case 0x009:
    {
        CheckSetJumpTarget();
        //ppu.gpr[op.rd] = ppu.CTR;
        const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
        a->mov(*rd, PPU_OFF_64(ctr));
        MakeCellGprDirty(op.rd);
        return;
    }
    case 0x100:
    {
        CheckSetJumpTarget();
        //ppu.gpr[op.rd] = ppu.VRSAVE; 
        const asmjit::X86GpReg* rd = GetLoadCellGpr(op.rd, false);
        a->mov(rd->r32(), PPU_OFF_32(vrsave));
        MakeCellGprDirty(op.rd);
        return;
    }
    case 0x10C:
        //ppu.gpr[op.rd] = get_timebased_time() & 0xffffffff; 
        InterpreterCall(op);
        return;
    case 0x10D:
        //ppu.gpr[op.rd] = get_timebased_time() >> 32; 
        InterpreterCall(op);
        return;
    }
}
void ppu_recompiler::MTOCRF(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::MFOCRF(ppu_opcode_t op) {
    InterpreterCall(op);
}
void ppu_recompiler::DSS(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::DST(ppu_opcode_t op) {
    CheckSetJumpTarget();
    a->nop();
}
void ppu_recompiler::SYNC(ppu_opcode_t op) {
    CheckSetJumpTarget();
    _mm_mfence();
}
void ppu_recompiler::ISYNC(ppu_opcode_t op) {
    CheckSetJumpTarget();
    _mm_mfence();
}
void ppu_recompiler::EIEIO(ppu_opcode_t op) {
    CheckSetJumpTarget();
    _mm_mfence();
}
void ppu_recompiler::ECIWX(ppu_opcode_t op) {
    CheckSetJumpTarget();
    throw std::runtime_error("ECIWX" HERE);
}
void ppu_recompiler::ECOWX(ppu_opcode_t op) {
    CheckSetJumpTarget();
    throw std::runtime_error("ECOWX" HERE);
}

void ppu_recompiler::UNK(ppu_opcode_t op)
{
    CheckSetJumpTarget();
    a->nop();
    //LOG_ERROR(PPU, "0x%05x: Unknown/Illegal opcode (0x%08x)", m_pos, op.opcode);
    //c->int3();
}
