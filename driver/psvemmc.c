#include "psvemmc.h"
 
//taihen plugin by yifanlu was used as a reference:
//https://github.com/yifanlu/taiHEN/blob/master/taihen.c
 
#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/net/net.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <taihen.h>
#include <module.h>

#include "sector_api.h"

#include "debug.h"       //hook init
#include "glog.h"
#include "dump.h"        //some dumping functions
#include "thread_test.h" //to try get current thread id
#include "net.h"    //to init net
#include "mtable.h" //to init tables

//=================================================

//defalut size of sector for SD MMC protocol
#define SD_DEFAULT_SECTOR_SIZE 0x200

sd_context_part* g_emmcCtx = 0;

sd_context_part* g_gcCtx = 0;

int g_emmcCtxInitialized = 0;

int g_gcCtxInitialized = 0;

int g_msCtxInitialized = 0;

int g_bytesPerSector = 0;
int g_sectorsPerCluster = 0;

int g_clusterPoolInitialized = 0;

SceUID g_clusterPool = 0;

void* g_clusterPoolPtr = 0;

//this function initializes a pool for single cluster
//this way I hope to save some time on free/malloc operations
//when I need to read single cluster

//this method should be called after partition table is read
//so that we know excatly bytesPerSector and sectorsPerCluster

int psvemmcInitialize(int bytesPerSector, int sectorsPerCluster)
{
  if(g_clusterPoolInitialized != 0)
    return -1;
  
  if(bytesPerSector != SD_DEFAULT_SECTOR_SIZE)
    return -2;
  
  g_bytesPerSector = bytesPerSector;
  g_sectorsPerCluster = sectorsPerCluster;
  
  g_clusterPool = ksceKernelAllocMemBlock("cluster_pool_kernel", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, g_bytesPerSector * g_sectorsPerCluster, 0);
  if(g_clusterPool < 0)
    return g_clusterPool;
  
  int res_1 = ksceKernelGetMemBlockBase(g_clusterPool, &g_clusterPoolPtr);
  if(res_1 < 0)
    return res_1;
  
  g_clusterPoolInitialized = 1;

  return 0;
}

int psvemmcDeinitialize()
{
  if(g_clusterPoolInitialized == 0)
    return -1;
  
  int res_1 = ksceKernelFreeMemBlock(g_clusterPool);
  if(res_1 < 0)
    return res_1;
  
  g_clusterPoolInitialized = 0;
  
  return 0;
}

//this reads 0x200 byte sector
//other size is not supported since this is a restriction for single packet of SD MMC protocol

int readSector(int sector, char* buffer)
{
  if(g_emmcCtxInitialized == 0)
    return -1;
  
  char buffer_kernel[SD_DEFAULT_SECTOR_SIZE];
  
  int res_1 = ksceSdifReadSectorAsync(g_emmcCtx, sector, buffer_kernel, 1);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, buffer_kernel, SD_DEFAULT_SECTOR_SIZE);
  if(res_2 < 0)
    return res_2;
  
  return 0;
}

//this reads 0x200 byte sector

int readSectorGc(int sector, char* buffer)
{
  if(g_gcCtxInitialized == 0)
    return -1;
  
  char buffer_kernel[SD_DEFAULT_SECTOR_SIZE];
  
  int res_1 = ksceSdifReadSectorAsync(g_gcCtx, sector, buffer_kernel, 1);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, buffer_kernel, SD_DEFAULT_SECTOR_SIZE);
  if(res_2 < 0)
    return res_2;
  
  return 0;
}

//this reads 0x200 byte sector

int readSectorMs(int sector, char* buffer)
{
  if(g_msCtxInitialized == 0)
    return -1;
  
  char buffer_kernel[SD_DEFAULT_SECTOR_SIZE];
  
  int res_1 = ksceMsifReadSector(sector, buffer_kernel, 1);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, buffer_kernel, SD_DEFAULT_SECTOR_SIZE);
  if(res_2 < 0)
    return res_2;
  
  return 0;
}

//this writes 0x200 byte sector
//other size is not supported since this is a restriction for single packet of SD MMC protocol

int writeSector(int sector, char* buffer)
{
  if(g_emmcCtxInitialized == 0)
    return -1;
  
  //not implemented
  return -2;
}

//this writes 0x200 byte sector

int writeSectorGc(int sector, char* buffer)
{
  if(g_gcCtxInitialized == 0)
    return -1;
  
  //not implemented
  return -2;
}

//this writes 0x200 byte sector

int writeSectorMs(int sector, char* buffer)
{
  if(g_msCtxInitialized == 0)
    return -1;
  
  //not implemented
  return -2;
}

