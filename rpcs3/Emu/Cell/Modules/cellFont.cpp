#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"
#include "Emu/Cell/PPUModule.h"

#include "cellFont.h"
#include FT_MODULE_H
#include FT_BITMAP_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_GLYPH_H

// just an internal mapping to system font files;
enum class CellSysFontNum
{
    RD_R_LATIN = 1,
    RD_L_LATIN,
    RD_B_LATIN,
    RD_R_LATIN2,
    RD_L_LATIN2,
    RD_B_LATIN2,

    MT_R_LATIN,

    NR_R_JPN,
    NR_L_JPN,
    NR_B_JPN,

    YG_R_KOR,

    SR_R_LATIN,
    SR_R_LATIN2,
    SR_R_JPN,

    VR_R_LATIN,
    VR_R_LATIN2,

    // found only in font-sets?
    DH_R_CGB,
    CP_R_KANA,
    YG_L_KOR,
    YG_B_KOR,
};

std::unordered_map<CellSysFontNum, std::string> CellSysFontPaths =
    {
        { CellSysFontNum::RD_R_LATIN, "/dev_flash/data/font/SCE-PS3-RD-R-LATIN.TTF" },
        { CellSysFontNum::RD_L_LATIN, "/dev_flash/data/font/SCE-PS3-RD-L-LATIN.TTF" },
        { CellSysFontNum::RD_B_LATIN, "/dev_flash/data/font/SCE-PS3-RD-B-LATIN.TTF" },

        { CellSysFontNum::RD_R_LATIN2, "/dev_flash/data/font/SCE-PS3-RD-R-LATIN2.TTF" },
        { CellSysFontNum::RD_L_LATIN2, "/dev_flash/data/font/SCE-PS3-RD-L-LATIN2.TTF" },
        { CellSysFontNum::RD_B_LATIN2, "/dev_flash/data/font/SCE-PS3-RD-B-LATIN2.TTF" },

        { CellSysFontNum::MT_R_LATIN, "/dev_flash/data/font/SCE-PS3-MT-R-LATIN.TTF" },

        { CellSysFontNum::NR_R_JPN, "/dev_flash/data/font/SCE-PS3-NR-R-JPN.TTF" },
        { CellSysFontNum::NR_L_JPN, "/dev_flash/data/font/SCE-PS3-NR-L-JPN.TTF" },
        { CellSysFontNum::NR_B_JPN, "/dev_flash/data/font/SCE-PS3-NR-B-JPN.TTF" },

        { CellSysFontNum::YG_R_KOR, "/dev_flash/data/font/SCE-PS3-YG-R-KOR.TTF" },

        { CellSysFontNum::SR_R_LATIN, "/dev_flash/data/font/SCE-PS3-SR-R-LATIN.TTF" },
        { CellSysFontNum::SR_R_LATIN2,"/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF" },
        { CellSysFontNum::SR_R_JPN, "/dev_flash/data/font/SCE-PS3-SR-R-JPN.TTF" },

        { CellSysFontNum::VR_R_LATIN, "/dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF" },
        { CellSysFontNum::VR_R_LATIN2, "/dev_flash/data/font/SCE-PS3-VR-R-LATIN2.TTF" },

        { CellSysFontNum::DH_R_CGB, "/dev_flash/data/font/SCE-PS3-DH-R-CGB.TTF" },
        { CellSysFontNum::CP_R_KANA, "/dev_flash/data/font/SCE-PS3-CP-R-KANA.TTF" },
        { CellSysFontNum::YG_L_KOR, "/dev_flash/data/font/SCE-PS3-YG-L-KOR.TTF" },
        { CellSysFontNum::YG_B_KOR, "/dev_flash/data/font/SCE-PS3-YG-B-KOR.TTF" }
    };

// internal use for loaded system font
struct CellSysFontEntry
{
    FT_Face face{ nullptr };
    // bufsize may be 0 when loaded, which indicates its file access mode
    u32 bufSize{ 0 };
    vm::ptr<void> buffer;
    u32 refCount{ 1 };
};

struct CellFontManager
{
    CellFontConfig config;

    // holds loaded fonts
    std::unordered_map<CellSysFontNum, CellSysFontEntry> system_fonts;
};

logs::channel cellFont("cellFont", logs::level::notice);

// This set of load flags seems to match what the ps3 does, used for loading chars/glyphs
constexpr static u32 CELLFONT_FT_LOADFLAGS = FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP;//FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_LINEAR_DESIGN;// | FT_LOAD_NO_SCALE | FT_LOAD_BITMAP_METRICS_ONLY;
constexpr static u32 CELLFONT_FT_FONTPIXELSIZE = 18; // default font size, we don't actually request any other size than this nor change it

