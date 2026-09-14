#include "application/pw_follower_prompts.h"
#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_follower_events.h"
#include "application/pw_home.h"
#include "support/lib_clear.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const SocialFrame *const g_socialSequences[7];

#define PW_DIARY_ACTION_EVENT_BASE 16
#define SOCIAL_ITEM_TIER_STEPS 500
#define SOCIAL_TOP_ITEM_STEPS 0x1194
#define SOCIAL_LAST_ITEM_INDEX 9
#define SOCIAL_HIGH_WATTS 50
#define SOCIAL_MIDDLE_WATTS 20
#define SOCIAL_LOW_WATTS 10
#define GENERATED_POKEMON_FRIENDSHIP 0x46

/* Set runtime Pokemon presence and clear hourly steps. When persistent status
 * has no Pokemon, make the third course encounter the companion, copy its
 * artwork, reset its nickname/friendship and clear diary actions. */
void AutoGeneratePokemon(void)
{
  u8 *buffer;
  u16 courseBytes;

  courseBytes = sizeof(Course);
  g_state.hourSteps = 0;
  g_state.flags.byte |= SYSTEM_HAS_POKEMON;
  ScratchReset();
  buffer = ScratchAlloc(sizeof(DeviceStatus));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, buffer,
                   sizeof(DeviceStatus));
  if (((DeviceStatus *)buffer)->hasPokemon == 0) {
    ((DeviceStatus *)buffer)->hasPokemon = 1;
    ((DeviceStatus *)buffer)->pokemonCompatibilityLe =
        ((DeviceStatus *)buffer)->consoleCompatibilityLe;
    ((DeviceStatus *)buffer)->pokemonGameVersionLe =
        ((DeviceStatus *)buffer)->gameVersionLe;
    ((DeviceStatus *)buffer)->generatedPokemon = 1;
    EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, buffer,
                      sizeof(DeviceStatus));

    ScratchReset();
    buffer = ScratchAlloc(POKEMON_ANIMATION_BYTES);
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                 encounterImages[2 * POKEMON_ANIMATION_BYTES]),
        buffer, POKEMON_ANIMATION_BYTES);
    EepromWrite(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonImage),
        buffer, POKEMON_ANIMATION_BYTES);
    /* Transfer the large animation through the full 1,536-byte scratch
     * storage. Only the small animation's 384 bytes are reserved through the
     * allocator, whose limit is 1,024 bytes. */
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        joiningPokemonImageLarge),
               buffer, POKEMON_LARGE_ANIMATION_BYTES);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                         pokemonImageLarge),
                buffer, POKEMON_LARGE_ANIMATION_BYTES);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        encounterNames[2 * TEXT_RASTER_BYTES]),
               buffer, TEXT_RASTER_BYTES);
    EepromWrite(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonName),
        buffer, TEXT_RASTER_BYTES);

    ScratchReset();
    buffer = ScratchAlloc(courseBytes);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values),
               buffer, courseBytes);
    {
      u16 i;

      ((Course *)buffer)->pokemon = ((Course *)buffer)->encounters[2];
      ((Course *)buffer)->pokemon.reservedAppearance = 0;
      ((Course *)buffer)->friendship = GENERATED_POKEMON_FRIENDSHIP;
      i = 0;
      do {
        ((Course *)buffer)->nickname[i] = 0;
        i++;
      } while (i < sizeof(((Course *)0)->nickname));
      EepromWrite(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values),
          buffer, courseBytes);
    }

    g_state.save.pokemonMinutes = 0;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    ClearDiaryActions();
  }
}

/* Apply accepted home rewards before their first sequence frame. Hourly steps
 * select course items 9 down to 0 in 500-step tiers. Display and log offered
 * items regardless of inventory space. */
void SocialApply(u8 eventId)
{
  u16 itemIdLe;
  u8 emptySlot;
  u16 slotBytes;
  u8 *buffer;

  itemIdLe = 0;
  g_ui.view.social.eventId = eventId;
  g_ui.view.social.sequenceFrame = g_socialSequences[eventId - 1];
  SetView(VIEW_SOCIAL);
  g_state.socialElapsedSeconds = 0;

  switch (eventId) {
  case SOCIAL_EVENT_ITEM:
    if (g_state.hourSteps < SOCIAL_TOP_ITEM_STEPS) {
      g_ui.view.social.rewardValue =
          (SOCIAL_LAST_ITEM_INDEX -
           (g_state.hourSteps / SOCIAL_ITEM_TIER_STEPS));
    } else {
      g_ui.view.social.rewardValue = 0;
    }
    itemIdLe = CourseLoadItemIdLe(g_ui.view.social.rewardValue);
    ScratchReset();
    buffer = ScratchAlloc(slotBytes = sizeof(Item) * INVENTORY_SLOTS);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), buffer,
               slotBytes);
    if ((emptySlot = ItemSlotFindEmpty((Item *)buffer)) < INVENTORY_SLOTS) {
      ((Item *)buffer)[emptySlot].idLe = itemIdLe;
      EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items),
                  buffer, slotBytes);
    }
    break;
  case SOCIAL_EVENT_WATTS_50:
    g_ui.view.social.rewardValue = SOCIAL_HIGH_WATTS;
    WattsAdd(SOCIAL_HIGH_WATTS);
    break;
  case SOCIAL_EVENT_WATTS_20:
    g_ui.view.social.rewardValue = SOCIAL_MIDDLE_WATTS;
    WattsAdd(SOCIAL_MIDDLE_WATTS);
    break;
  case SOCIAL_EVENT_WATTS_10:
    g_ui.view.social.rewardValue = SOCIAL_LOW_WATTS;
    WattsAdd(SOCIAL_LOW_WATTS);
    break;
  case SOCIAL_EVENT_NEW_POKEMON:
    AutoGeneratePokemon();
    break;
  default:
    g_ui.view.social.rewardValue = 0;
    break;
  }

  ScratchReset();
  buffer = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, buffer, sizeof(Course));
  {
    u8 encounter;

    encounter = 0;
    DiaryAppend((Course *)buffer, ScratchAlloc(sizeof(DiaryEntry)),
                (eventId + PW_DIARY_ACTION_EVENT_BASE),
                g_state.save.bonusCourse, encounter, itemIdLe);
  }

  switch (eventId) {
  case SOCIAL_EVENT_WATTS_50:
  case SOCIAL_EVENT_WATTS_20:
  case SOCIAL_EVENT_WATTS_10:
  case SOCIAL_EVENT_BORED: {
    u32 prng;

    prng = RandomNext() >> 3;
    g_ui.view.social.messageVariant = (prng % 3UL);
  } break;
  default:
    g_ui.view.social.messageVariant = 0;
    break;
  }
}

/* Callers select sequences with one-based event IDs 1..5 or 7. */
const SocialFrame *const g_socialSequences[7] = {
    g_socialItemSequence,      g_socialWatts50Sequence, g_socialWatts20Sequence,
    g_socialWatts10Sequence,   g_socialBoredSequence,   0,
    g_socialNewPokemonSequence};
