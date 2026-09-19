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
