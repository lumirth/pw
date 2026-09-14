#ifndef PW_FACTORY_TEST_H
#define PW_FACTORY_TEST_H

#include "types.h"

/* A fixture handshake runs destructive EEPROM/RTC tests, streams motion
 * samples and calibrates the battery reference. Success sleeps the device;
 * failed EEPROM, RTC or calibration checks hang after reporting a code.
 * An unrecognized handshake returns to normal startup. */
void BootSelfTest(void);

#endif
