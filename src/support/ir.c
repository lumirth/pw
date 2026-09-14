#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "application/pw_nt7508.h"
#include "application/pw_eeprom_m95512.h"
#include "support/ir.h"
#include "application/pw_friend.h"
#include "support/lib_common.h"
#include "startup/h8_resetprg.h"

/* Return the sum of big-endian byte pairs, seeded with 2, after folding the
 * carry twice. The frame builder and checker clear the header checksum bytes
 * before calling; SendPacket writes the result low byte first. */
uint PacketChecksum(u8 *bytes, u32 length)
{
  u32 checksum = 2;
  u32 index = 0;

  while (index < length) {
    u8 value = *bytes++;

    if (index & 1) {
      checksum += value;
    } else {
      checksum += (uint)value << 8;
    }
    ++index;
  }

  checksum = (checksum >> 16) + (uint)checksum;
  checksum += checksum >> 16;

  return checksum;
}

#include "startup/iodefine.h"
#include "project.h"

/* Initialize both EEPROM cursors and reset transfer progress at the start of
 * each peer phase. */
#pragma inline(BeginBulkPhase)
static void BeginBulkPhase(u8 phase, uint sourceAddress,
                           uint destinationAddress, uint byteCount)
{
  g_work.irc.work.bulkPhase = phase;
  g_work.irc.work.bulkSourceEepromAddress = sourceAddress;
  g_work.irc.work.bulkDestinationEepromAddress = destinationAddress;
  g_work.irc.work.bulkBytesRemaining = byteCount;
  g_work.irc.work.bulkChunksCompleted = 0;
}

/* Motion, sound and infrared communication reuse this workspace. */
Workspace g_work;

#pragma interrupt(Sci3Interrupt(vect = 37))
void Sci3Interrupt(void)
{
}

u8 *IrPayload(void)
{
  return (g_work.irc.packet + IR_HEADER_BYTES);
}

void IrInitPins(void)
{
  IO.TARGET_F088 = 3;
  IO.PDR3.BYTE = 1;
  IO.PCR3 = 5;
}

void IrTransmitByte(u8 value);

/* The eight-byte header contains a command, its parameter, a little-endian
 * checksum and four session-token bytes. Payload follows immediately. */
typedef struct {
  u8 command;
  u8 argument;
  u8 checksumLo;
  u8 checksumHi;
  u32 sessionToken;
} IrcHeader;

/* Checksum the header and payload with checksum bytes cleared. XOR and transmit
 * each byte, wait for SCI3 TEND and two Timer W counts, then drain a pending
 * receive byte. */
void SendPacket(u8 payloadLength, u8 command, u8 argument)
{
  IrcHeader *header;
  u16 checksum;
  u16 i;
  u16 transmitEndTicks;

  header = (IrcHeader *)g_work.irc.packet;
  header->command = command;
  header->argument = argument;
  header->sessionToken = g_work.irc.work.sessionToken;
  header->checksumLo = 0;
  header->checksumHi = 0;

  checksum = PacketChecksum(g_work.irc.packet,
                            (payloadLength + (uint)IR_HEADER_BYTES));
  header->checksumLo = checksum;
  header->checksumHi = (checksum >> 8);

  payloadLength += IR_HEADER_BYTES;
  i = 0;
  while (i < payloadLength) {
    IrTransmitByte(g_work.irc.packet[i]);
    i++;
  }

  while (SCI3.SSR3.BIT.TEND == 0) {
  }
  transmitEndTicks = TW.TCNT;
  while ((TW.TCNT - transmitEndTicks) < 2) {
  }
  if (SCI3.SSR3.BIT.RDRF != 0) {
    g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
  }
}

/* Enable SCI3's clock, initialize the UART, allow settling time, and enable
 * the IrDA transmit path. */
void IrConfigure(void)
{
  s8 settle;

  CKSTPR1.BYTE |= 0x40; /* SCI3 clock-stop bit 6 */
  SCI3.SPCR.BYTE = 1;
  SCI3.SSR3.BYTE &= 0x84;
  SCI3.SEMR.BYTE = 0;
  SCI3.SCR3.BYTE = 0;
  SCI3.SMR3.BYTE = 0;
  SCI3.BRR3 = 0;
  settle = 5;
  do {
  } while (--settle != 0);
  SCI3.SCR3.BYTE = 0x10;
  SCI3.IrCR.BYTE = 0x80;
  SCI3.SPCR.BYTE = 0x11;
  SCI3.SCR3.BYTE = 0x30;
}

/* Wait for SCI3 TDRE, XOR the byte with the transport mask, and write TDR3. */
void IrTransmitByte(u8 value)
{
  while (SCI3.SSR3.BIT.TDRE == 0) {
  }
  SCI3.TDR3 = (value ^ PW_IR_TRANSPORT_XOR);
}

void IrInit(void)
{
  IrInitPins();
}

