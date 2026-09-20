#include "retrohub.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef __PSL1GHT__
#include <lv2/sysfs.h>
#else
#include <dirent.h>
#endif

static int rh_stricmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}


static const char *rh_file_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    return (dot && dot[1]) ? dot + 1 : NULL;
}

static int rh_system_index_by_id(const char *id)
{
    size_t i;
    for (i = 0; i < rh_system_count; ++i)
        if (strcmp(rh_systems[i].id, id) == 0) return (int)i;
    return -1;
}

static int rh_detect_mixed_system(const char *filename)
{
    const char *e = rh_file_extension(filename);
    if (!e) return -1;

    if (!rh_stricmp(e,"nes") || !rh_stricmp(e,"fds") || !rh_stricmp(e,"unf") || !rh_stricmp(e,"unif")) return rh_system_index_by_id("nes");
    if (!rh_stricmp(e,"smc") || !rh_stricmp(e,"sfc") || !rh_stricmp(e,"fig") || !rh_stricmp(e,"swc")) return rh_system_index_by_id("snes");
    if (!rh_stricmp(e,"sms")) return rh_system_index_by_id("sms");
    if (!rh_stricmp(e,"gg")) return rh_system_index_by_id("gamegear");
    if (!rh_stricmp(e,"md") || !rh_stricmp(e,"gen") || !rh_stricmp(e,"smd")) return rh_system_index_by_id("genesis");
    if (!rh_stricmp(e,"gb") || !rh_stricmp(e,"gbc")) return rh_system_index_by_id("gb");
    if (!rh_stricmp(e,"gba")) return rh_system_index_by_id("gba");
    if (!rh_stricmp(e,"a26")) return rh_system_index_by_id("atari2600");
    if (!rh_stricmp(e,"a78")) return rh_system_index_by_id("atari7800");
    if (!rh_stricmp(e,"lnx")) return rh_system_index_by_id("lynx");
    if (!rh_stricmp(e,"pce")) return rh_system_index_by_id("pce");
    if (!rh_stricmp(e,"cue") || !rh_stricmp(e,"chd") || !rh_stricmp(e,"iso")) return rh_system_index_by_id("segacd");
    if (!rh_stricmp(e,"ngp") || !rh_stricmp(e,"ngc")) return rh_system_index_by_id("ngp");
    if (!rh_stricmp(e,"ws") || !rh_stricmp(e,"wsc")) return rh_system_index_by_id("wswan");
    if (!rh_stricmp(e,"vb") || !rh_stricmp(e,"vboy")) return rh_system_index_by_id("virtualboy");
    if (!rh_stricmp(e,"vec")) return rh_system_index_by_id("vectrex");
    if (!rh_stricmp(e,"mx1") || !rh_stricmp(e,"mx2") || !rh_stricmp(e,"cas")) return rh_system_index_by_id("msx");
    if (!rh_stricmp(e,"sg")) return rh_system_index_by_id("sg1000");
    if (!rh_stricmp(e,"a52")) return rh_system_index_by_id("atari5200");
    if (!rh_stricmp(e,"col")) return rh_system_index_by_id("coleco");
    if (!rh_stricmp(e,"int")) return rh_system_index_by_id("intv");
    if (!rh_stricmp(e,"chf")) return rh_system_index_by_id("channelf");
    if (!rh_stricmp(e,"mgw")) return rh_system_index_by_id("gw");
    if (!rh_stricmp(e,"min")) return rh_system_index_by_id("pokemini");
    if (!rh_stricmp(e,"sgx")) return rh_system_index_by_id("supergrafx");
    if (!rh_stricmp(e,"cpr") || !rh_stricmp(e,"cdt")) return rh_system_index_by_id("gx4000");

    return -1;
}

int rh_extension_matches(const char *filename, const char *extensions)
{
    const char *dot = strrchr(filename, '.');
    const char *p = extensions;
    char ext[24];
    size_t n = 0;

    if (!dot || !dot[1]) return 0;
    dot++;

    while (1) {
        if (*p == '|' || *p == '\0') {
            ext[n] = '\0';
            if (n && rh_stricmp(dot, ext) == 0) return 1;
            n = 0;
            if (*p == '\0') break;
        } else if (n + 1 < sizeof(ext)) {
            ext[n++] = *p;
        }
        p++;
    }
    return 0;
}

void rh_pretty_name(const char *path, char *out, size_t out_size)
{
    const char *base = strrchr(path, '/');
    const char *dot;
    size_t len;

    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    len = dot ? (size_t)(dot - base) : strlen(base);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, base, len);
    out[len] = '\0';
}

void rh_catalog_init(RHCatalog *catalog)
{
    catalog->count = 0;
}

static int rh_has_path(const RHCatalog *catalog, const char *path)
{
    size_t i;
    for (i = 0; i < catalog->count; ++i)
        if (strcmp(catalog->games[i].path, path) == 0) return 1;
    return 0;
}

static int rh_add_game(RHCatalog *catalog, int system_index, const char *path)
{
    RHGame *g;
    if (catalog->count >= RH_MAX_GAMES) return -1;
    if (rh_has_path(catalog, path)) return 0;
    g = &catalog->games[catalog->count++];
    memset(g, 0, sizeof(*g));
    g->system_index = system_index;
    snprintf(g->path, sizeof(g->path), "%s", path);
    rh_pretty_name(path, g->name, sizeof(g->name));
    return 0;
}

