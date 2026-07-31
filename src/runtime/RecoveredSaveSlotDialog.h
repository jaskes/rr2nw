#pragma once

#include "LevelSaveSlot.h"

#include <windows.h>

#include <cstdint>
#include <string>

enum ERecoveredSaveSlotDialogMode {
  RECOVERED_SAVE_SLOT_DIALOG_SAVE = 1,
  RECOVERED_SAVE_SLOT_DIALOG_LOAD = 2
};

struct SRecoveredSaveSlotDialogInput {
  HWND parent = nullptr;
  ERecoveredSaveSlotDialogMode mode = RECOVERED_SAVE_SLOT_DIALOG_SAVE;
  std::uint32_t slot = 0;
  bool occupied = false;
  bool readable = false;
  bool loadable = false;
  bool switchesLevel = false;
  std::string automaticTitle;
  std::string automaticDescription;
  SLevelSaveSlot archive;
};

struct SRecoveredSaveSlotDialogResult {
  bool accepted = false;
  bool previewAttempted = false;
  bool previewDisplayed = false;
  bool previewDecodeFailed = false;
  std::uint32_t slot = 0;
  std::string title;
  std::string description;
};

// Shows a small modal Windows slot-details sheet. Cancellation is a
// successful call with accepted=false; false means the UI itself failed.
bool RecoveredSaveSlotDialog_Show(
    const SRecoveredSaveSlotDialogInput& input,
    SRecoveredSaveSlotDialogResult* result, std::string* failure);
