#include "retrohub.h"

#include <stdio.h>
#include <string.h>

#ifdef __PSL1GHT__
#include <sys/process.h>
#include <lv2/sysfs.h>
#else
#include <sys/stat.h>
#endif

#define RH_INSTALL_ROOT "/dev_hdd0/game/RHUB00001/USRDIR"

int rh_file_exists(const char *path)
{
#ifdef __PSL1GHT__
    sysFSStat st;
    return sysFsStat(path, &st) == 0;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int rh_core_available_ps3(int system_index)
{
    char core[RH_PATH_MAX];
    const char *relative_core = rh_system_core(system_index);
    if (!relative_core) return 0;
    snprintf(core, sizeof(core), "%s/%s", RH_INSTALL_ROOT, relative_core);
    return rh_file_exists(core);
}

int rh_launch_game_ps3(const RHGame *game)
{
#ifdef __PSL1GHT__
    char core[RH_PATH_MAX];
    const char *argv[2];
    const char *relative_core;

    if (!game) return -1;
    relative_core = rh_system_core(game->system_index);
    if (!relative_core) return -2;
    snprintf(core, sizeof(core), "%s/%s", RH_INSTALL_ROOT, relative_core);
    if (!rh_file_exists(core)) return -3;
    if (!rh_file_exists(game->path)) return -4;

    /* Match RetroArch's current PS3 fork convention: the spawned core
       receives the content path as the argument list. */
    argv[0] = game->path;
    argv[1] = NULL;

    sysProcessExitSpawn2(core,
                         (const char **)argv,
                         NULL,
                         NULL,
                         0,
                         1000,
                         SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    return 0;
#else
    (void)game;
    return -1;
#endif
}
