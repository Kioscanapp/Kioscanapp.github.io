#include "retrohub.h"

/*
 * Systems listed here are intentionally tied to cores that the current
 * libretro build scripts include for PSL1GHT, or to Genesis Plus GX for the
 * Sega systems it supports. PS1/PS2/N64/Saturn/Dreamcast are not exposed yet.
 */
const RHSystem rh_systems[] = {
    {"nes",      "Nintendo NES",          "roms/nes",      "nes|fds|unf|unif|zip|7z",       "cores/nestopia_libretro_psl1ght.SELF"},
    {"snes",     "Super Nintendo",        "roms/snes",     "smc|sfc|fig|swc|zip|7z",        "cores/snes9x2010_libretro_psl1ght.SELF"},
    {"sms",      "Master System",         "roms/sms",      "sms|zip|7z",                    "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"gamegear", "Sega Game Gear",        "roms/gamegear", "gg|zip|7z",                     "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"genesis",  "Sega Mega Drive",       "roms/genesis",  "md|gen|smd|bin|zip|7z",         "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"segacd",   "Sega CD",               "roms/segacd",   "cue|chd|iso",                    "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"gb",       "Game Boy / Color",      "roms/gb",       "gb|gbc|zip|7z",                  "cores/gambatte_libretro_psl1ght.SELF"},
    {"gba",      "Game Boy Advance",      "roms/gba",      "gba|zip|7z",                     "cores/vba_next_libretro_psl1ght.SELF"},
    {"atari2600","Atari 2600",            "roms/atari2600","a26|bin|zip|7z",                 "cores/stella2014_libretro_psl1ght.SELF"},
    {"atari7800","Atari 7800",            "roms/atari7800","a78|bin|zip|7z",                 "cores/prosystem_libretro_psl1ght.SELF"},
    {"lynx",     "Atari Lynx",            "roms/lynx",     "lnx|zip|7z",                     "cores/handy_libretro_psl1ght.SELF"},
    {"pce",      "PC Engine / TurboGrafx", "roms/pce",      "pce|cue|chd|ccd|m3u|zip|7z",     "cores/mednafen_pce_fast_libretro_psl1ght.SELF"},
    {"ngp",      "Neo Geo Pocket",        "roms/ngp",      "ngp|ngc|zip|7z",                 "cores/mednafen_ngp_libretro_psl1ght.SELF"},
    {"wswan",    "WonderSwan",             "roms/wswan",    "ws|wsc|zip|7z",                  "cores/mednafen_wswan_libretro_psl1ght.SELF"},
    {"virtualboy","Virtual Boy",           "roms/virtualboy","vb|vboy|bin|zip|7z",            "cores/mednafen_vb_libretro_psl1ght.SELF"},
    {"vectrex",  "Vectrex",                "roms/vectrex",  "vec|bin|zip|7z",                 "cores/vecx_libretro_psl1ght.SELF"},
    {"msx",      "MSX / MSX2",            "roms/msx",      "rom|mx1|mx2|dsk|cas|zip|7z",     "cores/fmsx_libretro_psl1ght.SELF"},
    {"arcade",   "Arcade",                "roms/arcade",   "zip|7z",                  "cores/mame2003_libretro_psl1ght.SELF"},
    {"sg1000",   "Sega SG-1000",          "roms/sg1000",   "sg|sms|bin|rom|zip|7z",           "cores/genesis_plus_gx_libretro_psl1ght.SELF"},
    {"atari5200","Atari 5200",             "roms/atari5200","a52|bin|rom|zip|7z",              "cores/atari800_libretro_psl1ght.SELF"},
    {"coleco",   "ColecoVision",           "roms/coleco",   "col|rom|bin|zip|7z",          "cores/bluemsx_libretro_psl1ght.SELF"},
    {"intv",     "Intellivision",          "roms/intv",     "int|bin|rom|zip|7z",              "cores/freeintv_libretro_psl1ght.SELF"},
    {"o2",       "Odyssey2 / Videopac",    "roms/o2",       "bin|rom|zip|7z",                  "cores/o2em_libretro_psl1ght.SELF"},
    {"channelf", "Fairchild Channel F",    "roms/channelf", "bin|rom|chf|zip|7z",              "cores/freechaf_libretro_psl1ght.SELF"},
    {"gw",       "Game & Watch",           "roms/gw",       "mgw|zip|7z",                  "cores/gw_libretro_psl1ght.SELF"},
    {"pokemini", "Pokemon Mini",           "roms/pokemini", "min|zip|7z",                     "cores/pokemini_libretro_psl1ght.SELF"},
    {"supergrafx","PC Engine SuperGrafx",  "roms/supergrafx","pce|sgx|bin|zip|7z",             "cores/mednafen_supergrafx_libretro_psl1ght.SELF"},
    {"gx4000",   "Amstrad GX4000",         "roms/gx4000",   "cpr|dsk|cdt|m3u|zip|7z",      "cores/cap32_libretro_psl1ght.SELF"},
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
