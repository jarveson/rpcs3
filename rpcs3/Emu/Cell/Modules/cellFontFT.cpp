#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

#include "cellFontFT.h"

#include FT_MODULE_H

logs::channel cellFontFT("cellFontFT", logs::level::notice);

void* FTAllocFunc(FT_Memory memory, long size)
{
    CellFontLibrary* cfl = (CellFontLibrary*)memory->user;
    if (cfl == nullptr)
        fmt::throw_exception("null userData in FTAllocFunc.");
    void* addr = cfl->MemoryIF.malloc(*cfl->ppuContext, cfl->MemoryIF.arg, (u32)size).get_ptr();
    if (addr == nullptr)
        fmt::throw_exception("FTAllocFunc callback returned null");
    return addr;
}

void FTFreeFunc(FT_Memory memory, void *block)
{
    CellFontLibrary* cfl = (CellFontLibrary*)memory->user;
    if (cfl == nullptr)
        fmt::throw_exception("null userData in FTFreeFunc.");
    cfl->MemoryIF.free(*cfl->ppuContext, cfl->MemoryIF.arg, vm::get_addr(block));
}

void* FTReallocFunc(FT_Memory memory, long cur_size, long new_size, void* block)
{
    CellFontLibrary* cfl = (CellFontLibrary*)memory->user;
    if (cfl == nullptr)
        fmt::throw_exception("null userData in FTReAllocFunc.");

    void* addr = cfl->MemoryIF.realloc(*cfl->ppuContext, cfl->MemoryIF.arg, vm::get_addr(block), (u32)new_size).get_ptr();
    if (addr == nullptr)
        fmt::throw_exception("FTReallocFunc callback returned null");
    return addr;
}

s32 cellFontInitLibraryFreeTypeWithRevision(ppu_thread& ppu, u64 revisionFlags, vm::ptr<CellFontLibraryConfigFT> config, vm::pptr<CellFontLibrary> lib)
{
	cellFontFT.warning("cellFontInitLibraryFreeTypeWithRevision(revisionFlags=0x%llx, config=*0x%x, lib=**0x%x)", revisionFlags, config, lib);
    
    auto library = vm::ptr<CellFontLibrary>::make(config->MemoryIF.malloc(ppu, config->MemoryIF.arg, sizeof(CellFontLibrary)).addr());
    if (library == vm::null) {
        cellFontFT.error("Failed to allocate CellFontLibrary.");
        return CELL_FONT_ERROR_ALLOCATION_FAILED;
    }

    library->ftMemory.user = (void*)library.get_ptr();
    library->ftMemory.alloc = FTAllocFunc;
    library->ftMemory.realloc = FTReallocFunc;
    library->ftMemory.free = FTFreeFunc;

    library->MemoryIF.arg = config->MemoryIF.arg;
    library->MemoryIF.free = config->MemoryIF.free;
    library->MemoryIF.malloc = config->MemoryIF.malloc;
    library->MemoryIF.realloc = config->MemoryIF.realloc;

    library->ppuContext = &ppu;

    FT_Library ftLibrary;
    FT_Error res = FT_New_Library(&library->ftMemory, &ftLibrary);
    if (res != FT_Err_Ok) {
        cellFontFT.error("FreeType Init Failed %d", res);
        return CELL_FONT_ERROR_INITIALIZE_FAILED;
    }
    FT_Add_Default_Modules(ftLibrary);
    library->ftLibrary = ftLibrary;
    *lib = library;
	return CELL_OK;
}

s32 cellFontFTGetRevisionFlags(vm::ptr<u64> revisionFlags)
{
    cellFontFT.warning("cellFontFTGetRevisionFlags(revisionFlags=0x%x)", revisionFlags);
    if (revisionFlags == vm::null)
        return CELL_OK;
    // this is what ps3 returns
    *revisionFlags = 0x62;
    return CELL_OK;
}

s32 cellFontFTGetInitializedRevisionFlags()
{
    UNIMPLEMENTED_FUNC(cellFontFT);
    return CELL_OK;
}

DECLARE(ppu_module_manager::cellFontFT)("cellFontFT", []()
{
	REG_FUNC(cellFontFT, cellFontInitLibraryFreeTypeWithRevision);
	REG_FUNC(cellFontFT, cellFontFTGetRevisionFlags);
	REG_FUNC(cellFontFT, cellFontFTGetInitializedRevisionFlags);
});