//this function reads single cluster
//number of clusters and size of sector should be taken from partition table
//however size of sector other than 0x200 is not currently expected

//to change sector size I need to set block size with CMD16 SET_BLOCKLEN
//however currently I do not know how to execute single SD EMMC commands

//I should also mention that not all card types support block len change

int readCluster(int cluster, char* buffer)
{
  if(g_emmcCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  int res_1 = ksceSdifReadSectorAsync(g_emmcCtx, g_sectorsPerCluster * cluster, g_clusterPoolPtr, g_sectorsPerCluster);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, g_clusterPoolPtr, g_bytesPerSector * g_sectorsPerCluster);
  if(res_2 < 0)
    return res_2;

  return 0;
}

int readClusterGc(int cluster, char* buffer)
{
  if(g_gcCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  int res_1 = ksceSdifReadSectorAsync(g_gcCtx, g_sectorsPerCluster * cluster, g_clusterPoolPtr, g_sectorsPerCluster);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, g_clusterPoolPtr, g_bytesPerSector * g_sectorsPerCluster);
  if(res_2 < 0)
    return res_2;

  return 0;
}

int readClusterMs(int cluster, char* buffer)
{
  if(g_msCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  int res_1 = ksceMsifReadSector(g_sectorsPerCluster * cluster, g_clusterPoolPtr, g_sectorsPerCluster);
  if(res_1 < 0)
    return res_1;
  
  int res_2 = ksceKernelMemcpyKernelToUser((uintptr_t)buffer, g_clusterPoolPtr, g_bytesPerSector * g_sectorsPerCluster);
  if(res_2 < 0)
    return res_2;

  return 0;
}

//this function writes single cluster
//number of clusters and size of sector should be taken from partition table
//however size of sector other than 0x200 is not currently expected

//to change sector size I need to set block size with CMD16 SET_BLOCKLEN
//however currently I do not know how to execute single SD EMMC commands

//I should also mention that not all card types support block len change

int writeCluster(int cluster, char* buffer)
{
  if(g_emmcCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  //not implemented
  return -3;
}

int writeClusterGc(int cluster, char* buffer)
{
  if(g_gcCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  //not implemented
  return -3;
}

int writeClusterMs(int cluster, char* buffer)
{
  if(g_msCtxInitialized == 0)
    return -1;
  
  if(g_clusterPoolInitialized == 0)
    return -2;
  
  //not implemented
  return -3;
}

//=================================================

int initialize_emmc_globals()
{
  g_emmcCtx = ksceSdifGetSdContextPartMmc(SCE_SDIF_DEV_EMMC);
  if(g_emmcCtx == 0)
  {
    if(ksceSdifInitializeSdContextPartMmc(SCE_SDIF_DEV_EMMC, &g_emmcCtx) < 0)
    {
      g_emmcCtxInitialized = 0;
      return -1;
    }
    else
    {
      g_emmcCtxInitialized = 1;
      return 0;
    }
  }
  else
  {
    g_emmcCtxInitialized = 1;
    return 0;
  }
}

int initialize_gc_mmc_device()
{
  g_gcCtx = ksceSdifGetSdContextPartMmc(SCE_SDIF_DEV_GAME_CARD);
  if(g_gcCtx == 0)
  {
    if(ksceSdifInitializeSdContextPartMmc(SCE_SDIF_DEV_GAME_CARD, &g_gcCtx) < 0)
    {
      g_gcCtxInitialized = 0;
      return -1;
    }
    else
    {
      g_gcCtxInitialized = 1;
      return 0;
    }
  }
  else
  {
    g_gcCtxInitialized = 1;
    return 0;
  }
}

int initialize_gc_sd_device()
{
  g_gcCtx = ksceSdifGetSdContextPartSd(SCE_SDIF_DEV_GAME_CARD);
  if(g_gcCtx == 0)
  {
    //currently I do not know how initialization should be triggered for sd device
    
    g_gcCtxInitialized = 0;
    return -1;
  }
  else
  {
    g_gcCtxInitialized = 1;
    return 0;
  }
}

int initialize_gc_globals()
{
  //to be able to switch between sd and mmc cards - we need to aquire type of device from global context
  //before querying context part
  //cartridge must be plugged before henkaku loads this driver
  
  sd_context_global* gctx = ksceSdifGetSdContextGlobal(SCE_SDIF_DEV_GAME_CARD);
  if(gctx->ctx_data.dev_type_idx == 1) // mmc type
    return initialize_gc_mmc_device();
  else if(gctx->ctx_data.dev_type_idx == 2) // sd type
    return initialize_gc_sd_device();
  else
    return -1;
}

int initialize_ms_globals()
{
  //currently I do not know how to initialize ms
  //there are at least two functions that must be called
  
  g_msCtxInitialized = 1;
  return 0;
}

//==============================================

int dump_block_devices()
{
  call_proc_get_mount_data_C15B80("ext-pp-act-a");       // 1 1 0 1  : sd0:  
  call_proc_get_mount_data_C15B80("ext-lp-act-entire");  // 1 0 f 1  : sd0:

  call_proc_get_mount_data_C15B80("int-lp-act-os");      // 0 0 3 1  : os0: 0x3 - 003

  call_proc_get_mount_data_C15B80("int-lp-ign-vsh");     // 0 0 4 2  : vs0: 0x4 - 007

  call_proc_get_mount_data_C15B80("int-lp-ign-vshdata"); // 0 0 5 2  : vd0: 0x5 - 008
  call_proc_get_mount_data_C15B80("int-lp-ign-vtrm");    // 0 0 6 2  : tm0: 0x6 - 006
  call_proc_get_mount_data_C15B80("int-lp-ign-user");    // 0 0 7 2  : ur0: 0x7 - 011
  call_proc_get_mount_data_C15B80("int-lp-ign-updater"); // 0 0 b 2  : ud0: 0xB - 009
  call_proc_get_mount_data_C15B80("xmc-lp-ign-userext"); // ff 0 8 2 : ux0: 0x8 - 
  call_proc_get_mount_data_C15B80("int-lp-ign-userext"); // 0 0 8 2  : ux0: 0x8 - 012

  call_proc_get_mount_data_C15B80("gcd-lp-ign-gamero");  // 1 0 9 2  : gro0: 0x9 - 
  call_proc_get_mount_data_C15B80("gcd-lp-ign-gamerw");  // 1 0 a 2  : grw0: 0xA - 

  call_proc_get_mount_data_C15B80("int-lp-ign-sysdata"); // 0 0 c 2  : sa0: 0xC - 005
  call_proc_get_mount_data_C15B80("int-lp-ign-pidata");  // 0 0 e 2  : pd0: 0xE - 010

  call_proc_get_mount_data_C15B80("uma-pp-act-a");       // 3 1 0 1  : uma0:
  call_proc_get_mount_data_C15B80("uma-lp-act-entire");  // 3 0 f 1  : uma0:
  
  call_proc_get_mount_data_C15B80("gcd-lp-act-mediaid"); // 1 0 d 1  : external : 0xD - 
  call_proc_get_mount_data_C15B80("int-lp-act-entire");  // 0 0 f 1  :  ?       : ?   - 016

  call_proc_get_mount_data_C15B80("mcd-lp-act-mediaid"); // 2 0 d 1  : external : 0xD - 200

  call_proc_get_mount_data_C15B80("mcd-lp-act-entire");  // 2 0 f 1 : ?

  call_proc_get_mount_data_C15B80("int-lp-ina-os");      // 0 0 3 0 : os0: 0x3 - 003
  call_proc_get_mount_data_C15B80("int-lp-ign-os");      // 0 0 3 2 : os0: 0x3 - 003

  call_proc_get_mount_data_C15B80("int-lp-ign-idstor");  // 0 0 1 2 : ?    0x1 - 000
  call_proc_get_mount_data_C15B80("int-lp-ign-sloader"); // 0 0 2 2 : ?    0x2 - 001

  return 0;

  //second level

  /*
  str_ptr_00C1A238 DCD 0xC1A318 ;	"lp-"
  str_ptr_00C1A23C DCD 0xC1A31C ;	"pp-"
  */

  //forth level

  /*
  str_ptr_00C1A240 DCD 0xC1A320 ;	"unused"
  str_ptr_00C1A244 DCD 0xC1A328 ;	"idstor"
  str_ptr_00C1A248 DCD 0xC1A330 ;	"sloader"
  str_ptr_00C1A24C DCD 0xC1A338 ;	"os"
  str_ptr_00C1A250 DCD 0xC1A33C ;	"vsh"
  str_ptr_00C1A254 DCD 0xC1A340 ;	"vshdata"
  str_ptr_00C1A258 DCD 0xC1A348 ;	"vtrm"
  str_ptr_00C1A25C DCD 0xC1A350 ;	"user"
  str_ptr_00C1A260 DCD 0xC1A358 ;	"userext"
  str_ptr_00C1A264 DCD 0xC1A360 ;	"gamero"
  str_ptr_00C1A268 DCD 0xC1A368 ;	"gamerw"
  str_ptr_00C1A26C DCD 0xC1A370 ;	"updater"
  str_ptr_00C1A270 DCD 0xC1A378 ;	"sysdata"
  str_ptr_00C1A274 DCD 0xC1A380 ;	"mediaid"
  str_ptr_00C1A278 DCD 0xC1A388 ;	"pidata"
  str_ptr_00C1A27C DCD 0xC1A390 ;	"entire"
  */

  //third level

  /*
  aIna DCB "ina-"
  aAct DCB "act-"
  aIgn DCB "ign-"
  */

  //first level

  /*
  str_ptr_C1A2C0 DCD 0xC1A2E0 ; "int-"
  str_ptr_C1A2C4 DCD 0xC1A2E8 ; "ext-"
  str_ptr_C1A2C8 DCD 0xC1A2F0 ; "gcd-"
  str_ptr_C1A2CC DCD 0xC1A2F8 ; "mcd-"
  str_ptr_C1A2D0 DCD 0xC1A300 ; "uma-"
  str_ptr_C1A2D4 DCD 0xC1A308 ; "usd-"
  str_ptr_C1A2D8 DCD 0xC1A310 ; "xmc-"
  */

  /*
  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A2E0
  aInt DCB "int-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A2E4
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A2E8
  aExt DCB "ext-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A2EC
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A2F0
  aGcd DCB "gcd-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A2F4
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A2F8
  aMcd DCB "mcd-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A2FC
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A300
  aUma DCB "uma-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A304
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A308
  aUsd DCB "usd-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A30C
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A310
  aXmc DCB "xmc-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A314
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A318
  aLp DCB	"lp-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A31B
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A31C
  aPp DCB	"pp-"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A31F
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A320
  aUnused	DCB "unused"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A326
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A328
  aIdstor	DCB "idstor"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A32E
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A330
  aSloader DCB "sloader"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A337
  DCB    0
  aOs DCB	"os",0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A33C
  aVsh DCB "vsh"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A33F
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A340
  aVshdata DCB "vshdata"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A347
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A348
  aVtrm DCB "vtrm"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A34C
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A350
  aUser DCB "user"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A354
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A358
  aUserext DCB "userext"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A35F
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A360
  aGamero	DCB "gamero"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A366
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A368
  aGamerw	DCB "gamerw"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A36E
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A370
  aUpdater DCB "updater"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A377
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A378
  aSysdata DCB "sysdata"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A37F
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A380
  aMediaid DCB "mediaid"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A387
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A388
  aPidata	DCB "pidata"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A38E
  DCB    0
  DCB    0
  ; unknown ends


  ; Segment type:	Pure data
  AREA string, DATA, ABS
  ORG 0xC1A390
  aEntire	DCB "entire"
  ; string ends


  ; Segment type:	Regular
  AREA unknown, DATA, ABS
  ORG 0xC1A396
  DCB    0
  DCB    0
  str_ptr_00C1A398 DCD 0xC1A288 ;	"ina-"
  str_ptr_00C1A39C DCD 0xC1A290 ;	"act-"
  str_ptr_00C1A3A0 DCD 0xC1A298 ;	"ign-"
  DCB    0
  DCB    0
  DCB    0
  DCB    0
  */
}

int module_start(SceSize argc, const void *args) 
{
  //initialize emmc if required
  initialize_emmc_globals();
  
  //initialize gc if required
  initialize_gc_globals();
  
  //initialize ms if required
  initialize_ms_globals();
  
  initialize_all_hooks();

  //deal with module table
  construct_module_range_table();
  sort_segment_table();
  //print_segment_table();
  
  //dump_vfs_data();

  //dump_vfs_node_info();

  //dump_block_devices();

  //print_thread_info();

  //init_net(); // DISABLE NET FOR NOW
  
  //dump_device_context_mem_blocks_1000();
  
  //dump_device_context_mem_blocks_10000();
  
  //dump_sdstor_data();
  //dump_sdif_data();
  
  //-------------------------------
  
  /*
  print_device_info_arrays();
  
  clear_device_info_arrays();
  
  initialize_gc_sd();
  
  print_device_info_arrays();
  */
  
  return SCE_KERNEL_START_SUCCESS;
}
 
//Alias to inhibit compiler warning
void _start() __attribute__ ((weak, alias ("module_start")));
 
int module_stop(SceSize argc, const void *args) 
{
  //deinit_net(); // DISABLE NET FOR NOW

  deinitialize_all_hooks();
  
  //deinitialize buffers if required
  psvemmcDeinitialize();
  
  return SCE_KERNEL_STOP_SUCCESS;
}
