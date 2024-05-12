#ifndef CARTBUS_H
#define CARTBUS_H

#include <stdint.h>
#include <hardware/pio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIO_CARTBUS pio0
#define GPIO_FUNC_PIO_CARTBUS GPIO_FUNC_PIO0

#define SM_BUS_ADDR_OBSERVER_DMA 0
#define SM_BUS_ADDR_OBSERVER_CPU 1
#define SM_BUS_DATA_HANDLER 2


extern volatile uint32_t gCartbusBlockAddress;

void cartbus_setup ();
void cartbus_start();
void cartbus_stop();
void cartbus_cleanup(void);

static inline uint16_t cartbus_readAddr_blocking(void){
  // uint16_t ret;
  // while ((PIO_CARTBUS->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_BUS_ADDR_OBSERVER_CPU))) != 0) tight_loop_contents();
  // do{
  //   ret = PIO_CARTBUS->rxf[SM_BUS_ADDR_OBSERVER_CPU];
  // } while ((PIO_CARTBUS->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_BUS_ADDR_OBSERVER_CPU))) == 0);
  // return ret;
  return pio_sm_get_blocking(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_CPU);
}

static inline uint8_t cartbus_readData_blocking(void){
  return pio_sm_get_blocking(PIO_CARTBUS, SM_BUS_DATA_HANDLER);
}

static inline void setCarbusBlockAddress(const void *addr){
  gCartbusBlockAddress = (uint32_t)addr;
}

#ifdef __cplusplus
}
#endif

#endif // CARTBUS_H