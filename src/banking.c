
#include "banking.h"

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