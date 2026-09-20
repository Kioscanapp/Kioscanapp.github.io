#include "retrohub.h"

/*
 * Systems listed here are intentionally tied to cores that the current
 * libretro build scripts include for PSL1GHT, or to Genesis Plus GX for the
 * Sega systems it supports. PS1/PS2/N64/Saturn/Dreamcast are not exposed yet.
 */
const RHSystem rh_systems[] = {
    {"nes",      "Nintendo NES",          "roms/nes",      "nes|fds|unf|unif",       "cores/nestopia_libretro_psl1ght.SELF"},
    {"snes",     "Super Nintendo",        "roms/snes",     "smc|sfc|fig|swc",        "cores/snes9x2010_libretro_psl1ght.SELF"},
    {"sms",      "Master System",         "roms/sms",      "sms",                    "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"gamegear", "Sega Game Gear",        "roms/gamegear", "gg",                     "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"genesis",  "Sega Mega Drive",       "roms/genesis",  "md|gen|smd|bin",         "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"segacd",   "Sega CD",               "roms/segacd",   "cue|chd|iso",             "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"gb",       "Game Boy / Color",      "roms/gb",       "gb|gbc",                  "cores/gambatte_libretro_psl1ght.SELF"},
    {"gba",      "Game Boy Advance",      "roms/gba",      "gba",                     "cores/vba_next_libretro_psl1ght.SELF"},
    {"atari2600","Atari 2600",            "roms/atari2600","a26|bin",                 "cores/stella2014_libretro_psl1ght.SELF"},
    {"atari7800","Atari 7800",            "roms/atari7800","a78|bin",                 "cores/prosystem_libretro_psl1ght.SELF"},
    {"lynx",     "Atari Lynx",            "roms/lynx",     "lnx",                     "cores/handy_libretro_psl1ght.SELF"},
    {"pce",      "PC Engine / TurboGrafx", "roms/pce",      "pce|cue|chd|ccd|m3u",     "cores/mednafen_pce_fast_libretro_psl1ght.SELF"},
    {"ngp",      "Neo Geo Pocket",        "roms/ngp",      "ngp|ngc",                 "cores/mednafen_ngp_libretro_psl1ght.SELF"},
    {"wswan",    "WonderSwan",             "roms/wswan",    "ws|wsc",                  "cores/mednafen_wswan_libretro_psl1ght.SELF"},
    {"virtualboy","Virtual Boy",           "roms/virtualboy","vb|vboy|bin",            "cores/mednafen_vb_libretro_psl1ght.SELF"},
    {"vectrex",  "Vectrex",                "roms/vectrex",  "vec|bin",                 "cores/vecx_libretro_psl1ght.SELF"},
    {"msx",      "MSX / MSX2",            "roms/msx",      "rom|mx1|mx2|dsk|cas",     "cores/fmsx_libretro_psl1ght.SELF"},
    {"arcade",   "Arcade",                "roms/arcade",   "zip|7z",                  "cores/mame2003_libretro_psl1ght.SELF"},
    {"sg1000",   "Sega SG-1000",          "roms/sg1000",   "sg|sms|bin|rom",           "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"sega32x",  "Sega 32X",              "roms/sega32x",  "32x|bin|md|smd",           "cores/picodrive_libretro_psl1ght.SELF"},
    {"atari5200","Atari 5200",             "roms/atari5200","a52|bin|rom",              "cores/atari800_libretro_psl1ght.SELF"},
    {"coleco",   "ColecoVision",           "roms/coleco",   "col|rom|bin|zip",          "cores/bluemsx_libretro_psl1ght.SELF"},
    {"intv",     "Intellivision",          "roms/intv",     "int|bin|rom",              "cores/freeintv_libretro_psl1ght.SELF"},
    {"o2",       "Odyssey2 / Videopac",    "roms/o2",       "bin|rom",                  "cores/o2em_libretro_psl1ght.SELF"},
    {"channelf", "Fairchild Channel F",    "roms/channelf", "bin|rom|chf",              "cores/freechaf_libretro_psl1ght.SELF"},
    {"gw",       "Game & Watch",           "roms/gw",       "mgw|zip",                  "cores/gw_libretro_psl1ght.SELF"},
    {"pokemini", "Pokemon Mini",           "roms/pokemini", "min",                     "cores/pokemini_libretro_psl1ght.SELF"},
    {"supergrafx","PC Engine SuperGrafx",  "roms/supergrafx","pce|sgx|bin",             "cores/mednafen_supergrafx_libretro_psl1ght.SELF"},
    {"neogeo",   "Neo Geo AES",            "roms/neogeo",   "zip",                     "cores/fbalpha2012_neogeo_libretro_psl1ght.SELF"},
    {"neocd",    "Neo Geo CD",             "roms/neocd",    "cue|chd|iso",             "cores/neocd_libretro_psl1ght.SELF"},
    {"gx4000",   "Amstrad GX4000",         "roms/gx4000",   "cpr|dsk|cdt|m3u|zip",      "cores/cap32_libretro_psl1ght.SELF"},
};

const size_t rh_system_count = sizeof(rh_systems) / sizeof(rh_systems[0]);

const char *rh_system_name(int system_index)
{
    if (system_index < 0 || (size_t)system_index >= rh_system_count) return "Desconocido";
    return rh_systems[system_index].name;
}

const char *rh_system_core(int system_index)
{
    if (system_index < 0 || (size_t)system_index >= rh_system_count) return 0;
    return rh_systems[system_index].core_self;
}