std::vector<CellSysFontNum> cellFontFileNumsFromType(u32 type) {
    switch (type)
    {
        // specific fonts
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_LATIN:         return{ CellSysFontNum::RD_R_LATIN };
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_LIGHT_LATIN:   return{ CellSysFontNum::RD_L_LATIN };
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_BOLD_LATIN:    return{ CellSysFontNum::RD_B_LATIN };
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_LATIN2:        return{ CellSysFontNum::RD_R_LATIN2 };
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_LIGHT_LATIN2:  return{ CellSysFontNum::RD_L_LATIN2 };
    case CELL_FONT_TYPE_RODIN_SANS_SERIF_BOLD_LATIN2:   return{ CellSysFontNum::RD_B_LATIN2 };
    case CELL_FONT_TYPE_MATISSE_SERIF_LATIN:            return{ CellSysFontNum::MT_R_LATIN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_JAPANESE:       return{ CellSysFontNum::NR_R_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_LIGHT_JAPANESE: return{ CellSysFontNum::NR_L_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_BOLD_JAPANESE:  return{ CellSysFontNum::NR_B_JPN };
    case CELL_FONT_TYPE_YD_GOTHIC_KOREAN:               return{ CellSysFontNum::YG_R_KOR };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_LATIN:       return{ CellSysFontNum::SR_R_LATIN };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_LATIN2:      return{ CellSysFontNum::SR_R_LATIN2 };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_JAPANESE:    return{ CellSysFontNum::SR_R_JPN };
    case CELL_FONT_TYPE_VAGR_SANS_SERIF_ROUND:          return{ CellSysFontNum::VR_R_LATIN };
    case CELL_FONT_TYPE_VAGR_SANS_SERIF_ROUND_LATIN2:   return{ CellSysFontNum::VR_R_LATIN2 };

    // confirmed by ps3 cellfont lib
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_JP_SET:
        return{ CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_LATIN_SET:
        return{ CellSysFontNum::RD_R_LATIN, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN_SET:
        return{ CellSysFontNum::RD_R_LATIN, 
                CellSysFontNum::NR_R_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN2_SET:
        return{ CellSysFontNum::RD_R_LATIN2, 
                CellSysFontNum::NR_R_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_RODIN2_SET:
        return{ CellSysFontNum::RD_R_LATIN2, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_DFHEI5_SET:
        return{ CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_DFHEI5_RODIN_SET:
        return{ CellSysFontNum::RD_R_LATIN, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_DFHEI5_RODIN2_SET:
        return{ CellSysFontNum::RD_R_LATIN2, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_TCH_SET:
        return{ CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_RODIN_TCH_SET:
        return{ CellSysFontNum::RD_R_LATIN, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_RODIN2_TCH_SET:
        return{ CellSysFontNum::RD_R_LATIN2, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_SCH_SET:
        return{ CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_RODIN_SCH_SET:
        return{ CellSysFontNum::RD_R_LATIN, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_DFHEI5_GOTHIC_YG_NEWRODIN_RODIN2_SCH_SET:
        return{ CellSysFontNum::RD_R_LATIN2, 
                CellSysFontNum::NR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_RSANS_SET:
        return{ CellSysFontNum::SR_R_LATIN,
                CellSysFontNum::SR_R_JPN };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_RSANS_SET:
        return{ CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_JP_SET:
        return{ CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_YG_DFHEI5_RSANS_SET:
        return{ CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_YG_DFHEI5_RSANS_SET:
        return{ CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_VAGR_SEURAT_CAPIE_MARU_GOTHIC_RSANS_SET:
        return{ CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::VR_R_LATIN };
    case CELL_FONT_TYPE_VAGR_SEURAT_CAPIE_MARU_GOTHIC_YG_DFHEI5_RSANS_SET:
        return{ CellSysFontNum::VR_R_LATIN, 
                CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_LIGHT_SET:
        return{ CellSysFontNum::NR_L_JPN,
                CellSysFontNum::YG_L_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_RODIN_LIGHT_SET:
        return{ CellSysFontNum::RD_L_LATIN,
                CellSysFontNum::NR_L_JPN,   
                CellSysFontNum::YG_L_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_RODIN2_LIGHT_SET:
        return{ CellSysFontNum::RD_L_LATIN2, 
                CellSysFontNum::NR_L_JPN,
                CellSysFontNum::YG_L_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN_LIGHT_SET:
        return{ CellSysFontNum::RD_L_LATIN,
                CellSysFontNum::NR_L_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN2_LIGHT_SET:
        return{ CellSysFontNum::RD_L_LATIN2, 
                CellSysFontNum::NR_L_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_BOLD_SET:
        return{ CellSysFontNum::NR_B_JPN, 
                CellSysFontNum::YG_B_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_RODIN_BOLD_SET:
        return{ CellSysFontNum::RD_B_LATIN, 
                CellSysFontNum::NR_B_JPN, 
                CellSysFontNum::YG_B_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_YG_RODIN2_BOLD_SET:
        return{ CellSysFontNum::RD_B_LATIN2, 
                CellSysFontNum::NR_B_JPN, 
                CellSysFontNum::YG_B_KOR };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN_BOLD_SET:
        return{ CellSysFontNum::RD_B_LATIN, 
                CellSysFontNum::NR_B_JPN };
    case CELL_FONT_TYPE_NEWRODIN_GOTHIC_RODIN2_BOLD_SET:
        return{ CellSysFontNum::RD_B_LATIN2, 
                CellSysFontNum::NR_B_JPN };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_RSANS2_SET:
        return{ CellSysFontNum::SR_R_LATIN2, 
                CellSysFontNum::SR_R_JPN };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_RSANS2_SET:
        return{ CellSysFontNum::SR_R_LATIN2, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA };
    case CELL_FONT_TYPE_SEURAT_MARU_GOTHIC_YG_DFHEI5_RSANS2_SET:
        return{ CellSysFontNum::SR_R_LATIN2, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_YG_DFHEI5_RSANS2_SET:
        return{ CellSysFontNum::SR_R_LATIN2, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::YG_R_KOR,
                CellSysFontNum::DH_R_CGB };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_YG_DFHEI5_VAGR2_SET:
        return{ CellSysFontNum::SR_R_LATIN, 
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::YG_R_KOR, 
                CellSysFontNum::DH_R_CGB, 
                CellSysFontNum::VR_R_LATIN2 };
    case CELL_FONT_TYPE_SEURAT_CAPIE_MARU_GOTHIC_VAGR2_SET:
        return{ CellSysFontNum::SR_R_LATIN,
                CellSysFontNum::SR_R_JPN, 
                CellSysFontNum::CP_R_KANA, 
                CellSysFontNum::VR_R_LATIN2 };
    default:
        cellFont.error("fontType->type = %d not supported.", type);
        return{};
    }
}

void SetCellFontDefaults(vm::ptr<CellFont> font)
{
    font->renderer_addr = vm::null;
    font->lib = vm::null;

    font->hDpi = 72;
    font->vDpi = 72;

    font->effectWeight = 1.f;

    font->slant = 0.f;

    font->point_w = 18.f;
    font->point_h = 18.f;

    font->pixel_h = 18.f;
    font->pixel_w = 18.f;

    font->isSysFont = false;
    font->fontNum = -1;
}

void SetFTFaceDefaults(FT_Face face)
{
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    FT_Set_Pixel_Sizes(face, 18, 18);
}

// Functions
s32 cellFontInitializeWithRevision(u64 revisionFlags, vm::ptr<CellFontConfig> config)
{
	cellFont.warning("cellFontInitializeWithRevision(revisionFlags=0x%llx, config=*0x%x)", revisionFlags, config);
	if (config->fc_size < 24)
		return CELL_FONT_ERROR_INVALID_PARAMETER;

    const auto m = fxm::make<CellFontManager>();

    if (!m)
        return CELL_FONT_ERROR_ALREADY_INITIALIZED;

	if (config->flags != 0)
		cellFont.error("cellFontInitializeWithRevision: Unknown flags (0x%x)", config->flags);

    cellFont.warning("fc_size: %d, entryMax %d", config->fc_size, config->userFontEntryMax);

    // todo: im assuming they use some custom ft_stream, with the 'buffer' given in config to handle the caching of the stream

    m->config.fc_buffer = config->fc_buffer;
    m->config.fc_size = config->fc_size;
    m->config.flags = config->flags;
    m->config.userFontEntryMax = config->userFontEntryMax;
    m->config.userFontEntrys = config->userFontEntrys;

    memset(m->config.userFontEntrys.get_ptr(), 0, m->config.userFontEntryMax * sizeof(CellFontEntry));

	return CELL_OK;
}

s32 cellFontGetRevisionFlags(vm::ptr<u64> revisionFlags)
{
    cellFont.warning("cellFontGetRevisionFlags(revisionFlags=0x%x)", revisionFlags);
    if (revisionFlags == vm::null)
        return CELL_OK;
    // this is what ps3 returns
    *revisionFlags = 0x62;
	return CELL_OK;
}

s32 cellFontEnd()
{
	cellFont.warning("cellFontEnd()");
    //todo: close and dealloc

	return CELL_OK;
}

s32 cellFontSetFontsetOpenMode(u32 openMode)
{
	cellFont.todo("cellFontSetFontsetOpenMode(openMode=0x%x)", openMode);
	return CELL_OK;
}

s32 cellFontOpenFontMemory(ppu_thread& ppu, vm::ptr<CellFontLibrary> library, vm::ptr<void> fontAddr, u32 fontSize, u32 subNum, u32 uniqueId, vm::ptr<CellFont> font)
{
	cellFont.warning("cellFontOpenFontMemory(library=*0x%x, fontAddr=0x%x, fontSize=%d, subNum=%d, uniqueId=%d, font=*0x%x)", library, fontAddr, fontSize, subNum, uniqueId, font);

    if (library == vm::null || fontAddr == vm::null || font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || library->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;

    FT_Face outFace;
    library->ppuContext = &ppu;

    const FT_Error res = FT_New_Memory_Face(library->ftLibrary, static_cast<FT_Byte*>(fontAddr.get_ptr()), fontSize, subNum, &outFace);
    if (res != FT_Err_Ok) 
    {
        cellFont.error("FreeType New_Memory_Face Failed 0x%x", res);
        return CELL_FONT_ERROR_FONT_OPEN_FAILED;
    }

    if (outFace == nullptr) 
    {
        cellFont.error("FreeType outFace is null.");
        return CELL_FONT_ERROR_FONT_OPEN_FAILED;
    }

    SetCellFontDefaults(font);
    SetFTFaceDefaults(outFace);

    font->lib = library;
    font->isSysFont = false;
    font->fontNum = uniqueId;

	return CELL_OK;
}

s32 cellFontOpenFontFile(ppu_thread& ppu, vm::ptr<CellFontLibrary> library, vm::cptr<char> fontPath, u32 subNum, s32 uniqueId, vm::ptr<CellFont> font)
{
	cellFont.warning("cellFontOpenFontFile(library=*0x%x, fontPath=%s, subNum=%d, uniqueId=%d, font=*0x%x)", library, fontPath, subNum, uniqueId, font);

    if (library == vm::null || fontPath == vm::null || font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || library->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;

    FT_Face outFace;
    library->ppuContext = &ppu;

    const FT_Error res = FT_New_Face(library->ftLibrary, vfs::get(fontPath.get_ptr()).c_str(), 0, &outFace);

    if (res != FT_Err_Ok)
    {
        cellFont.error("FreeType FT_New_Face Failed 0x%x", res);
        return CELL_FONT_ERROR_FONT_OPEN_FAILED;
    }

    if (outFace == nullptr)
    {
        cellFont.error("FreeType outFace is null.");
        return CELL_FONT_ERROR_FONT_OPEN_FAILED;
    }

    SetCellFontDefaults(font);
    SetFTFaceDefaults(outFace);

    font->lib = library;
    font->isSysFont = false;
    font->fontNum = uniqueId;

    return CELL_OK;
}

s32 cellFontOpenFontset(ppu_thread& ppu, vm::ptr<CellFontLibrary> library, vm::ptr<CellFontType> fontType, vm::ptr<CellFont> font)
{
	cellFont.warning("cellFontOpenFontset(library=*0x%x, fontType=*0x%x, font=*0x%x)", library, fontType, font);

    if (library == vm::null || fontType == vm::null || font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || library->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;
	
    const auto fileNums = cellFontFileNumsFromType(fontType->type);
    if (fileNums.size() == 0)
        return CELL_FONT_ERROR_NO_SUPPORT_FONTSET;

    if (fontType->map != CELL_FONT_MAP_UNICODE)
        cellFont.error("cellFontOpenFontset: Only Unicode is supported");

    library->ppuContext = &ppu;

    for (auto const& fileNum : fileNums) {
        FT_Face outFace;
    
        auto sysFont = fm->system_fonts.find(fileNum);
        if (sysFont != fm->system_fonts.end())
        {
            sysFont->second.refCount++;
            continue;
        }

        const auto filepath = CellSysFontPaths.find(fileNum);
        if (filepath == CellSysFontPaths.end())
            fmt::throw_exception("Unknown filenum in CellSysFontPath. %d", (u32)fileNum);

        const std::string& path = CellSysFontPaths[fileNum];

        const FT_Error res = FT_New_Face(library->ftLibrary, vfs::get(path).c_str(), 0, &outFace);

        if (res == FT_Err_Cannot_Open_Resource)
        {
            cellFont.error("Freetype failed to open font file %s", vfs::get(path).c_str());
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }
        else if (res != FT_Err_Ok)
        {
            cellFont.error("FreeType FT_New_Face Failed 0x%x", res);
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }

        if (outFace == nullptr)
        {
            cellFont.error("FreeType outFace is null.");
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }

        SetFTFaceDefaults(outFace);

        CellSysFontEntry tmp;
        tmp.face = outFace;
        fm->system_fonts.emplace(fileNum, tmp);
    }

    SetCellFontDefaults(font);

    font->lib = library;
    font->isSysFont = true;
    font->fontNum = fontType->type;

    return CELL_OK;
}

s32 cellFontOpenFontsetOnMemory(ppu_thread& ppu, vm::ptr<CellFontLibrary> library, vm::ptr<CellFontType> fontType, vm::ptr<CellFont> font)
{
    cellFont.warning("cellFontOpenFontsetOnMemory(library=*0x%x, fontType=*0x%x, font=*0x%x)", library, fontType, font);

    if (library == vm::null || fontType == vm::null || font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || library->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;

    if (fontType->map != CELL_FONT_MAP_UNICODE)
        cellFont.warning("cellFontOpenFontsetOnMemory: Only Unicode is supported");

    const auto fileNums = cellFontFileNumsFromType(fontType->type);
    if (fileNums.size() == 0)
        return CELL_FONT_ERROR_NO_SUPPORT_FONTSET;

    library->ppuContext = &ppu;

    for (auto const& fileNum : fileNums) {
        auto sysFont = fm->system_fonts.find(fileNum);
        if (sysFont != fm->system_fonts.end())
        {
            sysFont->second.refCount++;
            continue;
        }

        const auto filepath = CellSysFontPaths.find(fileNum);
        if (filepath == CellSysFontPaths.end())
            fmt::throw_exception("Unknown filenum in CellSysFontPath. %d", (u32)fileNum);

        const fs::file f(vfs::get(filepath->second));
        if (!f)
        {
            cellFont.error("Failed to load font file %s", filepath->second.c_str());
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }

        const u32 fileSize = ::size32(f);
        const auto allocAddr = font->lib->MemoryIF.malloc(ppu, font->lib->MemoryIF.arg, fileSize);
        if (allocAddr == vm::null)
        {
            cellFont.error("cellFontRenderCharGlyphImage(): Alloc buffer failed!");
            return CELL_FONT_ERROR_ALLOCATION_FAILED;
        }
        FT_Face outFace;

        const FT_Error res = FT_New_Memory_Face(library->ftLibrary, static_cast<FT_Byte*>(allocAddr.get_ptr()), fileSize, 0, &outFace);

        if (res != FT_Err_Ok)
        {
            font->lib->MemoryIF.free(ppu, font->lib->MemoryIF.arg, allocAddr);
            cellFont.error("FreeType New_Memory_Face Failed 0x%x", res);
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }

        if (outFace == nullptr)
        {
            font->lib->MemoryIF.free(ppu, font->lib->MemoryIF.arg, allocAddr);
            cellFont.error("FreeType outFace is null.");
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }

        SetFTFaceDefaults(outFace);

        CellSysFontEntry tmp;
        tmp.face = outFace;
        tmp.buffer = allocAddr;
        tmp.bufSize = fileSize;
        fm->system_fonts.emplace(fileNum, tmp);
    }

    SetCellFontDefaults(font);

    font->lib = library;
    font->isSysFont = true;
    font->fontNum = fontType->type;

    return CELL_OK;
}

s32 cellFontOpenFontInstance(vm::ptr<CellFont> openedFont, vm::ptr<CellFont> font)
{
    cellFont.warning("cellFontOpenFontInstance(openedFont=*0x%x, font=*0x%x)", openedFont, font);

    if (openedFont == vm::null || font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || openedFont->lib == vm::null || openedFont->lib->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;

    if (!openedFont->isSysFont)
        fmt::throw_exception("todo");

    const auto fileNums = cellFontFileNumsFromType(openedFont->fontNum);
    if (fileNums.size() == 0)
        return CELL_FONT_ERROR_NO_SUPPORT_FONTSET;

    for (auto const& fileNum : fileNums) {
        auto sysFont = fm->system_fonts.find(fileNum);
        if (sysFont != fm->system_fonts.end())
        {
            sysFont->second.refCount++;
        }
        else
        {
            cellFont.error("cellFontOpenFontInstance: Couldn't find %d in system_font %d. ", (u32)fileNum, openedFont->fontNum);
        }
    }

    // todo: double check im not missing any of these
    memset(font.get_ptr(), 0, sizeof(CellFont));

    SetCellFontDefaults(font);

    font->pixel_h = openedFont->pixel_h;
    font->pixel_w = openedFont->pixel_w;

    font->hDpi = openedFont->hDpi;
    font->vDpi = openedFont->vDpi;

    font->point_h = openedFont->point_h;
    font->point_w = openedFont->point_w;

    font->effectWeight = openedFont->effectWeight;

    font->slant = openedFont->slant;

    font->renderer_addr = openedFont->renderer_addr;
    font->lib = openedFont->lib;

    font->fontNum = openedFont->fontNum;
    font->isSysFont = openedFont->isSysFont;

    return CELL_OK;
}

s32 cellFontSetFontOpenMode(vm::ptr<CellFontLibrary> library, u32 openMode)
{
	cellFont.todo("cellFontSetFontOpenMode(openMode=0x%x)", openMode);
	return CELL_OK;
}

s32 cellFontCreateRenderer(vm::ptr<CellFontLibrary> library, vm::ptr<CellFontRendererConfig> config, vm::ptr<CellFontRenderer> renderer)
{
	cellFont.warning("cellFontCreateRenderer(library=*0x%x, config=*0x%x, Renderer=*0x%x)", library, config, renderer);

    renderer->buffer = config->buffer;
    renderer->maxBufferSize = config->maxSize;
    renderer->currentBufferSize = config->initSize;

    renderer->scale_h = 0.f;
    renderer->scale_w = 0.f;

    renderer->effectWeight = 1.f;

    renderer->slant = 0.f;

	return CELL_OK;
}

void cellFontRenderSurfaceInit(vm::ptr<CellFontRenderSurface> surface, vm::ptr<void> buffer, s32 bufferWidthByte, s32 pixelSizeByte, s32 w, s32 h)
{
	cellFont.warning("cellFontRenderSurfaceInit(surface=*0x%x, buffer=*0x%x, bufferWidthByte=%d, pixelSizeByte=%d, w=%d, h=%d)", surface, buffer, bufferWidthByte, pixelSizeByte, w, h);

	surface->buffer         = buffer;
	surface->widthByte      = bufferWidthByte;
	surface->pixelSizeByte  = pixelSizeByte;
	surface->width          = w;
	surface->height         = h;
}

void cellFontRenderSurfaceSetScissor(vm::ptr<CellFontRenderSurface> surface, s32 x0, s32 y0, s32 w, s32 h)
{
	cellFont.warning("cellFontRenderSurfaceSetScissor(surface=*0x%x, x0=%d, y0=%d, w=%d, h=%d)", surface, x0, y0, w, h);

	surface->sc_x0 = x0;
	surface->sc_y0 = y0;
	surface->sc_x1 = w;
	surface->sc_y1 = h;
}

s32 cellFontGetHorizontalLayout(vm::ptr<CellFont> font, vm::ptr<CellFontHorizontalLayout> layout)
{
	cellFont.warning("cellFontGetHorizontalLayout(font=*0x%x, layout=*0x%x)", font, layout);

    if (font == vm::null || layout == vm::null )
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    auto fm = fxm::get<CellFontManager>();

    if (!fm || !font->lib || font->lib->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;

    // these may look a bit weird, but its *very* close to exact output for ps3
    // its ft_mulfix expressed differently to keep it closer to ps3

    // todo: account for, effectwidth / slant?

    if (!font->isSysFont)
        fmt::throw_exception("todo");

    const auto fileNums = cellFontFileNumsFromType(font->fontNum);
    if (fileNums.size() == 0)
        return CELL_FONT_ERROR_FONT_OPEN_FAILED;

    // the idea for system fonts seems to be loop through fonts included in set,
    // and return the largest 'box'

    //todo: do this generically for stock size, save it, then use it later?

    f32 baseline = 0.f;
    f32 lineheight = 0.f;

    for (auto const& fileNum : fileNums) {
        auto sysFont = fm->system_fonts.find(fileNum);
        if (sysFont != fm->system_fonts.end())
        {
            const FT_Face& face = sysFont->second.face;
            const u32 scale = ((font->point_h * 64) * 65536) / face->units_per_EM;

            baseline = std::max(((face->bbox.yMax * scale) / 65536) / 64.f, baseline);
            lineheight = std::max((((face->bbox.yMax - face->bbox.yMin) * scale) / 65536) / 64.f, lineheight);
        }
        else
        {
            cellFont.error("Couldn't find %d in system_font %d. ", (u32)fileNum, font->fontNum);
            return CELL_FONT_ERROR_FONT_OPEN_FAILED;
        }
    }

    layout->baseLineY = baseline;
    layout->lineHeight = lineheight;

    // todo: how to calcuate this? somehow account for effectwidth / slant
    layout->effectHeight = 0.f;// font->ftFace->size->metrics.height - ;
    
    /*
    cellFont.warning("ftHeight:%d", font->ftFace->height);
    cellFont.warning("unitsPerem:%d", font->ftFace->units_per_EM);
    cellFont.warning("ascender:%d", font->ftFace->ascender);
    cellFont.warning("desc:%d", font->ftFace->descender);
    cellFont.warning("scaledAsc:%d", font->ftFace->size->metrics.ascender);
    cellFont.warning("scaleddesc:%d", font->ftFace->size->metrics.descender);
    cellFont.warning("scaledheight:%d", font->ftFace->size->metrics.height);
    cellFont.warning("scalexppem:%d", font->ftFace->size->metrics.x_ppem);
    cellFont.warning("scalexscale:%d", font->ftFace->size->metrics.x_scale);
    cellFont.warning("scaleyppem:%d", font->ftFace->size->metrics.y_ppem);
    cellFont.warning("scaleyscale:%d", font->ftFace->size->metrics.y_scale);
    cellFont.warning("maxAdvancedWidth: %d", font->ftFace->max_advance_width);
    cellFont.warning("maxAdvancedHeight: %d", font->ftFace->max_advance_height);

    cellFont.warning("bboxHeight:%d", font->ftFace->bbox.yMax - font->ftFace->bbox.yMin);
    cellFont.warning("bboyyMax: %d", font->ftFace->bbox.yMax);

    cellFont.warning("bboxwidth:%d", font->ftFace->bbox.xMax - font->ftFace->bbox.xMin);
    cellFont.warning("bboxxMax: %d", font->ftFace->bbox.xMax);

    cellFont.warning("underlinepos: %d", font->ftFace->underline_position);
    cellFont.warning("underlinethickness: %d", font->ftFace->underline_thickness);
    */
	return CELL_OK;
}

s32 cellFontBindRenderer(vm::ptr<CellFont> font, vm::ptr<CellFontRenderer> renderer)
{
	cellFont.warning("cellFontBindRenderer(font=*0x%x, renderer=*0x%x)", font, renderer);

	if (font->renderer_addr != vm::null)
	{
		return CELL_FONT_ERROR_RENDERER_ALREADY_BIND;
	}

	font->renderer_addr = renderer;

	return CELL_OK;
}

s32 cellFontUnbindRenderer(vm::ptr<CellFont> font)
{
	cellFont.warning("cellFontBindRenderer(font=*0x%x)", font);
	
	if (font->renderer_addr == vm::null)
	{
		return CELL_FONT_ERROR_RENDERER_UNBIND;
	}

	font->renderer_addr = vm::null;

	return CELL_OK;
}

s32 cellFontDestroyRenderer()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontSetupRenderScalePixel(vm::ptr<CellFont> font, float w, float h)
{
	cellFont.warning("cellFontSetupRenderScalePixel(font=*0x%x, w=%f, h=%f)", font, w, h);

    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

	if (font->renderer_addr == vm::null)
		return CELL_FONT_ERROR_RENDERER_UNBIND;

    font->renderer_addr->scale_w = w;
    font->renderer_addr->scale_h = h;

	return CELL_OK;
}

s32 cellFontGetRenderCharGlyphMetrics(vm::ptr<CellFont> font, u32 code, vm::ptr<CellFontGlyphMetrics> metrics)
{
	cellFont.todo("cellFontGetRenderCharGlyphMetrics(font=*0x%x, code=0x%x, metrics=*0x%x)", font, code, metrics);

	if (font->renderer_addr == vm::null)
	{
		return CELL_FONT_ERROR_RENDERER_UNBIND;
	}

	// TODO: ?
	return CELL_OK;
}

s32 cellFontEndLibrary(ppu_thread& ppu, vm::ptr<CellFontLibrary> library)
{
    cellFont.warning("cellFontEndLibrary(library=0x%x)", library);
    if (library->ftLibrary == nullptr)
        return CELL_FONT_ERROR_UNINITIALIZED;
    library->ppuContext = &ppu;
    const FT_Error res = FT_Done_Library(library->ftLibrary);
    if (res)
    {
        cellFont.error("Error deleting ftLibrary, %d", res);
    }

    library->MemoryIF.free(ppu, library->MemoryIF.arg, library);
	return CELL_OK;
}

s32 cellFontSetEffectSlant(vm::ptr<CellFont> font, float slantParam)
{
	cellFont.warning("cellFontSetEffectSlant(font=*0x%x, slantParam=%f)", font, slantParam);

	if (slantParam < -1.0 || slantParam > 1.0)
	{
		return CELL_FONT_ERROR_INVALID_PARAMETER;
	}

	font->slant = slantParam;

	return CELL_OK;
}

s32 cellFontGetEffectSlant(vm::ptr<CellFont> font, vm::ptr<float> slantParam)
{
	cellFont.warning("cellFontSetEffectSlant(font=*0x%x, slantParam=*0x%x)", font, slantParam);

    if (slantParam == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

	*slantParam = font->slant;

	return CELL_OK;
}

s32 cellFontGetFontIdCode(vm::ptr<CellFont> font, u32 code, vm::ptr<u32> fontId, vm::ptr<u32> fontCode)
{
	cellFont.warning("cellFontGetFontIdCode(font=*0x%x, code=%d, fontId=*0x%x, fontCode=*0x%x)", font, code, fontId, fontCode);

    if (fontId == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    const auto m = fxm::get<CellFontManager>();
    if (!m)
        return CELL_FONT_ERROR_UNINITIALIZED;

    // custom font just returns unique number given 
    //todo: *Actually* check this
    if (!font->isSysFont)
    {
        *fontId = font->fontNum;
        if (fontCode != vm::null)
            *fontCode = code;
        return CELL_OK;
    }

    *fontId = 0;

    auto fontNums = cellFontFileNumsFromType(font->fontNum);
    if (fontNums.size() == 0) //uh..
        fmt::throw_exception("Invalid sysfontnum in font given. %d", font->fontNum);

    for (auto const& num : fontNums)
    {
        const auto sysfont = m->system_fonts.find(num);
        if (sysfont == m->system_fonts.end())
            fmt::throw_exception("fontNum not found in system_fonts. %d", (u32)num);

        const u32 glyphIdx = FT_Get_Char_Index(sysfont->second.face, code);
        if (glyphIdx > 0)
        {
            if (fontCode != vm::null)
                *fontCode = glyphIdx;
            *fontId = static_cast<u32>(num);
            return CELL_OK;
        }
    }
    cellFont.error("cellFontGetFontIdCode: couldn't find requested code %d in fontset", code, font->fontNum);

	return CELL_FONT_ERROR_NO_SUPPORT_CODE;
}

s32 cellFontCloseFont(ppu_thread &ppu, vm::ptr<CellFont> font)
{
	cellFont.warning("cellFontCloseFont(font=*0x%x)", font);
    font->lib->ppuContext = &ppu;
    //FT_Done_Face(font->ftFace);

    //todo:
	return CELL_OK;
}

s32 cellFontGetCharGlyphMetrics(ppu_thread &ppu, vm::ptr<CellFont> font, u32 code, vm::ptr<CellFontGlyphMetrics> metrics)
{
	cellFont.warning("cellFontGetCharGlyphMetrics(font=*0x%x, code=0x%x, metrics=*0x%x)", font, code, metrics);

    if (font == vm::null || metrics == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;
    const auto m = fxm::get<CellFontManager>();
    if (!m)
        return CELL_FONT_ERROR_UNINITIALIZED;

    if (code == 0)
        return CELL_FONT_ERROR_NO_SUPPORT_CODE;

    font->lib->ppuContext = &ppu;

    if (!font->isSysFont)
        fmt::throw_exception("todo");

    auto fontId = vm::make_var<u32>(0);
    auto fontCode = vm::make_var<u32>(0);
    const u32 ret = cellFontGetFontIdCode(font, code, fontId, fontCode);
    if (ret != 0)
        return ret;

    const auto& sysfont = m->system_fonts.at((CellSysFontNum)fontId->value());

    FT_Matrix matrix;
    matrix.xx = (FT_Fixed)(font->pixel_w / CELLFONT_FT_FONTPIXELSIZE * 0x10000L);
    matrix.xy = (FT_Fixed)(0.0f * 0x10000L);
    matrix.yx = (FT_Fixed)(0.0f * 0x10000L);
    matrix.yy = (FT_Fixed)(font->pixel_h / CELLFONT_FT_FONTPIXELSIZE * 0x10000L);

    FT_Set_Transform(sysfont.face, &matrix, 0);
    const FT_Error res = FT_Load_Glyph(sysfont.face, fontCode->value(), CELLFONT_FT_LOADFLAGS);
    if (res != FT_Err_Ok) {
        cellFont.error("Failed to load glyph: 0x%x", res);
        return CELL_FONT_ERROR_NO_SUPPORT_GLYPH;
    }

    // only use glyphslot for metrics
    const FT_GlyphSlot glyphSlot = sysfont.face->glyph;

    // these are in 26.6 fractional pixels
    metrics->width = static_cast<f32>(glyphSlot->metrics.width) / 64.f;
    metrics->height = static_cast<f32>(glyphSlot->metrics.height) / 64.f;
    metrics->h_advance = static_cast<f32>(glyphSlot->metrics.horiAdvance) / 64.f;
    metrics->h_bearingX = static_cast<f32>(glyphSlot->metrics.horiBearingX) / 64.f;
    metrics->h_bearingY = static_cast<f32>(glyphSlot->metrics.horiBearingY) / 64.f;
    metrics->v_advance = static_cast<f32>(glyphSlot->metrics.vertAdvance) / 64.f;
    // note, this is calculated as  -1 * (width / 2 ) by cellfont lib, leaving like this for now for testing;
    metrics->v_bearingX = static_cast<f32>(glyphSlot->metrics.vertBearingX) / 64.f;
    metrics->v_bearingY = static_cast<f32>(glyphSlot->metrics.vertBearingY) / 64.f;

    cellFont.error("advance x:%d, y:%d", glyphSlot->advance.x, glyphSlot->advance.y);

    cellFont.error("lva: %d, lha: %d", sysfont.face->glyph->linearVertAdvance, sysfont.face->glyph->linearHoriAdvance);

    FT_BBox cbox;
    FT_Outline_Get_BBox(&sysfont.face->glyph->outline, &cbox);

    cellFont.error("outcBox: xmax: %d, xmin: %d, ymax: %d, ymin:%d", cbox.xMax, cbox.xMin, cbox.yMax, cbox.yMin);

    font->glyphSlot = glyphSlot;

    return CELL_OK;
}


s32 cellFontRenderCharGlyphImage(ppu_thread& ppu, vm::ptr<CellFont> font, u32 code, vm::ptr<CellFontRenderSurface> surface, float x, float y, vm::ptr<CellFontGlyphMetrics> metrics, vm::ptr<CellFontImageTransInfo> transInfo)
{
    cellFont.notice("cellFontRenderCharGlyphImage(font=*0x%x, code=0x%x, surface=*0x%x, x=%f, y=%f, metrics=*0x%x, trans=*0x%x)", font, code, surface, x, y, metrics, transInfo);

    if (font == vm::null || metrics == vm::null || transInfo == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;
    if (font->renderer_addr == vm::null)
        return CELL_FONT_ERROR_RENDERER_UNBIND;
    if (code == 0)
        return CELL_FONT_ERROR_NO_SUPPORT_CODE;

    font->lib->ppuContext = &ppu;

    const s32 ret = cellFontGetCharGlyphMetrics(ppu, font, code, metrics);
    if (ret != CELL_OK)
        return ret;

    // still not fully sure how scale's are related...
    cellFont.error("rScaleW: %f, rScaleH: %f, scaleW: %f, scaleH: %f", font->renderer_addr->scale_w, font->renderer_addr->scale_h, font->pixel_w, font->pixel_h);
    //if (font->renderer_addr->scale_h != 0.f && (font->renderer_addr->scale_h != font->scale_h))
    //   cellFont.error("renderer scale_h does not match font scale_h!");

    //if (font->renderer_addr->scale_w != 0.f && (font->renderer_addr->scale_w != font->scale_w))
    //  cellFont.error("renderer scale_w does not match font scale_w!");

    const f32 lineHeight = font->renderer_addr->scale_h != 0 ? font->renderer_addr->scale_h : font->pixel_h;

    FT_Vector origin;
    origin.x = static_cast<u32>(x * 64.f);
    origin.y = static_cast<u32>(-(lineHeight + y) * 64.f);

    FT_Glyph loadedGlyph;
    FT_Error res = FT_Get_Glyph(font->glyphSlot, &loadedGlyph);
    if (res)
    {
        FT_Done_Glyph(loadedGlyph);
        cellFont.error("Failed to get glyph bitmap: 0x%x", res);
        return CELL_FONT_ERROR_FATAL;
    }

    res = FT_Glyph_To_Bitmap(&loadedGlyph, FT_RENDER_MODE_LIGHT, &origin, 1);
    if (res)
    {
        FT_Done_Glyph(loadedGlyph);
        cellFont.error("Failed to get glyph bitmap: 0x%x", res);
        return CELL_FONT_ERROR_FATAL;
    }
    FT_BBox box;

    FT_Glyph_Get_CBox(loadedGlyph, FT_GLYPH_BBOX_TRUNCATE, &box);
    cellFont.error("glyphBox: xmax: %d, xmin: %d, ymax: %d, ymin:%d", box.xMax, box.xMin, box.yMax, box.yMin);


    FT_BitmapGlyph castedGlyph = (FT_BitmapGlyph)loadedGlyph;

    cellFont.error("castGlyph: left %d, top %d, rows: %d, width: %d", castedGlyph->left, castedGlyph->top, castedGlyph->bitmap.rows, castedGlyph->bitmap.width);

    // not sure how renderer vs font weight should play into this, lets just take the one thats not '1.f'
    FT_Pos effectWeight = 64.f;
    if (font->effectWeight != 1.f && font->renderer_addr->effectWeight != 1.f && font->effectWeight != font->renderer_addr->effectWeight)
    {
        cellFont.error("Both font and renderer effectWeight aren't 0! using renderer weight");
        effectWeight = font->renderer_addr->effectWeight * 64.f;
    }
    else if (font->effectWeight != 1.f)
    {
        effectWeight = font->effectWeight * 64.f;
    }
    else if (font->renderer_addr->effectWeight != 1.f)
    {
        effectWeight = font->renderer_addr->effectWeight * 64.f;
    }

    effectWeight -= 64.f;

    if ((effectWeight >= (0.93f * 64.f)) && (effectWeight <= (64.f * 1.04f)))
    {
        res = FT_Bitmap_Embolden(font->lib->ftLibrary, &castedGlyph->bitmap, effectWeight, effectWeight);
        if (res)
        {
            FT_Done_Glyph(loadedGlyph);
            cellFont.error("Failed to embolden glyph bitmap: 0x%x", res);
            return CELL_FONT_ERROR_FATAL;
        }
    }
    // alloc buffer if needed
    const u32 neededBufSize = castedGlyph->bitmap.pitch * castedGlyph->bitmap.rows;
    if (font->renderer_addr->currentBufferSize < neededBufSize || font->renderer_addr->buffer == vm::null)
    {
        if (font->renderer_addr->maxBufferSize != 0 && neededBufSize > font->renderer_addr->maxBufferSize)
        {
            cellFont.error("cellFontRenderCharGlyphImage(): Needed buffer size larger than max! needed: %d, max %d", neededBufSize, font->renderer_addr->maxBufferSize);
            return CELL_FONT_ERROR_ENOUGH_RENDERING_BUFFER;
        }
        if (font->renderer_addr->buffer != vm::null)
            font->lib->MemoryIF.free(ppu, font->lib->MemoryIF.arg, font->renderer_addr->buffer);

        const auto allocAddr = font->lib->MemoryIF.malloc(ppu, font->lib->MemoryIF.arg, neededBufSize);

        if (allocAddr == vm::null)
        {
            cellFont.error("cellFontRenderCharGlyphImage(): Alloc buffer failed!");
            return CELL_FONT_ERROR_RENDERER_ALLOCATION_FAILED;
        }
        font->renderer_addr->buffer.set(allocAddr.addr());

        font->renderer_addr->currentBufferSize = neededBufSize;
    }

    // todo: it would be nice to have freetype already setup to render into the buffer rather than a temp bitmap buffer instead
    memcpy(font->renderer_addr->buffer.get_ptr(), castedGlyph->bitmap.buffer, castedGlyph->bitmap.rows * castedGlyph->bitmap.pitch);

    // now figure out offset into surface to place glyph
    const u32 xOffset = surface->pixelSizeByte * (castedGlyph->left);
    const u32 yOffset = surface->widthByte * (std::abs(castedGlyph->top) - 1);

    transInfo->image.set(font->renderer_addr->buffer.addr());
    transInfo->imageHeight = castedGlyph->bitmap.rows;
    transInfo->imageWidth = castedGlyph->bitmap.width;
    transInfo->imageWidthByte = castedGlyph->bitmap.pitch;
    transInfo->surface.set(surface->buffer.addr() + (xOffset)+(yOffset));
    transInfo->surfWidthByte = surface->widthByte;

    FT_Done_Glyph(loadedGlyph);

    return CELL_OK;
}

s32 cellFontGetResolutionDpi(vm::ptr<CellFont> font, vm::ptr<u32> hDpi, vm::ptr<u32> vDpi)
{
    cellFont.warning("cellFontGetResolutionDpi(font=0x%x, hDpi=0x%x, vDpi=0x%x )", font, hDpi, vDpi);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    if (hDpi != vm::null)
        *hDpi = font->hDpi;
    if (vDpi != vm::null)
        *vDpi = font->vDpi;
    return CELL_OK;
}

s32 cellFontSetResolutionDpi(vm::ptr<CellFont> font, u32 hDpi, u32 vDpi)
{
    cellFont.warning("cellFontSetResolutionDpi(font=0x%x, hDpi=%d, vDpi=%d)", font, hDpi, vDpi);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;
    font->hDpi = hDpi == 0 ? 72 : hDpi;
    font->vDpi = vDpi == 0 ? 72 : vDpi;
    return CELL_OK;
}

s32 cellFontSetScalePoint(vm::ptr<CellFont> font, f32 w, f32 h)
{
    cellFont.warning("cellFontSetScalePoint(font=0x%x, w=%f, h=%f)", font, w, h);

    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    font->point_w = w;
    font->point_h = h;

    font->pixel_h = (h * font->vDpi) / 72.f;
    font->pixel_w = (w * font->hDpi) / 72.f;

    return CELL_OK;
}

s32 cellFontGetScalePoint(vm::ptr<CellFont> font, vm::ptr<f32> w, vm::ptr<f32> h)
{
    cellFont.warning("cellFontGetScalePoint(font=0x%x, w=0x%x, h=0x%x )", font, w, h);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;
    if (w != vm::null)
        *w = font->point_w;
    if (h != vm::null)
        *h = font->point_h; 
    return CELL_OK;
}

s32 cellFontSetScalePixel(vm::ptr<CellFont> font, f32 w, f32 h)
{
    cellFont.warning("cellFontSetScalePixel(font=*0x%x, w=%f, h=%f)", font, w, h);

    font->pixel_h = w;
    font->pixel_w = h;

    font->point_h = (h * 72.f) / font->vDpi;
    font->point_w = (w * 72.f) / font->hDpi;

    return CELL_OK;
}

s32 cellFontGetScalePixel(vm::ptr<CellFont> font, vm::ptr<f32> w, vm::ptr<f32> h)
{
    cellFont.warning("cellFontGetScalePixel(font=0x%x, w=0x%x, h=0x%x )", font, w, h);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;
    if (w != vm::null)
        *w = font->pixel_w;
    if (h != vm::null)
        *h = font->pixel_h;
    return CELL_OK;
}

s32 cellFontSetupRenderEffectWeight(vm::ptr<CellFont> font, f32 weightParam)
{
    cellFont.warning("cellFontSetupRenderEffectWeight(font=0x%x, weightParam=%f)", font, weightParam);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    if (font->renderer_addr == vm::null)
        return CELL_FONT_ERROR_RENDERER_UNBIND;

    font->renderer_addr->effectWeight = weightParam < 0.93 ? 0.93 :
                                        weightParam > 1.04 ? 1.04 : weightParam;

    return CELL_OK;
}

s32 cellFontSetEffectWeight(vm::ptr<CellFont> font, f32 weightParam)
{
    cellFont.warning("cellFontSetEffectWeight(font=0x%x, weightParam=%f)", font, weightParam);
    font->effectWeight = weightParam < 0.93 ? 0.93 : 
                         weightParam > 1.04 ? 1.04 : weightParam;

    return CELL_OK;
}

s32 cellFontSetupRenderEffectSlant(vm::ptr<CellFont> font, f32 slant)
{
    cellFont.warning("cellFontSetupRenderEffectSlant(font=0x%x, slant=%f)", font, slant);
    if (font == vm::null)
        return CELL_FONT_ERROR_INVALID_PARAMETER;

    if (font->renderer_addr == vm::null)
        return CELL_FONT_ERROR_RENDERER_UNBIND;

    font->renderer_addr->slant = slant < -1.f ? -1.f :
        slant > 1.f ? 1.f : slant;

    return CELL_OK;
}

s32 cellFontGraphicsSetFontRGBA()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGraphicsSetScalePixel()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGraphicsGetScalePixel()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGlyphSetupVertexesGlyph()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGetVerticalLayout()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGetRenderCharGlyphMetricsVertical()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGraphicsSetLineRGBA()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGraphicsSetDrawType()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontEndGraphics()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGraphicsSetupDrawContext()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGlyphGetOutlineControlDistance()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGlyphGetVertexesGlyphSize()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGenerateCharGlyph()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontDeleteGlyph()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

//
struct CellFontExtendData2 {
    vm::bptr<void> extendFunc1;
};

struct CellFontExtendData {
    be_t<s32> stubFlags;
    be_t<u32> arg1;
    vm::bptr<CellFontExtendData2> data2;
};

s32 cellFontExtendFunc1(u32 arg1, u32 arg2) 
{
    cellFont.warning("cellFontExtendFunc1(a1=0x%x, a2=0x%x)", arg1, arg2);
    return 0;
}

s32 cellFontExtend(ppu_thread &ppu, u32 a1, u32 a2, vm::ptr<CellFontExtendData> a3)
{
    // undocumented function...
    cellFont.warning("cellFontExtend(a1=0x%x, a2=0x%x, a3=0x%x)", a1, a2, a3);

    if (a1 != 0xcfe00000 || a2 != 0 || a3 == vm::null)
        return CELL_FONT_ERROR_NO_SUPPORT_FUNCTION;

    if (a3->stubFlags == 0 || a3->stubFlags < 0)
        return CELL_OK;

    a3->stubFlags = 0x42;
    a3->data2 = vm::ptr<CellFontExtendData2>::make(vm::alloc(sizeof(CellFontExtendData2), vm::main));

	return CELL_OK;
}

s32 cellFontRenderCharGlyphImageVertical()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGetCharGlyphMetricsVertical()
{
	UNIMPLEMENTED_FUNC(cellFont);
	return CELL_OK;
}

s32 cellFontGetRenderEffectWeight()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGraphicsGetDrawType()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetKerning()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetRenderScaledKerning()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetRenderScalePixel()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphGetScalePixel()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphGetHorizontalShift()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontRenderCharGlyphImageHorizontal()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetEffectWeight()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontClearFileCache()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontAdjustFontScaling()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontSetupRenderScalePoint()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphGetVerticalShift()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetGlyphExpandBufferInfo()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetLibrary()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontVertexesGlyphRelocate()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetInitializedRevisionFlags()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphRenderImageVertical()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphRenderImageHorizontal()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontAdjustGlyphExpandBuffer()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetRenderScalePoint()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGraphicsGetFontRGBA()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphGetOutlineVertexes()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontDelete()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontPatchWorks()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGlyphRenderImage()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetBindingRenderer()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGenerateCharGlyphVertical()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGetRenderEffectSlant()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellFontGraphicsGetLineRGBA()
{
	fmt::throw_exception("Unimplemented" HERE);
}


DECLARE(ppu_module_manager::cellFont)("cellFont", []()
{
	static ppu_static_module cell_FreeType2("cell_FreeType2");

	REG_FUNC(cellFont, cellFontSetFontsetOpenMode);
	REG_FUNC(cellFont, cellFontSetFontOpenMode);
	REG_FUNC(cellFont, cellFontCreateRenderer);
	REG_FUNC(cellFont, cellFontGetHorizontalLayout);
	REG_FUNC(cellFont, cellFontDestroyRenderer);
	REG_FUNC(cellFont, cellFontSetupRenderScalePixel);
	REG_FUNC(cellFont, cellFontOpenFontInstance);
	REG_FUNC(cellFont, cellFontSetScalePixel);
	REG_FUNC(cellFont, cellFontGetRenderCharGlyphMetrics);
	REG_FUNC(cellFont, cellFontEndLibrary);
	REG_FUNC(cellFont, cellFontBindRenderer);
	REG_FUNC(cellFont, cellFontEnd);
	REG_FUNC(cellFont, cellFontSetEffectSlant);
	REG_FUNC(cellFont, cellFontGetEffectSlant);
	REG_FUNC(cellFont, cellFontRenderCharGlyphImage);
	REG_FUNC(cellFont, cellFontRenderSurfaceInit);
	REG_FUNC(cellFont, cellFontGetFontIdCode);
	REG_FUNC(cellFont, cellFontOpenFontset);
	REG_FUNC(cellFont, cellFontCloseFont);
	REG_FUNC(cellFont, cellFontRenderSurfaceSetScissor);
	REG_FUNC(cellFont, cellFontGetCharGlyphMetrics);
	REG_FUNC(cellFont, cellFontInitializeWithRevision);
	REG_FUNC(cellFont, cellFontGraphicsSetFontRGBA);
	REG_FUNC(cellFont, cellFontOpenFontsetOnMemory);
	REG_FUNC(cellFont, cellFontOpenFontFile);
	REG_FUNC(cellFont, cellFontGraphicsSetScalePixel);
	REG_FUNC(cellFont, cellFontGraphicsGetScalePixel);
	REG_FUNC(cellFont, cellFontSetEffectWeight);
	REG_FUNC(cellFont, cellFontGlyphSetupVertexesGlyph);
	REG_FUNC(cellFont, cellFontGetVerticalLayout);
	REG_FUNC(cellFont, cellFontGetRenderCharGlyphMetricsVertical);
	REG_FUNC(cellFont, cellFontSetScalePoint);
	REG_FUNC(cellFont, cellFontSetupRenderEffectSlant);
	REG_FUNC(cellFont, cellFontGraphicsSetLineRGBA);
	REG_FUNC(cellFont, cellFontGraphicsSetDrawType);
	REG_FUNC(cellFont, cellFontEndGraphics);
	REG_FUNC(cellFont, cellFontGraphicsSetupDrawContext);
	REG_FUNC(cellFont, cellFontOpenFontMemory);
	REG_FUNC(cellFont, cellFontSetupRenderEffectWeight);
	REG_FUNC(cellFont, cellFontGlyphGetOutlineControlDistance);
	REG_FUNC(cellFont, cellFontGlyphGetVertexesGlyphSize);
	REG_FUNC(cellFont, cellFontGenerateCharGlyph);
	REG_FUNC(cellFont, cellFontDeleteGlyph);
	REG_FUNC(cellFont, cellFontExtend);
	REG_FUNC(cellFont, cellFontRenderCharGlyphImageVertical);
	REG_FUNC(cellFont, cellFontSetResolutionDpi);
	REG_FUNC(cellFont, cellFontGetCharGlyphMetricsVertical);
	REG_FUNC(cellFont, cellFontUnbindRenderer);
	REG_FUNC(cellFont, cellFontGetRevisionFlags);
	REG_FUNC(cellFont, cellFontGetRenderEffectWeight);
	REG_FUNC(cellFont, cellFontGraphicsGetDrawType);
	REG_FUNC(cellFont, cellFontGetKerning);
	REG_FUNC(cellFont, cellFontGetRenderScaledKerning);
	REG_FUNC(cellFont, cellFontGetRenderScalePixel);
	REG_FUNC(cellFont, cellFontGlyphGetScalePixel);
	REG_FUNC(cellFont, cellFontGlyphGetHorizontalShift);
	REG_FUNC(cellFont, cellFontRenderCharGlyphImageHorizontal);
	REG_FUNC(cellFont, cellFontGetEffectWeight);
	REG_FUNC(cellFont, cellFontGetScalePixel);
	REG_FUNC(cellFont, cellFontClearFileCache);
	REG_FUNC(cellFont, cellFontAdjustFontScaling);
	REG_FUNC(cellFont, cellFontSetupRenderScalePoint);
	REG_FUNC(cellFont, cellFontGlyphGetVerticalShift);
	REG_FUNC(cellFont, cellFontGetGlyphExpandBufferInfo);
	REG_FUNC(cellFont, cellFontGetLibrary);
	REG_FUNC(cellFont, cellFontVertexesGlyphRelocate);
	REG_FUNC(cellFont, cellFontGetInitializedRevisionFlags);
	REG_FUNC(cellFont, cellFontGetResolutionDpi);
	REG_FUNC(cellFont, cellFontGlyphRenderImageVertical);
	REG_FUNC(cellFont, cellFontGlyphRenderImageHorizontal);
	REG_FUNC(cellFont, cellFontAdjustGlyphExpandBuffer);
	REG_FUNC(cellFont, cellFontGetRenderScalePoint);
	REG_FUNC(cellFont, cellFontGraphicsGetFontRGBA);
	REG_FUNC(cellFont, cellFontGlyphGetOutlineVertexes);
	REG_FUNC(cellFont, cellFontDelete);
	REG_FUNC(cellFont, cellFontPatchWorks);
	REG_FUNC(cellFont, cellFontGlyphRenderImage);
	REG_FUNC(cellFont, cellFontGetBindingRenderer);
	REG_FUNC(cellFont, cellFontGenerateCharGlyphVertical);
	REG_FUNC(cellFont, cellFontGetRenderEffectSlant);
	REG_FUNC(cellFont, cellFontGetScalePoint);
	REG_FUNC(cellFont, cellFontGraphicsGetLineRGBA);
});
