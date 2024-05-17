

#include "lfs_hal.h"

#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/regs/addressmap.h>

#define LFS_CACHE_SIZE (FLASH_SECTOR_SIZE / 4)


#define FS_SIZE (1.2 * 1024 * 1024)
#define LOOK_AHEAD_SIZE 32

static uint8_t readBuffer[LFS_CACHE_SIZE];
static uint8_t programBuffer[LFS_CACHE_SIZE];
static uint8_t lookaheadBuffer[LOOK_AHEAD_SIZE] __attribute__((aligned(4)));

static uint32_t fs_base(const struct lfs_config *c) {
    uint32_t storage_size = c->block_count * c->block_size;
    return PICO_FLASH_SIZE_BYTES - storage_size;
}

static int pico_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size){
    uint8_t* p = (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE + fs_base(c) + (block * FLASH_SECTOR_SIZE) + off);
    memcpy(buffer, p, size);
    return LFS_ERR_OK;
}

static int pico_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size){
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(fs_base(c) + (block * FLASH_SECTOR_SIZE) + off, buffer, size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int pico_erase(const struct lfs_config* c, lfs_block_t block) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(fs_base(c) + block * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int pico_sync(const struct lfs_config* c) {
    (void)c;
    return LFS_ERR_OK;
}

const struct lfs_config gLFS_pico_cfg = {
  .read  = pico_read,
  .prog  = pico_prog,
  .erase = pico_erase,
  .sync  = pico_sync,
  .read_size      = 1,
  .prog_size      = FLASH_PAGE_SIZE,
  .block_size     = FLASH_SECTOR_SIZE,
  .block_count    = FS_SIZE / FLASH_SECTOR_SIZE,
  .cache_size     = LFS_CACHE_SIZE,
  .lookahead_size = LOOK_AHEAD_SIZE,
  .block_cycles   = 500,
  .read_buffer    = readBuffer,
  .prog_buffer    = programBuffer,
  .lookahead_buffer = lookaheadBuffer
};
