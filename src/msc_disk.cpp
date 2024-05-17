
#include "EmuFATFS.hpp"


#include "tusb.h"
#include "tusb_config.h"
#include "class/msc/msc.h"
#include <pico/bootrom.h>

#pragma mark global vars
static bool ejected = false;
static bool fatIsInited = false;
static int gIsIOInProgress = 0;

static tihmstar::EmuFATFS<35,0x400> gEmuFat("OTAKUTACHI",CFG_TUD_MSC_EP_BUFSIZE);


void cb_newFile(const char *filename, const char filenameSuffix[3], uint32_t fileSize){
  if (strncasecmp(filenameSuffix, "UF2",3) == 0){
    reset_usb_boot(0,0);
  }
}

void init_fakefatfs(void){
  gEmuFat.resetFiles();
  gEmuFat.registerNewfileCallback(cb_newFile);

  fatIsInited = true;
}

#pragma mark tusb cb functions
// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]){
  (void) lun;

  const char vid[] = "tihmstar";
  const char pid[] = "Otakutachi";
  const char rev[] = "1.0";

  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun){
  (void) lun;

  // RAM disk is ready until ejected
  if (ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  if (!gIsIOInProgress){
    gIsIOInProgress++;

    if (!fatIsInited){
      init_fakefatfs();
    }

    gIsIOInProgress--;
  }
  
  return fatIsInited;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void) lun;

  *block_count = gEmuFat.diskBlockNum();
  *block_size  = gEmuFat.diskBlockSize();
}


// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject){
  (void) lun;
  (void) power_condition;

  if ( load_eject )
  {
    if (start)
    {
      // load disk storage
    }else
    {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize){
  (void) lun;

  int32_t didread = 0;
  if (!gIsIOInProgress){
    gIsIOInProgress++;
    didread =  gEmuFat.hostRead(offset + lba * gEmuFat.diskBlockSize(), buffer, bufsize);
    gIsIOInProgress--;
  }

  return didread & ~(gEmuFat.diskBlockSize()-1);
}

bool tud_msc_is_writable_cb (uint8_t lun) {
  (void) lun;
  return true;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize){
  (void) lun;

  int32_t didWrite = 0;
  if (!gIsIOInProgress){
    gIsIOInProgress++;
    didWrite = gEmuFat.hostWrite(offset + lba * gEmuFat.diskBlockSize(), buffer, bufsize);
    gIsIOInProgress--;
  }

  return didWrite & ~(gEmuFat.diskBlockSize()-1);
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t) resplen);
    }else
    {
      // SCSI output
    }
  }

  return (int32_t) resplen;
}