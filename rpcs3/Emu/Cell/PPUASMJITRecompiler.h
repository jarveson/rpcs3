#pragma once

#include "PPURecompiler.h"
#include "PPUAnalyser.h"
#include "PPUOpcodes.h"

namespace asmjit
{
    struct JitRuntime;
    struct X86Compiler;
    struct X86Assembler;
    struct X86GpVar;
    struct X86GpReg;
    struct X86XmmReg;
    struct X86XmmVar;
    struct X86Mem;
    struct Label;
}

class gpr_link {
public:
    const asmjit::X86GpReg* reg;
    int regNum = -1;
    bool isLoaded = false;
    bool isDirty = false;
    gpr_link() {};
    gpr_link(const asmjit::X86GpReg& bindReg) :
        reg(&bindReg) {}
};

// both fpr and vpr share these
class xmm_link {
public:
    const asmjit::X86XmmReg* reg;
    int fprRegNum = -1;
    int vprRegNum = -1;
    bool isLoaded = false;
    bool isDirty = false;
    xmm_link() {};
    xmm_link(const asmjit::X86XmmReg& bindReg) :
        reg(&bindReg) {}
};

class ppu_recompiler : public ppu_recompiler_base
{
private:
    const std::shared_ptr<asmjit::JitRuntime> m_jit;
public:
    ppu_recompiler();

    virtual void compile(ppu_rec_function_t& f, ppu_thread& ppu) override;
    virtual void createFuncCaller(ppu_thread& ppu) override;

private:
    ppu_thread *ppu;

    static const u128 xmmConstData[18];
    static const u128 xmmFloatConstData[4];
    static const u128 xmmLvslShift[16];
    static const u128 xmmLvsrShift[16];
    static const u128 xmmStvlxMask[16];
    static const u128 xmmStvrxMask[16];
    static const u128 xmmVsloMask[16];

    // emitter:
    asmjit::X86Assembler* a;

    // input:
    const asmjit::X86GpReg* cpu;
    const asmjit::X86GpReg* baseReg;
    const asmjit::X86GpReg* addrReg;

    // temporary:
    const asmjit::X86GpReg* qr0;

    const asmjit::X86XmmReg* xr0;
    const asmjit::X86XmmReg* xr1;

    std::array<gpr_link, 11> cellGprs;
    u32 gprsUsed;
    std::array<xmm_link, 14> cellXmms;
    u32 fprsUsed;
    u32 vprsUsed;

    std::unordered_map<u32, asmjit::Label> branchLabels;
    std::set<u32> branchLabelsUsed;

    // labels:
    asmjit::Label* end; // function end (return *addr)
    asmjit::Label* jumpInternal;

    const asmjit::X86GpReg* GetLoadCellGpr(u32 regNum, bool load);
    void MakeCellGprDirty(u32 regNum);

    const asmjit::X86XmmReg* GetLoadCellFpr(u32 regNum, bool load, bool makeDirty = true);

    // if Load is true, it loads in value, makeDirty will mark it for store later
    const asmjit::X86XmmReg* GetLoadCellVpr(u32 regNum, bool load, bool makeDirty = true);
    const asmjit::X86XmmReg* GetSpareCellVpr();

    // Todo: use a template or something here, bein lazy here
    void CellGprLockRegisters(u32 reg1, u32 reg2 = 32, u32 reg3 = 32, u32 reg4 = 32);
    void CellFprLockRegisters(u32 reg1, u32 reg2 = 32, u32 reg3 = 32, u32 reg4 = 32);
    void CellVprLockRegisters(u32 reg1, u32 reg2 = 32, u32 reg3 = 32, u32 reg4 = 32);

    // If fullreset is false, this will only dump dirty registers to memory,
    //  it leaves all existing data about loaded regs and dirty regs
    //  Set false to save before conditional returns
    void SaveRegisterState(bool fullReset = true);
    void SaveUsedCellGprsToMem(bool fullReset = true);
    void SaveUsedCellXmmsToMem(bool fullReset = true);

