#include "../include/cartbus.h"
#include <hardware/timer.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <stdio.h>

#include "cartbus.pio.h"

static int gDMAChannelCartAddressLoader = 0;
static int gDMAChannelBaseAddressLoader = 0;
static int gDMAChannelFetchTrigger = 0;
static int gDMAChannelDataFetcher = 0;

volatile uint32_t gCartbusBlockAddress = 0;
static volatile uint32_t _gDMADevNull;

static int gPioPCAddrObserverDMA = 0;
static int gPioPCAddrObserverCPU = 0;
static int gPioPCDataHandler = 0;
static int gPioPCDataReader = 0;

void cartbus_setup (){
  gDMAChannelCartAddressLoader = dma_claim_unused_channel(true);
  gDMAChannelBaseAddressLoader = dma_claim_unused_channel(true);
  gDMAChannelFetchTrigger = dma_claim_unused_channel(true);
  gDMAChannelDataFetcher = dma_claim_unused_channel(true);

  {
    /*
      Fetch ROMBANK offset from bus and write it to sniff_data
    */
    dma_channel_config c = dma_channel_get_default_config(gDMAChannelCartAddressLoader);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_DMA, false)); /* configure data request. true: sending data to the PIO state machine */
    channel_config_set_chain_to(&c, gDMAChannelBaseAddressLoader);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(gDMAChannelCartAddressLoader, &c,
                          &(dma_hw->sniff_data),                      //write target
                          &(PIO_CARTBUS->rxf[SM_BUS_ADDR_OBSERVER_DMA]),  //read source
                          1,    // always transfer one word (pointer)
                          false // do not trigger yet, will be done after all the
                                // other DMAs are setup
    );
  }

  {
    /*
      Perform addition using DMA to get correct ROM source address
    */
    dma_channel_config c = dma_channel_get_default_config(gDMAChannelBaseAddressLoader);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_sniff_enable(&c, true);
    channel_config_set_chain_to(&c, gDMAChannelFetchTrigger);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(gDMAChannelBaseAddressLoader, &c,
                          &_gDMADevNull,       //write target
                          &gCartbusBlockAddress,    //read source
                          1,    // always transfer one word (pointer)
                          false // do not trigger yet, will be done after all the
                                // other DMAs are setup
    );
    dma_sniffer_enable(gDMAChannelBaseAddressLoader, DMA_SNIFF_CTRL_CALC_VALUE_SUM, true);
  }

  {
    /*
      Tell data fetcher where to fetch data from and start it
    */
    dma_channel_config c = dma_channel_get_default_config(gDMAChannelFetchTrigger);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(gDMAChannelFetchTrigger, &c,
                          &dma_hw->ch[gDMAChannelDataFetcher].al3_read_addr_trig,//write target
                          &(dma_hw->sniff_data),                     //read source
                          1,    // always transfer one word (pointer)
                          false // do not trigger yet, will be done after all the
                                // other DMAs are setup
    );
  }

  {
    /*
      Finally fetch data and give it to handler PIO
    */
    dma_channel_config c = dma_channel_get_default_config(gDMAChannelDataFetcher);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, pio_get_dreq(PIO_CARTBUS, SM_BUS_DATA_HANDLER, true)); /* configure data request. true: sending data to the PIO state machine */
    channel_config_set_chain_to(&c, gDMAChannelCartAddressLoader);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(gDMAChannelDataFetcher, &c,
                          &(PIO_CARTBUS->txf[SM_BUS_DATA_HANDLER]),  //write target
                          NULL,                                      //read source (dummy)
                          1,    // always transfer one word (pointer)
                          false // do not trigger yet, will be done after all the
                                // other DMAs are setup
    );
  }

  for (int i=PIN_A0; i<=PIN_A12; i++){
    gpio_set_function(i, GPIO_FUNC_PIO_CARTBUS);
  }

  for (int i=PIN_D0; i<=PIN_D7; i++){
    gpio_set_function(i, GPIO_FUNC_PIO_CARTBUS);
  }

  gPioPCAddrObserverDMA = pio_add_program(PIO_CARTBUS, &cartbus_addr_observer_dma_program);
  gPioPCAddrObserverCPU = pio_add_program(PIO_CARTBUS, &cartbus_addr_observer_cpu_program);
  gPioPCDataHandler = pio_add_program(PIO_CARTBUS, &cartbus_data_handler_program);
  gPioPCDataReader = pio_add_program(PIO_CARTBUS, &cartbus_data_reader_program);

  cartbus_addr_observer_dma_program_init(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_DMA, gPioPCAddrObserverDMA);
  cartbus_addr_observer_cpu_program_init(PIO_CARTBUS, SM_BUS_ADDR_OBSERVER_CPU, gPioPCAddrObserverCPU);
  cartbus_data_handle_program_init(PIO_CARTBUS, SM_BUS_DATA_HANDLER, gPioPCDataHandler);
  cartbus_data_reader_program_init(PIO_CARTBUS, SM_BUS_DATA_READER, gPioPCDataReader);

}

void cartbus_start(){
  for (int i=0; i<4; i++){
    pio_sm_set_enabled(PIO_CARTBUS, i, false);
    pio_sm_clear_fifos(PIO_CARTBUS, i);
    pio_sm_restart(PIO_CARTBUS, i);
    pio_sm_clkdiv_restart(PIO_CARTBUS, i);
  }

  pio_set_sm_mask_enabled(
    PIO_CARTBUS,
    (1 << SM_BUS_ADDR_OBSERVER_DMA) | (1 << SM_BUS_ADDR_OBSERVER_CPU) | (1 << SM_BUS_DATA_HANDLER) | (1 << SM_BUS_DATA_READER),
    true
  );
  dma_channel_start(gDMAChannelCartAddressLoader);
}

void cartbus_stop(){
  pio_set_sm_mask_enabled(
    PIO_CARTBUS,
    (1 << SM_BUS_ADDR_OBSERVER_DMA) | (1 << SM_BUS_ADDR_OBSERVER_CPU) | (1 << SM_BUS_DATA_HANDLER) | (1 << SM_BUS_DATA_READER),
    false
  );
}

void cartbus_cleanup(void){
  cartbus_stop();
  for (int i=0; i<4; i++){
    pio_sm_set_enabled(PIO_CARTBUS, i, false);
    pio_sm_clear_fifos(PIO_CARTBUS, i);
    pio_sm_restart(PIO_CARTBUS, i);
    pio_sm_clkdiv_restart(PIO_CARTBUS, i);
  }

  pio_remove_program(PIO_CARTBUS, &cartbus_addr_observer_dma_program, gPioPCAddrObserverDMA);
  pio_remove_program(PIO_CARTBUS, &cartbus_addr_observer_cpu_program, gPioPCAddrObserverCPU);
  pio_remove_program(PIO_CARTBUS, &cartbus_data_handler_program, gPioPCDataHandler);

  if (gDMAChannelCartAddressLoader) dma_channel_unclaim(gDMAChannelCartAddressLoader);
  if (gDMAChannelBaseAddressLoader) dma_channel_unclaim(gDMAChannelBaseAddressLoader);
  if (gDMAChannelFetchTrigger) dma_channel_unclaim(gDMAChannelFetchTrigger);
  if (gDMAChannelDataFetcher) dma_channel_unclaim(gDMAChannelDataFetcher);
}