/* Start SCI3/IrDA with two delay units before and after lowering PDR3. Run
 * Timer W continuously as the protocol clock and drain pending SCI3 input. */
void IrHardwareStart(void)
{
  u8 ssr3;

  IrConfigure();
  LowClockDelay();
  LowClockDelay();
  IO.PDR3.BYTE = 0;
  LowClockDelay();
  LowClockDelay();
  CKSTPR2.BIT.TWCKSTP = 1;
  TW.TCRW.BYTE = ((TW.TCRW.BYTE & 0x8f) | 0x40);
  TW.TCRW.BIT.CCLR = 0;
  TW.TIERW.BIT.IMIEA = 0;
  TW.TMRW.BIT.CTS = 1;
  ssr3 = SCI3.SSR3.BYTE;
  SCI3.SSR3.BYTE = (ssr3 & 0xc4);
  if (SCI3.SSR3.BIT.RDRF != 0) {
    g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
  }
}

#define IR_ACK 0xF8u
#define IR_RESPONSE 0xFAu
#define IR_CONNECT 0xFCu
#define IR_SHUTDOWN 0xF4u

#define PW_IR_RX_WINDOW_CAPACITY ((uint)(IR_HEADER_BYTES + IR_PAYLOAD_BYTES))
#define PW_IR_FRAME_GAP_TICKS 4
#define PW_IR_INACTIVITY_TICKS 0xc80
#define PW_IR_TIMEOUT_RETRY_LIMIT 0x14u
#define PW_IR_CHECKSUM_FAIL_LIMIT 0x14u
#define PW_IR_RETRY_JITTER_MASK 0x0Fu
#define PW_IR_RETRY_JITTER_TICKS 0x60u

#define PW_IR_BULK_MAXIMUM_CHUNK_LENGTH EEPROM_PAGE_BYTES

typedef char IrcReceiveStorageHoldsMaximumFrame
    [sizeof(g_work.irc.packet) >= PW_IR_RX_WINDOW_CAPACITY ? 1 : -1];
typedef char IrcDecodeStorageHoldsMaximumChunk
    [sizeof(g_work.irc.eepromScratch) >= PW_IR_BULK_MAXIMUM_CHUNK_LENGTH ? 1
                                                                         : -1];
typedef char IrcHeaderMatchesPayloadOffset[sizeof(IrcHeader) == IR_HEADER_BYTES
                                               ? 1
                                               : -1];

/* Start probing with a token from the PRNG state, clear session progress,
 * and send the one-byte connection probe. */
void IrBegin(void)
{
  IrHardwareStart();
  g_state.irResult = IR_RESULT_NONE;
  g_work.irc.work.localHandshakeToken = g_state.randomState;
  g_work.irc.work.sessionToken = g_work.irc.work.localHandshakeToken;
  g_work.irc.work.handshakePhase = IR_PHASE_PROBING;
  g_work.irc.work.timeoutRetryCount = 0;
  g_work.irc.work.checksumFailureCount = 0;
  g_work.irc.work.completionAction = IR_ACTION_NONE;
  g_work.irc.work.sessionFlags.bits.receivedBurst = 0;
  g_state.irReceivedBytes = 0;
  g_work.irc.work.writeOnlyZeroByte = 0;
  g_state.irTimerReference = TW.TCNT;
  g_work.irc.work.bulkPhase = PEER_BULK_IDLE;
  IrTransmitByte(IR_CONNECT);
}

void IrFinish(void);

/* The peer-status exchange has its own request/reply role. */
#pragma inline(IsPeerStatusResponder)
static u8 IsPeerStatusResponder(void)
{
  return g_ui.view.ir.role.bits.peerStatusResponder;
}

/* Peer-info construction reuses the page-decompression buffer. */
#pragma inline(CoursePokemonBuffer)
static Pokemon *CoursePokemonBuffer(void)
{
  Pokemon *record;

  record = (Pokemon *)g_work.irc.eepromScratch;
  return record;
}

/* Fill outgoing peer info with local trainer and Pokemon details and current
 * step totals. */
#pragma inline(BuildPeerInfo)
static void BuildPeerInfo(void)
{
  PeerInfo *peerInfo;
  Pokemon *pokemon;
  int i;

  pokemon = CoursePokemonBuffer();
  peerInfo = (PeerInfo *)IrPayload();
  peerInfo->dailySteps = g_state.dailySteps;
  peerInfo->hourSteps = g_state.hourSteps;
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values.pokemon),
      pokemon, sizeof(Pokemon));
  peerInfo->fixedFacing = pokemon->fixedFacing;
  peerInfo->idLe = pokemon->idLe;
  peerInfo->form = pokemon->form;
  peerInfo->sex = pokemon->sex;
  peerInfo->shiny = pokemon->shiny;
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                   (u8 *)&g_work.irc.statusB.status, sizeof(DeviceStatus));
  peerInfo->compatibilityLe = g_work.irc.statusB.status.consoleCompatibilityLe;
  peerInfo->gameVersionLe = g_work.irc.statusB.status.gameVersionLe;
  i = 0;
  do {
    peerInfo->trainerName[i] = g_work.irc.statusB.status.trainerNameData[i];
    i++;
  } while (i < TRAINER_NAME_BYTES);
  EepromRead((u16)((CourseResources *)EEPROM_COURSE)->values.nickname,
             peerInfo->nickname, sizeof(peerInfo->nickname));
}

