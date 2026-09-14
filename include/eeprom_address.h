#ifndef PW_EEPROM_ADDRESS_H
#define PW_EEPROM_ADDRESS_H

#include "types.h"

/* Compute a serial EEPROM byte address for the driver from a record layout. */
#define PW_EEPROM_MEMBER_ADDRESS(base, type, member)                           \
  ((u16) & ((type *)(base))->member)

#endif
