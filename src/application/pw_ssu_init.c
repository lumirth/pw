#include "startup/hardware.h"
#include "application/pw_ssu_init.h"

/* Prepare the shared serial bus and deselect its devices before assigning the
 * serial pins to the SSU. Until then the pins remain under GPIO control. */
void HardwareSetup(void)
{
  CKSTPR2.BYTE |= 0x10;   /* ungate the SSU peripheral clock */
  SSU.SSCRL.BYTE |= 0x40; /* select SSU mode */
  SSU.SSMR.BYTE = SSU_MODE3_MAIN_DIV4;
  SSU.SSCRH.BYTE = 0x8c; /* master, plus select/clock polarity */
  IO.PUCR9.BYTE = 8;     /* P93 pull-up */
  IO.PCR9 = 1;           /* accelerometer select output */
  IO.PCR1 = 7;           /* LCD select, LCD command/data, EEPROM select */
  IO.PDR1.BYTE = 5;      /* deselect LCD/EEPROM; select LCD command mode */
  IO.PDR9.BYTE |= 1;     /* deselect accelerometer */
  IO.PMRB.BYTE = 1;      /* hand the pins to the SSU last */
}
