//
//
//  Otakutachi
//
//  Created by tihmstar on 08.05.24.
//

#include <stdint.h>
#include "bootloader.h"

#include "cartbus.h"
#include "lfs_hal.h"
#include "all.h"

#include "lfs.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <hardware/dma.h>
#include <hardware/sync.h>
#include <tusb.h>

#define ROM_ADDRESS_SPACE 0x1000
#define ROM_OFFSET_MASK    0xfff

static uint8_t atari_cart[0x10000+ROM_ADDRESS_SPACE] __attribute__ ((aligned(0x10000))); //64K ought to be enough for anybody
lfs_t gLFS = {};

void spinerror();

void *memmem(const void *l, size_t l_len, const void *s, size_t s_len){
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}

int __no_inline_not_in_flash_func(atari_bootdance)(void (*rom_handler_func)(void), bool canTimeout) {
  uint8_t *bootdance_rom = &atari_cart[sizeof(atari_cart)-ROM_ADDRESS_SPACE];

  for (size_t i = 0; i < ROM_ADDRESS_SPACE-4; i+=2){
    bootdance_rom[i+0] = 0x00;
    bootdance_rom[i+1] = 0x00;
  }
  

  bootdance_rom[ROM_ADDRESS_SPACE-4] = 0x74;
  bootdance_rom[ROM_ADDRESS_SPACE-3] = 0xFF;
  bootdance_rom[ROM_ADDRESS_SPACE-2] = 0x74;
  bootdance_rom[ROM_ADDRESS_SPACE-1] = 0xFF;

  uint32_t danceCounter = 0;
  uint16_t bootdance[5] = {0x1f76, 0x1f68, 0x1f74, 0x1f72, 0xF1F1};
  uint16_t addr = 0;
  uint32_t data = 0;

  bootdance[4] = *(uint16_t*)&atari_cart[ROM_ADDRESS_SPACE-4];

  setCarbusBlockAddress(bootdance_rom);
  cartbus_start();

  if (canTimeout){
    uint64_t time = time_us_64();
    for (size_t i = 0; i < 5; i++){
      while (pio_sm_is_rx_fifo_empty(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_CPU)){
        if (time_us_64() - time > USEC_PER_SEC*5){
          cartbus_cleanup();
          return 0;
        }
      }
      pio_sm_get(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_CPU);
    }
  }  
  

  while (true){
    addr = cartbus_readAddr_blocking();
    if ((addr & 0x1000) == 0) continue;

    int danceStage = (danceCounter++ & 7);

    if (danceStage <= 3){
      if (addr == 0x1ffe){
        danceCounter &= ~3;
        danceCounter += 4;
        continue;
      }else if (danceStage < 3){
        continue;
      }
    } else if (danceStage == 4){
      if (addr == 0x1fff) {
        *(uint16_t*)&bootdance_rom[ROM_ADDRESS_SPACE-2] = bootdance[(danceCounter>>3)+1];
        if ((danceCounter>>3) == 4) break;
        continue;
      }
    } else if (danceStage < 6){
      if (addr == bootdance[(danceCounter>>3)]) {
        danceCounter &= ~7;
        danceCounter += 8;
      }
      continue;
    }
    //restart boot dance
    danceCounter = 0;
    *(uint16_t*)&bootdance_rom[ROM_ADDRESS_SPACE-2] = bootdance[0]; 
  }
  setCarbusBlockAddress(atari_cart);
  rom_handler_func();
  return 0xff;
}

void __no_inline_not_in_flash_func(atari_boot_nodance)(void (*rom_handler_func)(void)) {
  setCarbusBlockAddress(atari_cart);
  cartbus_start();
  rom_handler_func();
}


void bootloader_print(uint8_t *line, uint8_t *lut, char *str){
  memset(line, lut[' '], 36);
  for (size_t i = 0; i < 36; i++){
    char c = str[i];
    if (!c) break;
    line[i] = lut[c];
  }
}

int getNumberOfRoms(){
  int ret = 0;
  int err = 0;
  int lfs_err = 0;
  lfs_dir_t dir = {};
  bool dirIsOpen = false;
  struct lfs_info lfsInfo = {};
  cassure((lfs_err = lfs_dir_open(&gLFS, &dir, "/")) == LFS_ERR_OK);
  dirIsOpen = true;
  while ((lfs_err = lfs_dir_read(&gLFS, &dir, &lfsInfo)) > 0){
    if (lfsInfo.type == LFS_TYPE_REG) {
      ret++;
    }
  }
error:;
  if (dirIsOpen) lfs_dir_close(&gLFS, &dir);
  return ret;
}

