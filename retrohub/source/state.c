#include "retrohub.h"

#include <stdio.h>
#include <string.h>

static int find_path(const RHPathEntry *entries, size_t count, const char *path)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (strcmp(entries[i].path, path) == 0) return (int)i;
    return -1;
}

void rh_state_init(RHState *state)
{
    memset(state, 0, sizeof(*state));
}

int rh_state_is_favorite(const RHState *state, const char *game_path)
{
    return find_path(state->favorites, state->favorite_count, game_path) >= 0;
}

int rh_state_toggle_favorite(RHState *state, const char *game_path)
{
    int at = find_path(state->favorites, state->favorite_count, game_path);
    size_t i;

    if (at >= 0) {
        for (i = (size_t)at; i + 1 < state->favorite_count; ++i)
            state->favorites[i] = state->favorites[i + 1];
        state->favorite_count--;
        return 0;
    }

    if (state->favorite_count >= RH_MAX_FAVORITES) return -1;
    snprintf(state->favorites[state->favorite_count].path,
             sizeof(state->favorites[state->favorite_count].path), "%s", game_path);
    state->favorite_count++;
    return 1;
}

void rh_state_touch_recent(RHState *state, const char *game_path)
{
    int at = find_path(state->recents, state->recent_count, game_path);
    size_t i;

    if (at == 0) return;
    if (at > 0) {
        for (i = (size_t)at; i > 0; --i)
            state->recents[i] = state->recents[i - 1];
        snprintf(state->recents[0].path, sizeof(state->recents[0].path), "%s", game_path);
        return;
    }

    if (state->recent_count < RH_MAX_RECENTS) state->recent_count++;
    for (i = state->recent_count - 1; i > 0; --i)
        state->recents[i] = state->recents[i - 1];
    snprintf(state->recents[0].path, sizeof(state->recents[0].path), "%s", game_path);
}

int rh_state_recent_rank(const RHState *state, const char *game_path)
{
    return find_path(state->recents, state->recent_count, game_path);
}

int rh_state_load(RHState *state, const char *path)
{
    FILE *f;
    char line[RH_PATH_MAX + 8];

    rh_state_init(state);
    f = fopen(path, "rb");
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        char *p;
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        if (len < 3 || line[1] != '|') continue;
        p = line + 2;
        if (line[0] == 'F' && state->favorite_count < RH_MAX_FAVORITES) {
            { size_t n = strlen(p); if (n >= RH_PATH_MAX) n = RH_PATH_MAX - 1;
              memcpy(state->favorites[state->favorite_count].path, p, n);
              state->favorites[state->favorite_count].path[n] = 0; state->favorite_count++; }
        } else if (line[0] == 'R' && state->recent_count < RH_MAX_RECENTS) {
            { size_t n = strlen(p); if (n >= RH_PATH_MAX) n = RH_PATH_MAX - 1;
              memcpy(state->recents[state->recent_count].path, p, n);
              state->recents[state->recent_count].path[n] = 0; state->recent_count++; }
        }
    }
    fclose(f);
    return 1;
}

int rh_state_save(const RHState *state, const char *path)
{
    FILE *f = fopen(path, "wb");
    size_t i;
    if (!f) return 0;
    for (i = 0; i < state->favorite_count; ++i)
        fprintf(f, "F|%s\n", state->favorites[i].path);
    for (i = 0; i < state->recent_count; ++i)
        fprintf(f, "R|%s\n", state->recents[i].path);
    fclose(f);
    return 1;
}
