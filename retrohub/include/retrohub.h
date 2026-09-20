#ifndef RETROHUB_H
#define RETROHUB_H

#include <stddef.h>

#define RH_MAX_SYSTEMS 32
#define RH_MAX_GAMES 8192
#define RH_PATH_MAX 512
#define RH_NAME_MAX 128
#define RH_MAX_FAVORITES 512
#define RH_MAX_RECENTS 32
#define RH_MAX_FILTERED RH_MAX_GAMES

typedef struct {
    const char *id;
    const char *name;
    const char *rom_dir;
    const char *extensions;
    const char *core_self;
} RHSystem;

typedef struct {
    char name[RH_NAME_MAX];
    char path[RH_PATH_MAX];
    int system_index;
} RHGame;

typedef struct {
    RHGame games[RH_MAX_GAMES];
    size_t count;
} RHCatalog;

typedef struct {
    char path[RH_PATH_MAX];
} RHPathEntry;

typedef struct {
    RHPathEntry favorites[RH_MAX_FAVORITES];
    size_t favorite_count;
    RHPathEntry recents[RH_MAX_RECENTS];
    size_t recent_count;
} RHState;

typedef enum {
    RH_VIEW_ALL = 0,
    RH_VIEW_FAVORITES,
    RH_VIEW_RECENTS,
    RH_VIEW_SYSTEM
} RHViewKind;

typedef struct {
    RHViewKind kind;
    int system_index;
} RHFilter;

extern const RHSystem rh_systems[];
extern const size_t rh_system_count;

void rh_catalog_init(RHCatalog *catalog);
int rh_catalog_scan(RHCatalog *catalog, const char *base_path);
int rh_catalog_scan_roots(RHCatalog *catalog, const char *const *roots, size_t root_count);
int rh_extension_matches(const char *filename, const char *extensions);
void rh_pretty_name(const char *path, char *out, size_t out_size);
const char *rh_system_name(int system_index);
const char *rh_system_core(int system_index);

void rh_state_init(RHState *state);
int rh_state_load(RHState *state, const char *path);
int rh_state_save(const RHState *state, const char *path);
int rh_state_is_favorite(const RHState *state, const char *game_path);
int rh_state_toggle_favorite(RHState *state, const char *game_path);
void rh_state_touch_recent(RHState *state, const char *game_path);
int rh_state_recent_rank(const RHState *state, const char *game_path);

size_t rh_filter_catalog(const RHCatalog *catalog, const RHState *state,
                         RHFilter filter, int *out_indices, size_t max_indices);

int rh_file_exists(const char *path);
int rh_core_available_ps3(int system_index);
int rh_launch_game_ps3(const RHGame *game);

#endif