/* A read request carries the remote EEPROM address high byte first,
 * followed by a capped byte count. */
#pragma inline(RequestBulkChunk)
static void RequestBulkChunk(void)
{
  u16 chunk;
  u16 src;
  u8 *out;

  const u16 remaining = g_work.irc.work.bulkBytesRemaining;
  chunk = remaining;
  if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
    chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
  }
  out = IrPayload();
  src = g_work.irc.work.bulkSourceEepromAddress;
  out[0] = (src >> 8);
  out[1] = src;
  out[2] = chunk;
  SendPacket(3, IR_CMD_EEPROM_READ, IR_WALKER_PARAMETER);
}

/* An idle Timer W gap closes a received burst. Probing establishes the shared
 * token before application traffic; peer transfer phases then advance on
 * replies. Packet handlers perform immediate storage writes and queue the
 * action that IrFinish applies after disconnect. Replies reuse the receive
 * buffer, so incoming fields needed afterward are saved before constructing
 * them. Accepted completion actions take priority over later errors. */
void IrProtocolTick(void)
{
  u8 receivedBytes;
  u16 elapsedTicks;
  u16 retryDelayTicks;
  u16 timerSample;
  u16 displayBank;
  u32 receivedToken;
  u16 receivedChecksum;
  u16 computedChecksum;
  IrcHeader *header;
  DeviceStatus *payloadStatus;
  u8 payloadLength;
  u8 *payload;
  u8 command;
  u8 argument;

  WatchdogService();
  SCI3.SSR3.BYTE &= 0xc4;
  receivedBytes = g_state.irReceivedBytes;
  if (SCI3.SSR3.BIT.RDRF != 0) {
    if (receivedBytes >= PW_IR_RX_WINDOW_CAPACITY) {
      g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
      g_state.irResult = IR_RESULT_RECEIVE_OVERFLOW;
      IrFinish();
      return;
    }
    g_work.irc.packet[g_state.irReceivedBytes++] =
        (SCI3.RDR3 ^ PW_IR_TRANSPORT_XOR);
    g_state.irTimerReference = TW.TCNT;
    return;
  }

  elapsedTicks = (TW.TCNT - g_state.irTimerReference);
  if (elapsedTicks <= PW_IR_FRAME_GAP_TICKS) {
    return;
  }
  if (elapsedTicks > PW_IR_INACTIVITY_TICKS) {
    g_work.irc.work.timeoutRetryCount++;
    if ((g_work.irc.work.handshakePhase >= IR_PHASE_INITIATOR) ||
        (g_work.irc.work.timeoutRetryCount >= PW_IR_TIMEOUT_RETRY_LIMIT)) {
      if (g_work.irc.work.sessionFlags.bits.receivedBurst != 0) {
        g_state.irResult = IR_RESULT_CONNECTION_ERROR;
      } else {
        g_state.irResult = IR_RESULT_NO_RESPONSE;
      }
      IrFinish();
      return;
    }
    retryDelayTicks = (((RandomNext() >> 5) & PW_IR_RETRY_JITTER_MASK) *
                       PW_IR_RETRY_JITTER_TICKS);
    g_state.irTimerReference = TW.TCNT;
    while ((TW.TCNT - g_state.irTimerReference) < retryDelayTicks) {
    }
    g_work.irc.work.handshakePhase = IR_PHASE_PROBING;
    IrTransmitByte(IR_CONNECT);
    timerSample = TW.TCNT;
    /* Timer W bit 14 selects the raster bank. */
    displayBank = ((timerSample >> 14) & 1);
    DisplaySelectBank(displayBank);
    g_state.irTimerReference = TW.TCNT;
    return;
  }

  if (receivedBytes == 0) {
    return;
  }
  g_work.irc.work.sessionFlags.bits.receivedBurst = 1;
  if (g_state.irReceivedBytes == 1) {
    /* Handle CONNECT as a single-byte probe. */
    g_state.irReceivedBytes = 0;
    if (g_work.irc.packet[0] != IR_CONNECT) {
      return;
    }
    /* The probing phase answers CONNECT. */
    switch (g_work.irc.work.handshakePhase) {
    case IR_PHASE_PROBING:
      g_work.irc.work.handshakePhase = IR_PHASE_REPLY_SENT;
      SendPacket(0, IR_RESPONSE, IR_WALKER_PARAMETER);
      break;
    case IR_PHASE_REPLY_SENT:
      break;
    case IR_PHASE_RESPONDER:
    case IR_PHASE_INITIATOR:
      break;
    }
    return;
  }

  header = (IrcHeader *)g_work.irc.packet;
  receivedChecksum = (header->checksumLo + (header->checksumHi << 8));
  header->checksumLo = 0;
  header->checksumHi = 0;
  computedChecksum = PacketChecksum(g_work.irc.packet, g_state.irReceivedBytes);
  if (receivedChecksum != computedChecksum) {
    g_state.irReceivedBytes = 0;
    g_work.irc.work.checksumFailureCount++;
    if (g_work.irc.work.checksumFailureCount < PW_IR_CHECKSUM_FAIL_LIMIT) {
      return;
    }
    g_state.irResult = IR_RESULT_CONNECTION_ERROR;
    IrFinish();
    return;
  }

  /* Keep the received token before constructing a reply in the same buffer. */
  receivedToken = header->sessionToken;

  argument = header->argument;
  command = header->command;
  payloadLength = g_state.irReceivedBytes;
  payloadLength -= IR_HEADER_BYTES;

  payload = g_work.irc.packet + IR_HEADER_BYTES;
  if (command < IR_ACK) {
    if (receivedToken != g_work.irc.work.sessionToken) {
      goto packetDone;
    }
    if (g_work.irc.work.handshakePhase < IR_PHASE_INITIATOR) {
      goto packetDone;
    }
  }
  switch (command) {
  case IR_RESPONSE:
    if ((argument == IR_CONSOLE_PARAMETER) ||
        (argument == IR_WALKER_PARAMETER)) {
      switch (g_work.irc.work.handshakePhase) {
      case IR_PHASE_PROBING:
        g_work.irc.work.handshakePhase = IR_PHASE_INITIATOR;
        SendPacket(0, IR_ACK, IR_WALKER_PARAMETER);
        g_work.irc.work.sessionToken =
            receivedToken ^ g_work.irc.work.localHandshakeToken;
        break;
      case IR_PHASE_RESPONDER:
      case IR_PHASE_INITIATOR:
      case IR_PHASE_REPLY_SENT:
        retryDelayTicks = (((RandomNext() >> 5) & PW_IR_RETRY_JITTER_MASK) *
                           PW_IR_RETRY_JITTER_TICKS);
        g_state.irTimerReference = TW.TCNT;
        while ((TW.TCNT - g_state.irTimerReference) < retryDelayTicks) {
        }
        g_work.irc.work.handshakePhase = IR_PHASE_PROBING;
        IrTransmitByte(IR_CONNECT);
        g_state.irTimerReference = TW.TCNT;
        break;
      }
    } else {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
    }
    break;
  case IR_SHUTDOWN:
    IrFinish();
    break;
  case IR_ACK:
    /* A Walker acknowledgement starts the peer-status exchange. */
    if (argument != IR_WALKER_PARAMETER) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.work.handshakePhase < IR_PHASE_INITIATOR) {
      g_work.irc.work.sessionToken =
          receivedToken ^ g_work.irc.work.localHandshakeToken;
      g_work.irc.work.handshakePhase = IR_PHASE_RESPONDER;
      EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, IrPayload(),
                       sizeof(DeviceStatus));
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REQUEST,
                 IR_WALKER_PARAMETER);
      g_ui.view.ir.role.bits.peerStatusResponder = 0;
    }
    break;
  case IR_CMD_PEER_STATUS_REQUEST:
    /* Each failed eligibility check sends a status reply and ends the session.
     * A previously recorded peer gets a dedicated reply. */
    g_ui.view.ir.role.bits.peerStatusResponder = 1;
    payloadStatus = (DeviceStatus *)IrPayload();
    /* Save peer status before loading local status into the payload buffer. */
    g_work.irc.statusA.status = *payloadStatus;
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)payloadStatus, sizeof(DeviceStatus));
    if (payloadStatus->registered == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.registered == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.hasPokemon == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (payloadStatus->peerProtocol != g_work.irc.statusA.status.peerProtocol) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.firmwareCompatibility != 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (payloadStatus->hasPokemon == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
                 IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_NO_POKEMON;
      IrFinish();
      break;
    }
    if (SeenPeer(g_work.irc.statusA.status.deviceId) != 0) {
      SendPacket(0, IR_CMD_PEER_ALREADY_RECORDED, IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_PEER_ALREADY_SEEN;
      IrFinish();
      break;
    }
    SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY,
               IR_WALKER_PARAMETER);
    break;
  case IR_CMD_PEER_STATUS_REPLY:
    /* Check status locally and end the session on failure. A previously
     * recorded peer gets a dedicated reply; a valid peer starts icon transfer.
     */
    payloadStatus = (DeviceStatus *)IrPayload();
    /* Save peer status before loading local status into the payload buffer. */
    g_work.irc.statusA.status = *payloadStatus;
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)payloadStatus, sizeof(DeviceStatus));
    if (payloadStatus->registered == 0) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.registered == 0) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.hasPokemon == 0) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (payloadStatus->peerProtocol != g_work.irc.statusA.status.peerProtocol) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.firmwareCompatibility != 0) {
      g_state.irResult = IR_RESULT_REJECTED;
      IrFinish();
      break;
    }
    if (payloadStatus->hasPokemon == 0) {
      g_state.irResult = IR_RESULT_NO_POKEMON;
      IrFinish();
      break;
    }
    if (SeenPeer(g_work.irc.statusA.status.deviceId) != 0) {
      SendPacket(0, IR_CMD_PEER_ALREADY_RECORDED, IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_PEER_ALREADY_SEEN;
      IrFinish();
      break;
    }
    BeginBulkPhase(
        PEER_BULK_SEND_ICON,
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonImage),
        EEPROM_PEER_IMAGE, sizeof(((CourseResources *)0)->pokemonImage));
    {
      u16 chunk;
      u16 dest;

      chunk = g_work.irc.work.bulkBytesRemaining;
      if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
        chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
      }
      EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
      dest = g_work.irc.work.bulkDestinationEepromAddress;
      SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
      g_work.irc.work.bulkSourceEepromAddress =
          (g_work.irc.work.bulkSourceEepromAddress + chunk);
      g_work.irc.work.bulkDestinationEepromAddress =
          (g_work.irc.work.bulkDestinationEepromAddress + chunk);
      g_work.irc.work.bulkBytesRemaining =
          (g_work.irc.work.bulkBytesRemaining - chunk);
      g_work.irc.work.bulkChunksCompleted++;
    }
    break;
  case IR_CMD_PEER_INFO:
    if (IsPeerStatusResponder() != 0) {

      EepromWrite(EEPROM_PEER_INFO, IrPayload(), sizeof(PeerInfo));
      BuildPeerInfo();
      SendPacket(sizeof(PeerInfo), IR_CMD_PEER_INFO, IR_WALKER_PARAMETER);
    } else {
      EepromWrite(EEPROM_PEER_INFO, IrPayload(), sizeof(PeerInfo));
      SendPacket(0, IR_CMD_PEER_START, IR_WALKER_PARAMETER);
    }
    break;
  case IR_CMD_PEER_START:
    if (IsPeerStatusResponder() != 0) {
      SendPacket(0, IR_CMD_PEER_START, IR_WALKER_PARAMETER);
    }
    g_work.irc.work.completionAction = IR_CMD_PEER_START;
    IrFinish();
    break;
  case IR_CMD_PEER_ALREADY_RECORDED:
    g_state.irResult = IR_RESULT_PEER_ALREADY_SEEN;
    IrFinish();
    break;
  case IR_CMD_STATUS_REQUEST:
    payloadStatus = (DeviceStatus *)IrPayload();
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)IrPayload(), sizeof(DeviceStatus));
    payloadStatus->totalSteps = g_state.save.totalSteps;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, watts),
                &g_state.save.watts, 2);
    g_work.irc.statusB.status = *payloadStatus;
    SendPacket(sizeof(DeviceStatus), IR_CMD_STATUS_REPLY, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_WALK_START_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_START_ACCEPTED, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_WALK_START_REJECTED:
    g_state.irResult = IR_RESULT_REJECTED;
    IrFinish();
    break;
  case IR_CMD_WALK_START_COMMIT:
    SendPacket(0, IR_CMD_WALK_START_COMMIT, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_WALK_START_COMMIT;
    break;
  case IR_CMD_WALK_END_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_END_ACCEPTED, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_WALK_END_REJECTED:
    g_state.irResult = IR_RESULT_REJECTED;
    IrFinish();
    break;
  case IR_CMD_WALK_END_COMMIT:
    SendPacket(0, IR_CMD_WALK_END_ACK, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_WALK_END_COMMIT;
    IrFinish();
    break;
  case IR_CMD_WALK_UPDATE_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_UPDATE_ACCEPTED, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_WALK_UPDATE_COMMIT:
    SendPacket(0, IR_CMD_WALK_UPDATE_COMMIT, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_WALK_UPDATE_COMMIT;
    IrFinish();
    break;
  case IR_CMD_WALK_UPDATE_REJECTED:
    g_state.irResult = IR_RESULT_REJECTED;
    IrFinish();
    break;
  case IR_CMD_GIFT_REQUEST:
    StatusApplyTime();
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)&g_work.irc.statusB.status, sizeof(DeviceStatus));
    SendPacket(0, IR_CMD_GIFT_ACCEPTED, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_GIFT_COLLECTION_COMMIT:
    SendPacket(0, IR_CMD_GIFT_COLLECTION_ACK, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_GIFT_COLLECTION_COMMIT;
    break;
  case IR_CMD_GIFT_REJECTED:
    g_state.irResult = IR_RESULT_REJECTED;
    IrFinish();
    break;
  case IR_CMD_EVENT_MAP_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= EVENT_PRESENT_MAP;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_MAP_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_MAP_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_MAP_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= (EVENT_PRESENT_MAP | EVENT_PRESENT_STAMPS);
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_MAP_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_MAP_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_POKEMON_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= EVENT_PRESENT_POKEMON;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_POKEMON_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_POKEMON_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_POKEMON_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= (EVENT_PRESENT_POKEMON | EVENT_PRESENT_STAMPS);
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_POKEMON_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_POKEMON_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_ITEM_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= EVENT_PRESENT_ITEM;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_ITEM_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_ITEM_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_ITEM_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= (EVENT_PRESENT_ITEM | EVENT_PRESENT_STAMPS);
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_ITEM_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_ITEM_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_COURSE_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= EVENT_PRESENT_COURSE;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    g_state.save.bonusCourse = 1;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_EVENT_COURSE_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_COURSE_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_COURSE_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= (EVENT_PRESENT_COURSE | EVENT_PRESENT_STAMPS);
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    g_state.save.bonusCourse = 1;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_EVENT_COURSE_DONE, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_EVENT_COURSE_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_REJECTED:
    g_state.irResult = IR_RESULT_REJECTED;
    IrFinish();
    break;
  case IR_CMD_PING_REQUEST:
    SendPacket(0, IR_CMD_PING_REPLY, IR_WALKER_PARAMETER);
    break;
  case IR_CMD_PAGE_COMPRESSED_B:
  case IR_CMD_PAGE_COMPRESSED_A: {
    u16 address;

    address = (((u16)argument << 8) + command);
    if (payloadLength == PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
      EepromWritePage(address, payload);
    } else {
      BulkDecode(payload, g_work.irc.eepromScratch);
      EepromWritePage(address, g_work.irc.eepromScratch);
    }
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_PAGE_WRITE_B:
  case IR_CMD_PAGE_WRITE_A: {
    u16 address;

    address = (((u16)argument << 8) + (command & 0x80));
    EepromWrite(address, payload, payloadLength);
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_PAGE_REPLY: {
    u16 chunk;
    u16 dest;

    if (g_work.irc.work.bulkBytesRemaining == 0) {
      switch (g_work.irc.work.bulkPhase) {
      case PEER_BULK_SEND_ICON:
        BeginBulkPhase(PEER_BULK_SEND_NAME,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonName),
                       EEPROM_PEER_NAME,
                       sizeof(((CourseResources *)0)->pokemonName));
        chunk = g_work.irc.work.bulkBytesRemaining;
        if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
          chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
        }
        EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
        dest = g_work.irc.work.bulkDestinationEepromAddress;
        SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
        g_work.irc.work.bulkSourceEepromAddress =
            (g_work.irc.work.bulkSourceEepromAddress + chunk);
        g_work.irc.work.bulkDestinationEepromAddress =
            (g_work.irc.work.bulkDestinationEepromAddress + chunk);
        g_work.irc.work.bulkBytesRemaining =
            (g_work.irc.work.bulkBytesRemaining - chunk);
        g_work.irc.work.bulkChunksCompleted++;
        break;
      case PEER_BULK_SEND_NAME:
        BeginBulkPhase(PEER_BULK_SEND_RECORDS, EEPROM_OWN_RECORDS,
                       EEPROM_PEER_RECORDS, sizeof(PeerRecords));
        chunk = g_work.irc.work.bulkBytesRemaining;
        if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
          chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
        }
        EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
        dest = g_work.irc.work.bulkDestinationEepromAddress;
        SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
        g_work.irc.work.bulkSourceEepromAddress =
            (g_work.irc.work.bulkSourceEepromAddress + chunk);
        g_work.irc.work.bulkDestinationEepromAddress =
            (g_work.irc.work.bulkDestinationEepromAddress + chunk);
        g_work.irc.work.bulkBytesRemaining =
            (g_work.irc.work.bulkBytesRemaining - chunk);
        g_work.irc.work.bulkChunksCompleted++;
        break;
      case PEER_BULK_SEND_RECORDS:
        BeginBulkPhase(PEER_BULK_RECEIVE_ICON,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonImage),
                       EEPROM_PEER_IMAGE,
                       sizeof(((CourseResources *)0)->pokemonImage));
        RequestBulkChunk();
        break;
      }
    } else {
      chunk = g_work.irc.work.bulkBytesRemaining;
      if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
        chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
      }
      EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
      dest = g_work.irc.work.bulkDestinationEepromAddress;
      SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
      g_work.irc.work.bulkSourceEepromAddress =
          (g_work.irc.work.bulkSourceEepromAddress + chunk);
      g_work.irc.work.bulkDestinationEepromAddress =
          (g_work.irc.work.bulkDestinationEepromAddress + chunk);
      g_work.irc.work.bulkBytesRemaining =
          (g_work.irc.work.bulkBytesRemaining - chunk);
      g_work.irc.work.bulkChunksCompleted++;
    }
    break;
  }
  case IR_CMD_EVENT_COURSE_CHECK:
  case IR_CMD_EVENT_ITEM_CHECK:
  case IR_CMD_EVENT_POKEMON_CHECK:
  case IR_CMD_EVENT_MAP_CHECK: {
    u8 i;
    u8 *out;

    StatusApplyTime();
    out = IrPayload();
    i = 0;
    do {
      *out++ = g_work.irc.statusB.status.receivedEvents[i];
      i++;
    } while (i < sizeof(g_work.irc.statusB.status.receivedEvents));
    *out = g_work.irc.statusA.status.receiptIndex;
    if (StatusLoadReceived(&g_work.irc.statusB.status,
                           g_work.irc.statusA.status.receiptIndex) != 0) {
      SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1),
                 IR_CMD_EVENT_ALREADY_RECEIVED, IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_EVENT_ALREADY_RECEIVED;
      IrFinish();
      break;
    }
    SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1), command,
               IR_WALKER_PARAMETER);
    break;
  }
  case IR_CMD_EVENT_STAMP3_CHECK:
  case IR_CMD_EVENT_STAMP2_CHECK:
  case IR_CMD_EVENT_STAMP1_CHECK:
  case IR_CMD_EVENT_STAMP0_CHECK: {
    u8 i;
    u8 *out;

    StatusApplyTime();
    out = IrPayload();
    i = 0;
    do {
      *out++ = g_work.irc.statusB.status.receivedEvents[i];
      i++;
    } while (i < sizeof(g_work.irc.statusB.status.receivedEvents));
    *out = g_work.irc.statusA.status.receiptIndex;
    if (StatusLoadReceived(&g_work.irc.statusB.status,
                           g_work.irc.statusA.status.receiptIndex) != 0) {
      SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1),
                 IR_CMD_EVENT_ALREADY_RECEIVED, IR_WALKER_PARAMETER);
      g_state.irResult = IR_RESULT_EVENT_ALREADY_RECEIVED;
      IrFinish();
      break;
    }
    SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1), command,
               IR_WALKER_PARAMETER);
    break;
  }
  case IR_CMD_EVENT_STAMP3:
  case IR_CMD_EVENT_STAMP2:
  case IR_CMD_EVENT_STAMP1:
  case IR_CMD_EVENT_STAMP0: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    switch (command) {
    case IR_CMD_EVENT_STAMP0:
      flags |= EVENT_PRESENT_STAMP0;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP0;
      break;
    case IR_CMD_EVENT_STAMP1:
      flags |= EVENT_PRESENT_STAMP1;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP1;
      break;
    case IR_CMD_EVENT_STAMP2:
      flags |= EVENT_PRESENT_STAMP2;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP2;
      break;
    case IR_CMD_EVENT_STAMP3:
      flags |= EVENT_PRESENT_STAMP3;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP3;
      break;
    }
    EepromWriteByte(EEPROM_EVENTS, flags);
    SendPacket(0, (command + 0x10), IR_WALKER_PARAMETER);
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_ALREADY_RECEIVED:
    SendPacket(0, IR_CMD_EVENT_ALREADY_RECEIVED, IR_WALKER_PARAMETER);
    g_state.irResult = IR_RESULT_EVENT_NOT_RECEIVED;
    IrFinish();
    break;
  case IR_CMD_EVENT_BUFFER_FULL:
    SendPacket(0, IR_CMD_EVENT_BUFFER_FULL, IR_WALKER_PARAMETER);
    g_state.irResult = IR_RESULT_EVENT_NOT_RECEIVED;
    IrFinish();
    break;
  case IR_CMD_FACTORY_SETUP: {
    FactoryData *setup;
    u8 i;

    setup = (FactoryData *)IrPayload();
    EepromMirrorWrite(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, setup->deviceId,
                      DEVICE_ID_BYTES);
    EepromWrite(EEPROM_COUNTERS, &setup->thresholds, sizeof(MotionThresholds));
    g_ui.view.ir.diagnosticsReady = 1;
    g_work.irc.work.completionAction = IR_CMD_FACTORY_SETUP;
    /* The diagnostics view reads this result from shared UI byte 1 after
     * IrComplete transitions to it. */
    switch (setup->mode) {
    case FACTORY_SETUP_WRITE_LCD:
      EepromMirrorWrite(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                        (u8 *)setup + offsetof(FactoryData, lcdParameters),
                        sizeof(setup->lcdParameters));
      break;
    case FACTORY_SETUP_CHECK_LCD:
      EepromMirrorRead(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                       g_work.irc.eepromScratch, sizeof(setup->lcdParameters));
      {
        const FactoryData *expected = setup;

        for (i = 0; i < sizeof(expected->lcdParameters); i++) {
          if (expected->lcdParameters[i] != g_work.irc.eepromScratch[i]) {
            g_ui.view.ir.diagnosticsReady = 0;
            break;
          }
        }
      }
      break;

    case FACTORY_SETUP_KEEP_LCD:
      break;
    case FACTORY_SETUP_WRITE_LCD_RESET:
      EepromMirrorWrite(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                        (u8 *)setup + offsetof(FactoryData, lcdParameters),
                        sizeof(setup->lcdParameters));
      g_work.irc.work.completionAction = IR_ACTION_FACTORY_RESET;
      break;
    }
    SendPacket(DEVICE_ID_BYTES, IR_CMD_FACTORY_SETUP, IR_WALKER_PARAMETER);
    break;
  }
  case IR_CMD_MOTION_TEST_SETUP:
    if ((argument == 1) && (payloadLength == sizeof(MotionThresholds))) {
      EepromWrite(EEPROM_COUNTERS, IrPayload(), sizeof(MotionThresholds));
      SendPacket(0, IR_CMD_MOTION_TEST_SETUP, IR_WALKER_PARAMETER);
      g_work.irc.work.completionAction = IR_CMD_MOTION_TEST_SETUP;
    }
    break;
  case IR_CMD_RESET_ALL:
    EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, IrPayload(),
                     DEVICE_ID_BYTES);
    SendPacket(DEVICE_ID_BYTES, IR_CMD_RESET_ALL, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_RESET_ALL;
    break;
  case IR_CMD_RESET_KEEP_STEPS:
    EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, IrPayload(),
                     DEVICE_ID_BYTES);
    SendPacket(DEVICE_ID_BYTES, IR_CMD_RESET_ALL, IR_WALKER_PARAMETER);
    g_work.irc.work.completionAction = IR_CMD_RESET_KEEP_STEPS;
    break;
  case IR_CMD_EEPROM_READ: {
    u16 address;
    u8 length;

    address = ((payload[0] << 8) + payload[1]);
    length = payload[2];
    EepromRead(address, IrPayload(), length);
    SendPacket(length, IR_CMD_EEPROM_REPLY, IR_WALKER_PARAMETER);
    break;
  }
  case IR_CMD_EEPROM_REPLY: {
    EepromWrite(g_work.irc.work.bulkDestinationEepromAddress, IrPayload(),
                payloadLength);

    g_work.irc.work.bulkSourceEepromAddress += payloadLength;
    g_work.irc.work.bulkDestinationEepromAddress += payloadLength;
    g_work.irc.work.bulkBytesRemaining -= payloadLength;
    g_work.irc.work.bulkChunksCompleted++;
    if (g_work.irc.work.bulkBytesRemaining == 0) {
      switch (g_work.irc.work.bulkPhase) {
      case PEER_BULK_RECEIVE_ICON:
        BeginBulkPhase(PEER_BULK_RECEIVE_NAME,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonName),
                       EEPROM_PEER_NAME,
                       sizeof(((CourseResources *)0)->pokemonName));
        RequestBulkChunk();
        break;
      case PEER_BULK_RECEIVE_NAME:
        BeginBulkPhase(PEER_BULK_RECEIVE_RECORDS, EEPROM_OWN_RECORDS,
                       EEPROM_PEER_RECORDS, sizeof(PeerRecords));
        RequestBulkChunk();
        break;
      case PEER_BULK_RECEIVE_RECORDS:
        BuildPeerInfo();
        SendPacket(sizeof(PeerInfo), IR_CMD_PEER_INFO, IR_WALKER_PARAMETER);
        goto packetDone;
      default:
        goto packetDone;
      }
    } else {
      RequestBulkChunk();
    }
    break;
  }
  case IR_CMD_EEPROM_WRITE: {
    u16 address;

    address = (((u16)argument << 8) + payload[0]);
    EepromWrite(address, payload + 1, (payloadLength - 1));
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_RAM_WRITE: {
    u8 *dest;
    u8 i;

    dest = (u8 *)(((u16)argument << 8) + payload[0]);

    for (i = 0; i < payloadLength - 1; i++) {
      *dest++ = payload[i + 1];
    }
    SendPacket(0, IR_CMD_RAM_WRITE, argument);
    break;
  }
  default:
    break;
  }

packetDone:
  timerSample = TW.TCNT;
  /* Timer W bit 14 animates the two prepared connection frames. */
  displayBank = ((timerSample >> 14) & 1);
  DisplaySelectBank(displayBank);
  g_state.irReceivedBytes = 0;
  g_state.irTimerReference = TW.TCNT;
}

/* Stop the IR peripherals, then dispatch the session's completion action or
 * result. */
void IrFinish(void)
{
  IRR1.BIT.IRRI1 = 0;
  IO.PDR3.BYTE = 1;
  SCI3.SPCR.BYTE = 1;
  SCI3.SCR3.BYTE = 0;
  CKSTPR1.BIT.S3CKSTP = 0;
  TW.TCRW.BIT.CCLR = 1;
  TW.TMRW.BIT.CTS = 0;
  CKSTPR2.BIT.TWCKSTP = 0;
  IrComplete();
}