int bootloader_PutROMNameForIndex(uint8_t *line, uint8_t *lut, int idx){
  int curIdx = -1;
  int err = 0;
  int lfs_err = 0;
  lfs_dir_t dir = {};
  bool dirIsOpen = false;
  struct lfs_info lfsInfo = {};
  cassure((lfs_err = lfs_dir_open(&gLFS, &dir, "/")) == LFS_ERR_OK);
  dirIsOpen = true;
  while ((lfs_err = lfs_dir_read(&gLFS, &dir, &lfsInfo)) > 0){
    if (lfsInfo.type == LFS_TYPE_REG) {
      if (++curIdx == idx){
        bootloader_print(line, lut, lfsInfo.name);
      }
    }
  }
error:;
  if (dirIsOpen) lfs_dir_close(&gLFS, &dir);
  return curIdx;
}


void bootloader_handler(void) {
  int err = 0;
  uint8_t lut[0x100] = {};
  uint8_t *lines[10] = {};
  uint8_t *selLineLoc = NULL;
  uint16_t selLineLocAddr = 0;
  {
    uint8_t *magic = memmem(atari_cart, ROM_ADDRESS_SPACE, "tihmstar", sizeof("tihmstar")-1);
    cassure(magic);
    selLineLoc = magic-1;
    selLineLocAddr = ((selLineLoc-atari_cart) & 0xfff) | 0x1000;
  }

  for (size_t i = 0; i < 10; i++){
    lines[9-i] = selLineLoc-36*(i+1);
  }
  memset(lut, lines[1][0], sizeof(lut));
  for (uint8_t i = 0; i <= 9; i++){
    lut['0'+i] = lines[9][i];
  }

  for (uint8_t i = 'A'-'A'; i <= 'Z'-'A'; i++){
    lut['A'+i] = lines[9][10+i];
    lut['a'+i] = lines[9][10+i];
  }

  bootloader_print(lines[0], lut, "Otakutachi");

  int romCnt = getNumberOfRoms();
  for (size_t i = 0; i < romCnt && i<10; i++){
    bootloader_PutROMNameForIndex(lines[i], lut, i);
  }

  while (true){
    uint16_t addr = cartbus_readAddr_blocking();
    uint32_t data = cartbus_readData_blocking();

    static uint32_t dbuf[200] = {};

    if (addr == selLineLocAddr+1){
      //line selector
      // selLineLoc[0] = data;
      // selLineLoc[1] = data;
    }else if (addr == selLineLocAddr+2){
      //rom select trigger!
#warning TODO: try reading data from write cycle!
      printf("",data);
      for (int i=0; i<sizeof(dbuf)/sizeof(*dbuf); i++){
        dbuf[i] = cartbus_readData_blocking();
      }

       printf("",data,dbuf);
    }
  }
  
error:
  spinerror();
}

void spinerror(){
  while (true){
    gpio_put(25, 0);
    sleep_ms(1000);
    gpio_put(25, 1);
    sleep_ms(1000);
  }
}

int main() {
  // Set system clock speed.
  // 266 MHz (because shilga does this too)
  vreg_set_voltage(VREG_VOLTAGE_1_15);
  set_sys_clock_khz(266000, true); 

  // stdio_init_all();
  // printf("--- HELLO FROM OTAKUTACHI ---\n");

  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  gpio_put(25, 0);

  {
    int lfs_err = lfs_mount(&gLFS, &gLFS_pico_cfg);
    if (lfs_err != LFS_ERR_OK) {
      lfs_format(&gLFS, &gLFS_pico_cfg);
      lfs_err = lfs_mount(&gLFS, &gLFS_pico_cfg);
      if (lfs_err != LFS_ERR_OK) spinerror();
    }
  }

  gpio_put(25, 1);

  //atari stuff
  memcpy(atari_cart, bootloader, ROM_ADDRESS_SPACE);

  cartbus_setup();
  atari_bootdance(bootloader_handler, true);

  // setCarbusBlockAddress(atari_cart);
  // cartbus_start();
  // bootloader_handler();

  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  gpio_put(25, 0);  
  //usb stuff
  tusb_init();
  while (1){
    tud_task(); // tinyusb device task
  }
}