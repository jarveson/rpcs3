#pragma once

#include "PPUAnalyser.h"
#include <array>

#include <mutex>

// PPU Recompiler instance base (must be global or PS3 process-local)
class ppu_recompiler_base
{
protected:
    std::mutex m_mutex; // must be locked in compile()

    const ppu_rec_function_t* m_func; // current function

    ppu_jit_func_caller_t m_funcCaller{ nullptr };

    u32 m_pos; // current position

public:
    virtual ~ppu_recompiler_base() = default;

    // Compile specified function
    virtual void compile(ppu_rec_function_t& f, ppu_thread& ppu) = 0;

    virtual void createFuncCaller(ppu_thread& ppu) = 0;

    // Run
    static void enter(class ppu_thread&);
};
