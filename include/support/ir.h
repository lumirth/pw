#ifndef PW_IR_H
#define PW_IR_H

#include "types.h"

/* Communication results passed to the foreground UI. */
#define IR_RESULT_NONE 0
#define IR_RESULT_NO_RESPONSE 1
#define IR_RESULT_CONNECTION_ERROR 2
#define IR_RESULT_REJECTED 3
#define IR_RESULT_NO_POKEMON 4
#define IR_RESULT_PEER_ALREADY_SEEN 5
#define IR_RESULT_EVENT_ALREADY_RECEIVED 6
#define IR_RESULT_EVENT_NOT_RECEIVED 7
#define IR_RESULT_RECEIVE_OVERFLOW 8

#define PW_IR_TRANSPORT_XOR 0xAAu

/* Borrow the packet's payload window. Replies, EEPROM reads and subsequent
 * receive bursts reuse it, so retain incoming values before those operations.
 */
u8 *IrPayload(void);
/* Initialize the shared IR workspace and peripherals and send a probe.
 * The caller installs IrProtocolTick as the foreground task. */
void IrBegin(void);
/* Accumulate a burst, settle the handshake, then dispatch application packets
 * and bulk transfers. Completion stops the peripherals and applies the queued
 * action through IrComplete, which chooses the next foreground task. */
void IrProtocolTick(void);

void IrInit(void);

#endif
