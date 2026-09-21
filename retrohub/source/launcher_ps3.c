#include "retrohub.h"

#include <stdio.h>
#include <string.h>

#ifdef __PSL1GHT__
#include <sys/process.h>
#include <lv2/sysfs.h>
#else
#include <sys/stat.h>
#endif

#define RH_INSTALL_ROOT "/dev_hdd0/game/RVIC00001/USRDIR"
#define RH_CONFIG_PATH RH_INSTALL_ROOT "/retroarch.cfg"
#define RH_SYSTEM_DIR RH_INSTALL_ROOT "/system"
#define RH_SEGA_CD_BIOS_SIZE 131072L

static const char *rh_usb_roots[] = {
    "/dev_usb000/RETROVICIOS", "/dev_usb001/RETROVICIOS",
    "/dev_usb002/RETROVICIOS", "/dev_usb003/RETROVICIOS",
    "/dev_usb004/RETROVICIOS", "/dev_usb005/RETROVICIOS",
    "/dev_usb006/RETROVICIOS", "/dev_usb007/RETROVICIOS"
};

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

static long rh_file_size(const char *path)
{
    FILE *f=fopen(path,"rb");
    long n;
    if(!f)return -1;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return -1;}
    n=ftell(f);
    fclose(f);
    return n;
}

static int rh_copy_file(const char *src,const char *dst)
{
    FILE *in=fopen(src,"rb");
    FILE *out;
    unsigned char buf[16384];
    size_t got;

    if(!in)return -1;
    out=fopen(dst,"wb");
    if(!out){fclose(in);return -2;}

    while((got=fread(buf,1,sizeof(buf),in))>0){
        if(fwrite(buf,1,got,out)!=got){
            fclose(in);fclose(out);return -3;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static void rh_write_runtime_config(void)
{
    FILE *f=fopen(RH_CONFIG_PATH,"w");
    if(!f)return;

    fprintf(f,"menu_driver = \"rgui\"\n");
    fprintf(f,"config_save_on_exit = \"false\"\n");
    fprintf(f,"system_directory = \"%s/system\"\n",RH_INSTALL_ROOT);
    fprintf(f,"savefile_directory = \"%s/savefiles\"\n",RH_INSTALL_ROOT);
    fprintf(f,"savestate_directory = \"%s/savestates\"\n",RH_INSTALL_ROOT);
    fprintf(f,"screenshot_directory = \"%s/screenshots\"\n",RH_INSTALL_ROOT);
    fprintf(f,"input_menu_toggle_gamepad_combo = \"0\"\n");
    fprintf(f,"input_quit_gamepad_combo = \"4\"\n");
    fclose(f);
}

static int rh_import_one_bios(const char *bios_name)
{
    char dst[RH_PATH_MAX];
    char src[RH_PATH_MAX];
    size_t i;

    snprintf(dst,sizeof(dst),"%s/%s",RH_SYSTEM_DIR,bios_name);

    if(rh_file_exists(dst) && rh_file_size(dst)==RH_SEGA_CD_BIOS_SIZE)
        return 1;

    for(i=0;i<sizeof(rh_usb_roots)/sizeof(rh_usb_roots[0]);++i){
        snprintf(src,sizeof(src),"%s/system/%s",rh_usb_roots[i],bios_name);
        if(rh_file_exists(src) && rh_file_size(src)==RH_SEGA_CD_BIOS_SIZE){
            if(rh_copy_file(src,dst)==0)return 1;
        }
    }
    return 0;
}

static void rh_import_sega_cd_bios(void)
{
    rh_import_one_bios("bios_CD_U.bin");
    rh_import_one_bios("bios_CD_E.bin");
    rh_import_one_bios("bios_CD_J.bin");
}

int rh_core_available_ps3(int system_index)
{
    char core[RH_PATH_MAX];
    const char *relative_core=rh_system_core(system_index);
    if(!relative_core)return 0;
    snprintf(core,sizeof(core),"%s/%s",RH_INSTALL_ROOT,relative_core);
    return rh_file_exists(core);
}

int rh_launch_game_ps3(const RHGame *game)
{
#ifdef __PSL1GHT__
    char core[RH_PATH_MAX];
    const char *argv[2];
    const char *relative_core;

    if(!game)return -1;
    relative_core=rh_system_core(game->system_index);
    if(!relative_core)return -2;
    snprintf(core,sizeof(core),"%s/%s",RH_INSTALL_ROOT,relative_core);
    if(!rh_file_exists(core))return -3;
    if(!rh_file_exists(game->path))return -4;

    if(game->system_index>=0 &&
       strcmp(rh_systems[game->system_index].id,"segacd")==0){
        /* Import any user-supplied regional BIOS, but do not guess the
           disc region from the filename. Genesis Plus GX inspects the
           actual content and decides which BIOS it needs. */
        rh_import_sega_cd_bios();
    }

    rh_write_runtime_config();

    argv[0]=game->path;
    argv[1]=NULL;

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
