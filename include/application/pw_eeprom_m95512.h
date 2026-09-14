#ifndef PW_EEPROM_M95512_H
#define PW_EEPROM_M95512_H

#include "types.h"
#include "records.h"

/* Shared M95512 command encoding and transaction retry limit. */
#define EEPROM_CMD_PAGE_PROGRAM 2
#define EEPROM_CMD_READ 3
#define EEPROM_CMD_READ_STATUS 5
#define EEPROM_CMD_WRITE_ENABLE 6
#define EEPROM_TRANSFER_ATTEMPTS 3

/* Mirrored payloads have a trailing additive checksum. Length must fit the
 * byte-sized checksum cursor (0..255). Writes commit primary before backup.
 * Returns the shared event flags sampled by the final checksum write.
 */
u8 EepromMirrorWrite(u16 primary, u16 backup, u8 *buffer, u16 length);
/* Load buffer and repair a bad copy. If neither checksum is valid, write
 * 0xFF payloads and checksum bytes to both copies. When both are valid, buffer
 * contains primary. The repair decision compares their additive sums: unequal
 * sums trigger a backup rewrite from primary.
 */
void EepromMirrorRead(u16 primary, u16 backup, u8 *buffer, u16 length);
/* Transfers share the SSU and retry on receive overruns. Verifying stored data
 * requires a subsequent read. Reads need writable storage even at length zero:
 * command/address responses are first received into destination[0]. */
void EepromWrite(u16 address, void *source, u16 length);
void EepromRead(u16 address, void *destination, u16 length);
u8 EepromReadByte(u16 address);
/* Fixed-page transfers wrap within the selected EEPROM page. Use aligned
 * addresses for a sequential full-page write. */
void EepromFillPage(u16 address, u8 byteValue);
void EepromFill(u16 address, u16 count, u8 value);
void EepromWritePage(uint address, u8 *source);
void EepromConfigure(void);
void EepromIdle(void);
u8 EepromReceive(void);
/* Returns the shared event flags sampled after the final attempt. */
u8 EepromWriteByte(u16 address, u8 value);
/* Destructive incrementing-pattern test from a 256-byte-aligned start to
 * 0xffff. */
u8 EepromSelfTest(uint startAddress);
/* Receipt IDs must fit the 128-bit bitmap. ID zero is a no-op; nonzero IDs
 * reload and repair status before setting and saving their bit. */
void StatusSetReceived(DeviceStatus *status, u8 eventId);
/* Nonzero IDs reload status through the repairing mirrored read. ID zero
 * immediately returns false. The caller must supply an ID below 128. */
u8 StatusLoadReceived(DeviceStatus *status, u8 eventId);

#endif