    void CheckSetJumpTarget();
    void SetCRFromCmp(u32 field, bool is_signed);

    void XmmByteSwap32(const asmjit::X86XmmReg& reg);
    void XmmByteSwap64(const asmjit::X86XmmReg& reg);
    void XmmByteSwap128(const asmjit::X86XmmReg& reg);

    void Negate32Bit(const asmjit::X86XmmReg& reg);
    void Negate64Bit(const asmjit::X86XmmReg& reg);
    void Abs64Bit(const asmjit::X86XmmReg& reg);

    void MaskLow5BitsDWord(const asmjit::X86XmmReg& reg);

    void LoadAddrRbRa0(ppu_opcode_t op);

public:
    void InterpreterCall(ppu_opcode_t op);
    void FunctionCall(u32 newPC);

    void TDI(ppu_opcode_t op);
    void TWI(ppu_opcode_t op);
    void MFVSCR(ppu_opcode_t op);
    void MTVSCR(ppu_opcode_t op);
    void VADDCUW(ppu_opcode_t op);
    void VADDFP(ppu_opcode_t op);
    void VADDSBS(ppu_opcode_t op);
    void VADDSHS(ppu_opcode_t op);
    void VADDSWS(ppu_opcode_t op);
    void VADDUBM(ppu_opcode_t op);
    void VADDUBS(ppu_opcode_t op);
    void VADDUHM(ppu_opcode_t op);
    void VADDUHS(ppu_opcode_t op);
    void VADDUWM(ppu_opcode_t op);
    void VADDUWS(ppu_opcode_t op);
    void VAND(ppu_opcode_t op);
    void VANDC(ppu_opcode_t op);
    void VAVGSB(ppu_opcode_t op);
    void VAVGSH(ppu_opcode_t op);
    void VAVGSW(ppu_opcode_t op);
    void VAVGUB(ppu_opcode_t op);
    void VAVGUH(ppu_opcode_t op);
    void VAVGUW(ppu_opcode_t op);
    void VCFSX(ppu_opcode_t op);
    void VCFUX(ppu_opcode_t op);
    void VCMPBFP(ppu_opcode_t op);
    void VCMPEQFP(ppu_opcode_t op);
    void VCMPEQUB(ppu_opcode_t op);
    void VCMPEQUH(ppu_opcode_t op);
    void VCMPEQUW(ppu_opcode_t op);
    void VCMPGEFP(ppu_opcode_t op);
    void VCMPGTFP(ppu_opcode_t op);
    void VCMPGTSB(ppu_opcode_t op);
    void VCMPGTSH(ppu_opcode_t op);
    void VCMPGTSW(ppu_opcode_t op);
    void VCMPGTUB(ppu_opcode_t op);
    void VCMPGTUH(ppu_opcode_t op);
    void VCMPGTUW(ppu_opcode_t op);
    void VCTSXS(ppu_opcode_t op);
    void VCTUXS(ppu_opcode_t op);
    void VEXPTEFP(ppu_opcode_t op);
    void VLOGEFP(ppu_opcode_t op);
    void VMADDFP(ppu_opcode_t op);
    void VMAXFP(ppu_opcode_t op);
    void VMAXSB(ppu_opcode_t op);
    void VMAXSH(ppu_opcode_t op);
    void VMAXSW(ppu_opcode_t op);
    void VMAXUB(ppu_opcode_t op);
    void VMAXUH(ppu_opcode_t op);
    void VMAXUW(ppu_opcode_t op);
    void VMHADDSHS(ppu_opcode_t op);
    void VMHRADDSHS(ppu_opcode_t op);
    void VMINFP(ppu_opcode_t op);
    void VMINSB(ppu_opcode_t op);
    void VMINSH(ppu_opcode_t op);
    void VMINSW(ppu_opcode_t op);
    void VMINUB(ppu_opcode_t op);
    void VMINUH(ppu_opcode_t op);
    void VMINUW(ppu_opcode_t op);
    void VMLADDUHM(ppu_opcode_t op);
    void VMRGHB(ppu_opcode_t op);
    void VMRGHH(ppu_opcode_t op);
    void VMRGHW(ppu_opcode_t op);
    void VMRGLB(ppu_opcode_t op);
    void VMRGLH(ppu_opcode_t op);
    void VMRGLW(ppu_opcode_t op);
    void VMSUMMBM(ppu_opcode_t op);
    void VMSUMSHM(ppu_opcode_t op);
    void VMSUMSHS(ppu_opcode_t op);
    void VMSUMUBM(ppu_opcode_t op);
    void VMSUMUHM(ppu_opcode_t op);
    void VMSUMUHS(ppu_opcode_t op);
    void VMULESB(ppu_opcode_t op);
    void VMULESH(ppu_opcode_t op);
    void VMULEUB(ppu_opcode_t op);
    void VMULEUH(ppu_opcode_t op);
    void VMULOSB(ppu_opcode_t op);
    void VMULOSH(ppu_opcode_t op);
    void VMULOUB(ppu_opcode_t op);
    void VMULOUH(ppu_opcode_t op);
    void VNMSUBFP(ppu_opcode_t op);
    void VNOR(ppu_opcode_t op);
    void VOR(ppu_opcode_t op);
    void VPERM(ppu_opcode_t op);
    void VPKPX(ppu_opcode_t op);
    void VPKSHSS(ppu_opcode_t op);
    void VPKSHUS(ppu_opcode_t op);
    void VPKSWSS(ppu_opcode_t op);
    void VPKSWUS(ppu_opcode_t op);
    void VPKUHUM(ppu_opcode_t op);
    void VPKUHUS(ppu_opcode_t op);
    void VPKUWUM(ppu_opcode_t op);
    void VPKUWUS(ppu_opcode_t op);
    void VREFP(ppu_opcode_t op);
    void VRFIM(ppu_opcode_t op);
    void VRFIN(ppu_opcode_t op);
    void VRFIP(ppu_opcode_t op);
    void VRFIZ(ppu_opcode_t op);
    void VRLB(ppu_opcode_t op);
    void VRLH(ppu_opcode_t op);
    void VRLW(ppu_opcode_t op);
    void VRSQRTEFP(ppu_opcode_t op);
    void VSEL(ppu_opcode_t op);
    void VSL(ppu_opcode_t op);
    void VSLB(ppu_opcode_t op);
    void VSLDOI(ppu_opcode_t op);
    void VSLH(ppu_opcode_t op);
    void VSLO(ppu_opcode_t op);
    void VSLW(ppu_opcode_t op);
    void VSPLTB(ppu_opcode_t op);
    void VSPLTH(ppu_opcode_t op);
    void VSPLTISB(ppu_opcode_t op);
    void VSPLTISH(ppu_opcode_t op);
    void VSPLTISW(ppu_opcode_t op);
    void VSPLTW(ppu_opcode_t op);
    void VSR(ppu_opcode_t op);
    void VSRAB(ppu_opcode_t op);
    void VSRAH(ppu_opcode_t op);
    void VSRAW(ppu_opcode_t op);
    void VSRB(ppu_opcode_t op);
    void VSRH(ppu_opcode_t op);
    void VSRO(ppu_opcode_t op);
    void VSRW(ppu_opcode_t op);
    void VSUBCUW(ppu_opcode_t op);
    void VSUBFP(ppu_opcode_t op);
    void VSUBSBS(ppu_opcode_t op);
    void VSUBSHS(ppu_opcode_t op);
    void VSUBSWS(ppu_opcode_t op);
    void VSUBUBM(ppu_opcode_t op);
    void VSUBUBS(ppu_opcode_t op);
    void VSUBUHM(ppu_opcode_t op);
    void VSUBUHS(ppu_opcode_t op);
    void VSUBUWM(ppu_opcode_t op);
    void VSUBUWS(ppu_opcode_t op);
    void VSUMSWS(ppu_opcode_t op);
    void VSUM2SWS(ppu_opcode_t op);
    void VSUM4SBS(ppu_opcode_t op);
    void VSUM4SHS(ppu_opcode_t op);
    void VSUM4UBS(ppu_opcode_t op);
    void VUPKHPX(ppu_opcode_t op);
    void VUPKHSB(ppu_opcode_t op);
    void VUPKHSH(ppu_opcode_t op);
    void VUPKLPX(ppu_opcode_t op);
    void VUPKLSB(ppu_opcode_t op);
    void VUPKLSH(ppu_opcode_t op);
    void VXOR(ppu_opcode_t op);
    void MULLI(ppu_opcode_t op);
    void SUBFIC(ppu_opcode_t op);
    void CMPLI(ppu_opcode_t op);
    void CMPI(ppu_opcode_t op);
    void ADDIC(ppu_opcode_t op);
    void ADDI(ppu_opcode_t op);
    void ADDIS(ppu_opcode_t op);
    void BC(ppu_opcode_t op);
    void HACK(ppu_opcode_t op);
    void SC(ppu_opcode_t op);
    void B(ppu_opcode_t op);
    void MCRF(ppu_opcode_t op);
    void BCLR(ppu_opcode_t op);
    void CRNOR(ppu_opcode_t op);
    void CRANDC(ppu_opcode_t op);
    void ISYNC(ppu_opcode_t op);
    void CRXOR(ppu_opcode_t op);
    void CRNAND(ppu_opcode_t op);
    void CRAND(ppu_opcode_t op);
    void CREQV(ppu_opcode_t op);
    void CRORC(ppu_opcode_t op);
    void CROR(ppu_opcode_t op);
    void BCCTR(ppu_opcode_t op);
    void RLWIMI(ppu_opcode_t op);
    void RLWINM(ppu_opcode_t op);
    void RLWNM(ppu_opcode_t op);
    void ORI(ppu_opcode_t op);
    void ORIS(ppu_opcode_t op);
    void XORI(ppu_opcode_t op);
    void XORIS(ppu_opcode_t op);
    void ANDI(ppu_opcode_t op);
    void ANDIS(ppu_opcode_t op);
    void RLDICL(ppu_opcode_t op);
    void RLDICR(ppu_opcode_t op);
    void RLDIC(ppu_opcode_t op);
    void RLDIMI(ppu_opcode_t op);
    void RLDCL(ppu_opcode_t op);
    void RLDCR(ppu_opcode_t op);
    void CMP(ppu_opcode_t op);
    void TW(ppu_opcode_t op);
    void LVSL(ppu_opcode_t op);
    void LVEBX(ppu_opcode_t op);
    void SUBFC(ppu_opcode_t op);
    void MULHDU(ppu_opcode_t op);
    void ADDC(ppu_opcode_t op);
    void MULHWU(ppu_opcode_t op);
    void MFOCRF(ppu_opcode_t op);
    void LWARX(ppu_opcode_t op);
    void LDX(ppu_opcode_t op);
    void LWZX(ppu_opcode_t op);
    void SLW(ppu_opcode_t op);
    void CNTLZW(ppu_opcode_t op);
    void SLD(ppu_opcode_t op);
    void AND(ppu_opcode_t op);
    void CMPL(ppu_opcode_t op);
    void LVSR(ppu_opcode_t op);
    void LVEHX(ppu_opcode_t op);
    void SUBF(ppu_opcode_t op);
    void LDUX(ppu_opcode_t op);
    void DCBST(ppu_opcode_t op);
    void LWZUX(ppu_opcode_t op);
    void CNTLZD(ppu_opcode_t op);
    void ANDC(ppu_opcode_t op);
    void TD(ppu_opcode_t op);
    void LVEWX(ppu_opcode_t op);
    void MULHD(ppu_opcode_t op);
    void MULHW(ppu_opcode_t op);
    void LDARX(ppu_opcode_t op);
    void DCBF(ppu_opcode_t op);
    void LBZX(ppu_opcode_t op);
    void LVX(ppu_opcode_t op);
    void NEG(ppu_opcode_t op);
    void LBZUX(ppu_opcode_t op);
    void NOR(ppu_opcode_t op);
    void STVEBX(ppu_opcode_t op);
    void SUBFE(ppu_opcode_t op);
    void ADDE(ppu_opcode_t op);
    void MTOCRF(ppu_opcode_t op);
    void STDX(ppu_opcode_t op);
    void STWCX(ppu_opcode_t op);
    void STWX(ppu_opcode_t op);
    void STVEHX(ppu_opcode_t op);
    void STDUX(ppu_opcode_t op);
    void STWUX(ppu_opcode_t op);
    void STVEWX(ppu_opcode_t op);
    void SUBFZE(ppu_opcode_t op);
    void ADDZE(ppu_opcode_t op);
    void STDCX(ppu_opcode_t op);
    void STBX(ppu_opcode_t op);
    void STVX(ppu_opcode_t op);
    void MULLD(ppu_opcode_t op);
    void SUBFME(ppu_opcode_t op);
    void ADDME(ppu_opcode_t op);
    void MULLW(ppu_opcode_t op);
    void DCBTST(ppu_opcode_t op);
    void STBUX(ppu_opcode_t op);
    void ADD(ppu_opcode_t op);
    void DCBT(ppu_opcode_t op);
    void LHZX(ppu_opcode_t op);
    void EQV(ppu_opcode_t op);
    void ECIWX(ppu_opcode_t op);
    void LHZUX(ppu_opcode_t op);
    void XOR(ppu_opcode_t op);
    void MFSPR(ppu_opcode_t op);
    void LWAX(ppu_opcode_t op);
    void DST(ppu_opcode_t op);
    void LHAX(ppu_opcode_t op);
    void LVXL(ppu_opcode_t op);
    void MFTB(ppu_opcode_t op);
    void LWAUX(ppu_opcode_t op);
    void DSTST(ppu_opcode_t op);
    void LHAUX(ppu_opcode_t op);
    void STHX(ppu_opcode_t op);
    void ORC(ppu_opcode_t op);
    void ECOWX(ppu_opcode_t op);
    void STHUX(ppu_opcode_t op);
    void OR(ppu_opcode_t op);
    void DIVDU(ppu_opcode_t op);
    void DIVWU(ppu_opcode_t op);
    void MTSPR(ppu_opcode_t op);
    void DCBI(ppu_opcode_t op);
    void NAND(ppu_opcode_t op);
    void STVXL(ppu_opcode_t op);
    void DIVD(ppu_opcode_t op);
    void DIVW(ppu_opcode_t op);
    void LVLX(ppu_opcode_t op);
    void LDBRX(ppu_opcode_t op);
    void LSWX(ppu_opcode_t op);
    void LWBRX(ppu_opcode_t op);
    void LFSX(ppu_opcode_t op);
    void SRW(ppu_opcode_t op);
    void SRD(ppu_opcode_t op);
    void LVRX(ppu_opcode_t op);
    void LSWI(ppu_opcode_t op);
    void LFSUX(ppu_opcode_t op);
    void SYNC(ppu_opcode_t op);
    void LFDX(ppu_opcode_t op);
    void LFDUX(ppu_opcode_t op);
    void STVLX(ppu_opcode_t op);
    void STDBRX(ppu_opcode_t op);
    void STSWX(ppu_opcode_t op);
    void STWBRX(ppu_opcode_t op);
    void STFSX(ppu_opcode_t op);
    void STVRX(ppu_opcode_t op);
    void STFSUX(ppu_opcode_t op);
    void STSWI(ppu_opcode_t op);
    void STFDX(ppu_opcode_t op);
    void STFDUX(ppu_opcode_t op);
    void LVLXL(ppu_opcode_t op);
    void LHBRX(ppu_opcode_t op);
    void SRAW(ppu_opcode_t op);
    void SRAD(ppu_opcode_t op);
    void LVRXL(ppu_opcode_t op);
    void DSS(ppu_opcode_t op);
    void SRAWI(ppu_opcode_t op);
    void SRADI(ppu_opcode_t op);
    void EIEIO(ppu_opcode_t op);
    void STVLXL(ppu_opcode_t op);
    void STHBRX(ppu_opcode_t op);
    void EXTSH(ppu_opcode_t op);
    void STVRXL(ppu_opcode_t op);
    void EXTSB(ppu_opcode_t op);
    void STFIWX(ppu_opcode_t op);
    void EXTSW(ppu_opcode_t op);
    void ICBI(ppu_opcode_t op);
    void DCBZ(ppu_opcode_t op);
    void LWZ(ppu_opcode_t op);
    void LWZU(ppu_opcode_t op);
    void LBZ(ppu_opcode_t op);
    void LBZU(ppu_opcode_t op);
    void STW(ppu_opcode_t op);
    void STWU(ppu_opcode_t op);
    void STB(ppu_opcode_t op);
    void STBU(ppu_opcode_t op);
    void LHZ(ppu_opcode_t op);
    void LHZU(ppu_opcode_t op);
    void LHA(ppu_opcode_t op);
    void LHAU(ppu_opcode_t op);
    void STH(ppu_opcode_t op);
    void STHU(ppu_opcode_t op);
    void LMW(ppu_opcode_t op);
    void STMW(ppu_opcode_t op);
    void LFS(ppu_opcode_t op);
    void LFSU(ppu_opcode_t op);
    void LFD(ppu_opcode_t op);
    void LFDU(ppu_opcode_t op);
    void STFS(ppu_opcode_t op);
    void STFSU(ppu_opcode_t op);
    void STFD(ppu_opcode_t op);
    void STFDU(ppu_opcode_t op);
    void LD(ppu_opcode_t op);
    void LDU(ppu_opcode_t op);
    void LWA(ppu_opcode_t op);
    void FDIVS(ppu_opcode_t op);
    void FSUBS(ppu_opcode_t op);
    void FADDS(ppu_opcode_t op);
    void FSQRTS(ppu_opcode_t op);
    void FRES(ppu_opcode_t op);
    void FMULS(ppu_opcode_t op);
    void FMADDS(ppu_opcode_t op);
    void FMSUBS(ppu_opcode_t op);
    void FNMSUBS(ppu_opcode_t op);
    void FNMADDS(ppu_opcode_t op);
    void STD(ppu_opcode_t op);
    void STDU(ppu_opcode_t op);
    void MTFSB1(ppu_opcode_t op);
    void MCRFS(ppu_opcode_t op);
    void MTFSB0(ppu_opcode_t op);
    void MTFSFI(ppu_opcode_t op);
    void MFFS(ppu_opcode_t op);
    void MTFSF(ppu_opcode_t op);
    void FCMPU(ppu_opcode_t op);
    void FRSP(ppu_opcode_t op);
    void FCTIW(ppu_opcode_t op);
    void FCTIWZ(ppu_opcode_t op);
    void FDIV(ppu_opcode_t op);
    void FSUB(ppu_opcode_t op);
    void FADD(ppu_opcode_t op);
    void FSQRT(ppu_opcode_t op);
    void FSEL(ppu_opcode_t op);
    void FMUL(ppu_opcode_t op);
    void FRSQRTE(ppu_opcode_t op);
    void FMSUB(ppu_opcode_t op);
    void FMADD(ppu_opcode_t op);
    void FNMSUB(ppu_opcode_t op);
    void FNMADD(ppu_opcode_t op);
    void FCMPO(ppu_opcode_t op);
    void FNEG(ppu_opcode_t op);
    void FMR(ppu_opcode_t op);
    void FNABS(ppu_opcode_t op);
    void FABS(ppu_opcode_t op);
    void FCTID(ppu_opcode_t op);
    void FCTIDZ(ppu_opcode_t op);
    void FCFID(ppu_opcode_t op);

    void UNK(ppu_opcode_t op);
};