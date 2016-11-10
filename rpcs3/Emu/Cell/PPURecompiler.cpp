#include "stdafx.h"
#include "Emu/IdManager.h"
#include "Emu/Memory/Memory.h"

#include "PPUThread.h"
#include "PPURecompiler.h"
#include "PPUASMJITRecompiler.h"

extern u64 get_system_time();

void ppu_recompiler_base::enter(ppu_thread& ppu)
{
    if (ppu.cia % 4)
    {
        fmt::throw_exception("Invalid cia: 0x%05x", ppu.cia);
    }
    const auto base = vm::ps3::_ptr<u32>(0);

    if (!ppu.ppu_rec)
    {
        ppu.ppu_rec = fxm::get_always<ppu_recompiler>();
    }

    if (!ppu.ppu_rec->m_funcCaller)
        ppu.ppu_rec->createFuncCaller(ppu);

    while (true) {
        // Always validate (TODO)

        ppu_jit_func_t funcPtr;
        if ((ppu.cia >> 2) < MAX_FUNC_PTRS)
            funcPtr = (ppu_jit_func_t)ppu.ppu_db->funcPointers[ppu.cia >> 2];
        if (funcPtr) {
            //const u32 res = funcPtr(&ppu);
            const u32 res = ppu.ppu_rec->m_funcCaller(funcPtr, &ppu);

            /*if (const auto exception = ppu.pending_exception)
            {
                ppu.pending_exception = nullptr;
                std::rethrow_exception(exception);
            }*/
            ppu.cia = res;
            // jump outside compiled region from interp
            if (res & 0x20000000) {
                //if (ppu.cia == 0x20000000)
                //break;
                ppu.cia -= 0x20000000;
                //break;
            }
            // pending exception
            else if (res & 0x40000000) {
                ppu.cia -= 0x40000000;
                break;
            }
            else if (res & 0x080000000) {
                // cpuStatus change
                ppu.cia -= 0x080000000;
                break;
            }
            continue;
        }

        const auto func = ppu.ppu_db->analyse(ppu.cia);

        if (func->can_be_compiled && !func->compiled)
        {
            ppu.ppu_rec->compile(*func, ppu);

        }
        if (func->compiled) {

            //const u32 res = func->compiled(&ppu);
            const u32 res = ppu.ppu_rec->m_funcCaller(func->compiled, &ppu);

            /*if (const auto exception = ppu.pending_exception)
            {
                ppu.pending_exception = nullptr;
                std::rethrow_exception(exception);
            }*/
            ppu.cia = res;
            // jump outside compiled region from interp
            if (res & 0x20000000) {
                //if (ppu.cia == 0x20000000)
                //    break;
                ppu.cia -= 0x20000000;
                //break;
            }
            // pending exception
            else if (res & 0x40000000) {
                ppu.cia -= 0x40000000;
                break;
            }
            else if (res & 0x080000000) {
                // cpuStatus change
                ppu.cia -= 0x080000000;
                break;
            }
            //break;
        }
        else {
            // else fall out and let interpreter take over
            break;
        }
    }

    /*if (res & 0x1000000)
    {
    ppu.halt();
    }

    if (res & 0x2000000)
    {
    }

    if (res & 0x4000000)
    {
    if (res & 0x8000000)
    {
    throw std::logic_error("Invalid interrupt status set" HERE);
    }

    spu.set_interrupt_status(true);
    }
    else if (res & 0x8000000)
    {
    spu.set_interrupt_status(false);
    }*/
}
