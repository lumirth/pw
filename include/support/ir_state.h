#ifndef PW_IR_STATE_H
#define PW_IR_STATE_H

#include "flags.h"

#include "types.h"

/* Deferred actions use command values, plus the factory-reset action below.
 * Stamped rewards normalize to their corresponding plain reward. */
enum { IR_ACTION_FACTORY_RESET = 0xE0 };
#define IR_ACTION_NONE 0xff

/* Values 3 and 4 complete the handshake and admit application packets. */
enum {
  IR_PHASE_PROBING = 1,
  IR_PHASE_REPLY_SENT = 2,
  IR_PHASE_INITIATOR = 3,
  IR_PHASE_RESPONDER = 4
};

/* Transfer order is send icon/name/records, then receive icon/name/records.
 * Each send/receive pair shares one kind of peer data. */
#define PEER_BULK_IDLE 0
#define PEER_BULK_SEND_ICON 1
#define PEER_BULK_RECEIVE_ICON 2
#define PEER_BULK_SEND_NAME 3
#define PEER_BULK_RECEIVE_NAME 4
#define PEER_BULK_SEND_RECORDS 5
#define PEER_BULK_RECEIVE_RECORDS 6

typedef struct {
  /* Keep the local token while sessionToken becomes the XOR of both. */
  volatile u32 localHandshakeToken;
  u32 sessionToken;
  u8 handshakePhase;
  u8 timeoutRetryCount;
  u8 reservedAfterTimeout;
  u8 writeOnlyZeroByte;
  u8 checksumFailureCount;
  IrSessionFlags sessionFlags;
  u8 completionAction;
  u8 bulkPhase;
  u16 bulkBytesRemaining;
  /* Receiving reads the remote source into the local destination. */
  u16 bulkSourceEepromAddress;
  u16 bulkDestinationEepromAddress;
  u8 bulkChunksCompleted;
  u8 sci3RxDrainByte;
} IrcWork;

#endif
