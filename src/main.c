//
//
//  Otakutachi
//
//  Created by tihmstar on 08.05.24.
//

#include <stdint.h>
#include "rom.h"

#include "cartbus.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <hardware/dma.h>
#include <hardware/sync.h>


#define ROM_ADDRESS_SPACE 0x1000
#define ROM_OFFSET_MASK    0xfff

static uint8_t atari_cart[0x10000+ROM_ADDRESS_SPACE] __attribute__ ((aligned(0x10000))); //64K ought to be enough for anybody

void __no_inline_not_in_flash_func(atari_bootdance)(void (*rom_handler_func)(void)) {
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
}

void __no_inline_not_in_flash_func(atari_boot_nodance)(void (*rom_handler_func)(void)) {
  setCarbusBlockAddress(atari_cart);
  cartbus_start();
  rom_handler_func();
}

void __no_inline_not_in_flash_func(rom_handler_F8)(void) {
  while (true){
    uint16_t addr = cartbus_readAddr_blocking();
    switch (addr){
      case 0x1ff8:
        setCarbusBlockAddress(&atari_cart[ROM_ADDRESS_SPACE*0]);
        break;

      case 0x1ff9:
        setCarbusBlockAddress(&atari_cart[ROM_ADDRESS_SPACE*1]);
        break;

      default:
        break;
    }
  }
}

void __no_inline_not_in_flash_func(rom_handler_nobank)(void) {
  while (true){
    __wfi();
  }
}

int main() {
  // Set system clock speed.
  // 266 MHz (because shilga does this too)
  vreg_set_voltage(VREG_VOLTAGE_1_15);
  set_sys_clock_khz(266000, true); 

  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  gpio_put(25, 1);

  memcpy(atari_cart, rom_contents, sizeof(rom_contents));
  if (sizeof(rom_contents) < ROM_ADDRESS_SPACE){
    memcpy(atari_cart+ROM_ADDRESS_SPACE-sizeof(rom_contents), rom_contents, sizeof(rom_contents));
  }
  
  cartbus_setup();


  if (sizeof(rom_contents) <= ROM_ADDRESS_SPACE){
    atari_bootdance(rom_handler_nobank);
  }else{
    atari_bootdance(rom_handler_F8);
  }



  // if (sizeof(rom_contents) <= ROM_ADDRESS_SPACE){
  //   atari_boot_nodance(rom_handler_nobank);
  // }else{
  //   atari_boot_nodance(rom_handler_F8);
  // }
  
  while (1) {
    tight_loop_contents();
  }
}