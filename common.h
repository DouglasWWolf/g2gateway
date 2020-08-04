//=================================================================================================
// common.h - Definitions commonly used by multiple modules
//=================================================================================================
#pragma once

// Specs from the config file
#define SPEC_INTERFACE      "INTERFACE"
#define SPEC_EEPROM_DEVICE  "EEPROM_DEVICE"
#define SPEC_EEPROM_IS_FILE "EEPROM_IS_FILE"
#define SPEC_SANDBOX        "SANDBOX"
#define SPEC_LOCK_FS        "LOCK_FS"

// Specs from the EEPROM
#define SPEC_INSTRUMENT_SN  "INSTRUMENT_SN"
#define SPEC_DEFAULT_IP     "DEFAULT_IP"

// When we operating as our own gateway, the module appears
// to be in this slot.  (slots are number 0 thru 3)
#define ASSUMED_SLOT        0
