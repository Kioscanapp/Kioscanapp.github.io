#include "retrohub.h"

#include <stddef.h>
#include <string.h>

size_t rh_filter_catalog(const RHCatalog *catalog, const RHState *state,
                         RHFilter filter, int *out_indices, size_t max_indices)
{
    size_t n = 0, i;

    if (filter.kind == RH_VIEW_RECENTS) {
        size_t r;
        for (r = 0; r < state->recent_count && n < max_indices; ++r) {
            for (i = 0; i < catalog->count; ++i) {
                if (!strcmp(state->recents[r].path, catalog->games[i].path)) {
                    out_indices[n++] = (int)i;
                    break;
                }
            }
        }
        return n;
    }

    for (i = 0; i < catalog->count && n < max_indices; ++i) {
        const RHGame *g = &catalog->games[i];
        int include = 0;
        if (filter.kind == RH_VIEW_ALL) include = 1;
        else if (filter.kind == RH_VIEW_FAVORITES) include = rh_state_is_favorite(state, g->path);
        else if (filter.kind == RH_VIEW_SYSTEM) include = g->system_index == filter.system_index;
        if (include) out_indices[n++] = (int)i;
    }
    return n;
}