#ifdef __PSL1GHT__
static int rh_scan_dir(RHCatalog *catalog, int system_index, const char *dir_path, const char *extensions)
{
    s32 fd;
    u64 read = 0;
    sysFSDirent entry;
    char full[RH_PATH_MAX];

    if (sysFsOpendir(dir_path, &fd) != 0) return 0;
    while (sysFsReaddir(fd, &entry, &read) == 0 && read > 0) {
        if (!strcmp(entry.d_name, ".") || !strcmp(entry.d_name, "..")) continue;
        {
            size_t a = strlen(dir_path), b = strlen(entry.d_name);
            if (a + 1 + b + 1 > sizeof(full)) continue;
            memcpy(full, dir_path, a); full[a] = '/';
            memcpy(full + a + 1, entry.d_name, b + 1);
        }
        if (rh_extension_matches(entry.d_name, extensions))
            rh_add_game(catalog, system_index, full);
    }
    sysFsClosedir(fd);
    return 0;
}
#else
static int rh_scan_dir(RHCatalog *catalog, int system_index, const char *dir_path, const char *extensions)
{
    DIR *dir = opendir(dir_path);
    struct dirent *entry;
    char full[RH_PATH_MAX];
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        {
            size_t a = strlen(dir_path), b = strlen(entry->d_name);
            if (a + 1 + b + 1 > sizeof(full)) continue;
            memcpy(full, dir_path, a); full[a] = '/';
            memcpy(full + a + 1, entry->d_name, b + 1);
        }
        if (rh_extension_matches(entry->d_name, extensions))
            rh_add_game(catalog, system_index, full);
    }
    closedir(dir);
    return 0;
}
#endif


#ifdef __PSL1GHT__
static int rh_scan_mixed_dir(RHCatalog *catalog, const char *dir_path)
{
    s32 fd;
    u64 read = 0;
    sysFSDirent entry;
    char full[RH_PATH_MAX];

    if (sysFsOpendir(dir_path, &fd) != 0) return 0;
    while (sysFsReaddir(fd, &entry, &read) == 0 && read > 0) {
        int system_index;
        size_t a, b;
        if (!strcmp(entry.d_name, ".") || !strcmp(entry.d_name, "..")) continue;
        system_index = rh_detect_mixed_system(entry.d_name);
        if (system_index < 0) continue;

        a = strlen(dir_path); b = strlen(entry.d_name);
        if (a + 1 + b + 1 > sizeof(full)) continue;
        memcpy(full, dir_path, a); full[a] = '/';
        memcpy(full + a + 1, entry.d_name, b + 1);
        rh_add_game(catalog, system_index, full);
    }
    sysFsClosedir(fd);
    return 0;
}
#else
static int rh_scan_mixed_dir(RHCatalog *catalog, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    struct dirent *entry;
    char full[RH_PATH_MAX];
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        int system_index;
        size_t a, b;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        system_index = rh_detect_mixed_system(entry->d_name);
        if (system_index < 0) continue;

        a = strlen(dir_path); b = strlen(entry->d_name);
        if (a + 1 + b + 1 > sizeof(full)) continue;
        memcpy(full, dir_path, a); full[a] = '/';
        memcpy(full + a + 1, entry->d_name, b + 1);
        rh_add_game(catalog, system_index, full);
    }
    closedir(dir);
    return 0;
}
#endif

static int rh_game_cmp(const void *a, const void *b)
{
    const RHGame *ga = (const RHGame *)a;
    const RHGame *gb = (const RHGame *)b;
    int c = rh_stricmp(ga->name, gb->name);
    if (c) return c;
    return rh_stricmp(ga->path, gb->path);
}

static void rh_catalog_scan_one_root(RHCatalog *catalog, const char *base_path)
{
    size_t i;
    char path[RH_PATH_MAX];
    {
        size_t a = strlen(base_path);
        const char *mixed = "roms";
        size_t b = strlen(mixed);
        if (a + 1 + b + 1 <= sizeof(path)) {
            memcpy(path, base_path, a); path[a] = '/';
            memcpy(path + a + 1, mixed, b + 1);
            rh_scan_mixed_dir(catalog, path);
        }
    }
    for (i = 0; i < rh_system_count; ++i) {
        size_t a = strlen(base_path), b = strlen(rh_systems[i].rom_dir);
        if (a + 1 + b + 1 > sizeof(path)) continue;
        memcpy(path, base_path, a); path[a] = '/';
        memcpy(path + a + 1, rh_systems[i].rom_dir, b + 1);
        rh_scan_dir(catalog, (int)i, path, rh_systems[i].extensions);
    }
}

int rh_catalog_scan(RHCatalog *catalog, const char *base_path)
{
    rh_catalog_init(catalog);
    rh_catalog_scan_one_root(catalog, base_path);
    qsort(catalog->games, catalog->count, sizeof(catalog->games[0]), rh_game_cmp);
    return (int)catalog->count;
}

int rh_catalog_scan_roots(RHCatalog *catalog, const char *const *roots, size_t root_count)
{
    size_t i;
    rh_catalog_init(catalog);
    for (i = 0; i < root_count; ++i)
        if (roots[i] && roots[i][0]) rh_catalog_scan_one_root(catalog, roots[i]);
    qsort(catalog->games, catalog->count, sizeof(catalog->games[0]), rh_game_cmp);
    return (int)catalog->count;
}
