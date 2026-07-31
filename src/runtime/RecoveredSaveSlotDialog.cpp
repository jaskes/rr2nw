#include "RecoveredSaveSlotDialog.h"

#include "RecoveredSavePreview.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kDialogClass[] = L"RR2NWRecoveredSaveSlotDialog";
constexpr int kClientWidth = 760;
constexpr int kClientHeight = 505;
constexpr int kPreviewLeft = 18;
constexpr int kPreviewTop = 54;
constexpr int kPreviewWidth = 464;
constexpr int kPreviewHeight = 348;
constexpr int kDetailsLeft = 506;
constexpr int kDetailsWidth = 232;
constexpr int kTitleEdit = 1001;
constexpr int kDescriptionEdit = 1002;

struct DialogState {
  const SRecoveredSaveSlotDialogInput* input = nullptr;
  SRecoveredSaveSlotDialogResult* output = nullptr;
  HWND window = nullptr;
  HWND titleEdit = nullptr;
  HWND descriptionEdit = nullptr;
  HBITMAP preview = nullptr;
  std::uint32_t previewWidth = 0;
  std::uint32_t previewHeight = 0;
  std::string liveTitle;
  std::string liveDescription;
  bool closed = false;
};

void SetFailure(std::string* failure, const char* detail) {
  if (failure != nullptr) *failure = detail;
}

std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring converted(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(
          CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
          static_cast<int>(text.size()), &converted[0], length) != length)
    return {};
  return converted;
}

bool WideToUtf8(const std::wstring& text, std::string* converted) {
  if (converted == nullptr) return false;
  converted->clear();
  if (text.empty()) return true;
  const int length = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return false;
  converted->resize(static_cast<std::size_t>(length));
  return WideCharToMultiByte(
             CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
             static_cast<int>(text.size()), &(*converted)[0], length,
             nullptr, nullptr) == length;
}

std::wstring ReadControlText(HWND control) {
  const int length = GetWindowTextLengthW(control);
  if (length <= 0) return {};
  std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1u);
  const int copied = GetWindowTextW(control, buffer.data(), length + 1);
  return copied > 0 ? std::wstring(buffer.data(), copied) : std::wstring();
}

std::wstring SavedUtc(std::uint64_t seconds) {
  if (seconds == 0u ||
      seconds > static_cast<std::uint64_t>(
                    (std::numeric_limits<std::time_t>::max)()))
    return L"unknown";
  const std::time_t value = static_cast<std::time_t>(seconds);
  std::tm utc = {};
  wchar_t text[64] = {};
  if (gmtime_s(&utc, &value) != 0 ||
      wcsftime(text, sizeof(text) / sizeof(text[0]),
               L"%Y-%m-%d %H:%M:%S UTC", &utc) == 0u)
    return L"unknown";
  return text;
}

void ApplyGuiFont(HWND control) {
  SendMessageW(control, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               TRUE);
}

HWND CreateControl(DialogState* state, const wchar_t* className,
                   const wchar_t* text, DWORD style, int x, int y,
                   int width, int height, int identifier = 0,
                   DWORD extendedStyle = 0) {
  HWND control = CreateWindowExW(
      extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
      x, y, width, height, state->window,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
      GetModuleHandleW(nullptr), nullptr);
  if (control != nullptr) ApplyGuiFont(control);
  return control;
}

std::wstring MetadataText(const SRecoveredSaveSlotDialogInput& input) {
  if (!input.occupied) {
    return L"Empty slot\r\n\r\nThe current frame will be captured after "
           L"the dialog closes and the active simulation frame reaches its "
           L"safe boundary.";
  }
  if (!input.readable) {
    return L"The slot file is corrupt or unsupported. It cannot be loaded, "
           L"but it may be replaced by a new RR2NW save.";
  }
  const SLevelSaveSlot& archive = input.archive;
  std::wstring text = L"Level: " + Utf8ToWide(archive.level) +
                      L"\r\nSaved: " + SavedUtc(archive.savedAtUnixSeconds) +
                      L"\r\nTick: " +
                      std::to_wstring(archive.simulationTick) +
                      L"\r\nSimulation: " +
                      std::to_wstring(archive.simulationTime) + L" s";
  if (input.switchesLevel)
    text += L"\r\n\r\nLoading will switch to this Level.";
  if (!input.loadable)
    text += L"\r\n\r\nThis save cannot be loaded with the current retail data.";
  else if (input.mode == RECOVERED_SAVE_SLOT_DIALOG_LOAD)
    text += L"\r\n\r\nUnsaved progress will be replaced.";
  return text;
}

bool CreatePreviewBitmap(const SRecoveredSavePreviewImage& image,
                         HBITMAP* bitmap) {
  if (bitmap == nullptr || !image.ready || image.bgra.empty()) return false;
  *bitmap = nullptr;
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = static_cast<LONG>(image.width);
  info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
  info.bmiHeader.biPlanes = 1u;
  info.bmiHeader.biBitCount = 32u;
  info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  HBITMAP created = CreateDIBSection(
      nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0u);
  if (created == nullptr || pixels == nullptr) {
    if (created != nullptr) DeleteObject(created);
    return false;
  }
  std::memcpy(pixels, image.bgra.data(), image.bgra.size());
  *bitmap = created;
  return true;
}

bool BuildControls(DialogState* state) {
  const SRecoveredSaveSlotDialogInput& input = *state->input;
  const std::wstring heading =
      std::wstring(input.mode == RECOVERED_SAVE_SLOT_DIALOG_SAVE
                       ? L"Save to slot "
                       : L"Load slot ") +
      std::to_wstring(input.slot + 1u);
  if (CreateControl(state, L"STATIC", heading.c_str(), SS_LEFT,
                    18, 16, 720, 25) == nullptr ||
      CreateControl(state, L"STATIC", L"Title", SS_LEFT,
                    kDetailsLeft, 54, kDetailsWidth, 18) == nullptr)
    return false;

  std::wstring title = input.readable ? Utf8ToWide(input.archive.title)
                                      : Utf8ToWide(input.automaticTitle);
  std::wstring description =
      input.readable ? Utf8ToWide(input.archive.description)
                     : Utf8ToWide(input.automaticDescription);
  const bool saveMode = input.mode == RECOVERED_SAVE_SLOT_DIALOG_SAVE;
  const DWORD editReadOnly = saveMode ? 0u : ES_READONLY;
  state->titleEdit = CreateControl(
      state, L"EDIT", title.c_str(), WS_TABSTOP | ES_AUTOHSCROLL |
          editReadOnly, kDetailsLeft, 74, kDetailsWidth, 25,
      kTitleEdit, WS_EX_CLIENTEDGE);
  if (state->titleEdit == nullptr) return false;
  SendMessageW(state->titleEdit, EM_SETLIMITTEXT, 1024u, 0);

  if (CreateControl(state, L"STATIC", L"Description", SS_LEFT,
                    kDetailsLeft, 110, kDetailsWidth, 18) == nullptr)
    return false;
  state->descriptionEdit = CreateControl(
      state, L"EDIT", description.c_str(), WS_TABSTOP | ES_MULTILINE |
          ES_AUTOVSCROLL | WS_VSCROLL | editReadOnly,
      kDetailsLeft, 130, kDetailsWidth, 86,
      kDescriptionEdit, WS_EX_CLIENTEDGE);
  if (state->descriptionEdit == nullptr) return false;
  SendMessageW(state->descriptionEdit, EM_SETLIMITTEXT, 4096u, 0);

  const std::wstring metadata = MetadataText(input);
  if (CreateControl(state, L"STATIC", metadata.c_str(),
                    SS_LEFT, kDetailsLeft, 230, kDetailsWidth, 172) == nullptr)
    return false;

  const wchar_t* primary = saveMode ? L"Save" : L"Load";
  HWND accept = CreateControl(
      state, L"BUTTON", primary,
      WS_TABSTOP | BS_DEFPUSHBUTTON, 546, 454, 88, 30, IDOK);
  HWND cancel = CreateControl(
      state, L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON,
      646, 454, 88, 30, IDCANCEL);
  if (accept == nullptr || cancel == nullptr) return false;
  if (!saveMode && !input.loadable) EnableWindow(accept, FALSE);
  return true;
}

void PaintPreview(DialogState* state) {
  PAINTSTRUCT paint = {};
  HDC target = BeginPaint(state->window, &paint);
  RECT previewRect = {kPreviewLeft, kPreviewTop,
                      kPreviewLeft + kPreviewWidth,
                      kPreviewTop + kPreviewHeight};
  FillRect(target, &previewRect,
           reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  FrameRect(target, &previewRect,
            reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
  if (state->preview != nullptr) {
    HDC memory = CreateCompatibleDC(target);
    if (memory != nullptr) {
      HGDIOBJ previous = SelectObject(memory, state->preview);
      const int left = kPreviewLeft +
          (kPreviewWidth - static_cast<int>(state->previewWidth)) / 2;
      const int top = kPreviewTop +
          (kPreviewHeight - static_cast<int>(state->previewHeight)) / 2;
      BitBlt(target, left, top, static_cast<int>(state->previewWidth),
             static_cast<int>(state->previewHeight), memory, 0, 0, SRCCOPY);
      SelectObject(memory, previous);
      DeleteDC(memory);
    }
  } else {
    SetBkMode(target, TRANSPARENT);
    SetTextColor(target, RGB(210, 210, 210));
    std::wstring placeholder =
        state->input->readable && !state->input->archive.previewPng.empty()
            ? L"Embedded preview could not be decoded"
            : L"No saved preview yet";
    DrawTextW(target, placeholder.c_str(), -1, &previewRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  EndPaint(state->window, &paint);
}

bool AcceptDialog(DialogState* state) {
  if (state->input->mode == RECOVERED_SAVE_SLOT_DIALOG_LOAD) {
    state->output->accepted = state->input->loadable;
    return state->output->accepted;
  }
  HWND titleEdit = GetDlgItem(state->window, kTitleEdit);
  HWND descriptionEdit = GetDlgItem(state->window, kDescriptionEdit);
  if (titleEdit == nullptr || descriptionEdit == nullptr) return false;
  const std::wstring wideTitle = ReadControlText(titleEdit);
  const std::wstring wideDescription = ReadControlText(descriptionEdit);
  SLevelSaveSlotStatus status;
  if (!WideToUtf8(wideTitle, &state->liveTitle) ||
      !WideToUtf8(wideDescription, &state->liveDescription) ||
      !LevelSaveSlot_ValidateDisplayMetadata(
          state->liveTitle, state->liveDescription, &status)) {
    const std::wstring detail = Utf8ToWide(status.detail);
    MessageBoxW(state->window,
                detail.empty()
                    ? L"Title or description is invalid. The title is "
                      L"required and UTF-8 limits are 96/1024 bytes."
                    : detail.c_str(),
                L"RR2NW save game", MB_OK | MB_ICONWARNING);
    SetFocus(titleEdit);
    return false;
  }
  state->output->title = state->liveTitle;
  state->output->description = state->liveDescription;
  state->output->accepted = true;
  return true;
}

LRESULT CALLBACK DialogWindowProc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam) {
  DialogState* state = reinterpret_cast<DialogState*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const CREATESTRUCTW* create =
        reinterpret_cast<const CREATESTRUCTW*>(lParam);
    state = static_cast<DialogState*>(create->lpCreateParams);
    state->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(state));
  }
  if (state == nullptr) return DefWindowProcW(window, message, wParam, lParam);

  switch (message) {
    case WM_CREATE:
      return BuildControls(state) ? 0 : -1;
    case WM_COMMAND:
      if (HIWORD(wParam) == EN_CHANGE &&
          (LOWORD(wParam) == kTitleEdit ||
           LOWORD(wParam) == kDescriptionEdit) &&
          state->input->mode == RECOVERED_SAVE_SLOT_DIALOG_SAVE) {
        HWND title = GetDlgItem(window, kTitleEdit);
        HWND description = GetDlgItem(window, kDescriptionEdit);
        if (title != nullptr)
          WideToUtf8(ReadControlText(title), &state->liveTitle);
        if (description != nullptr)
          WideToUtf8(ReadControlText(description), &state->liveDescription);
        return 0;
      }
      if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED) {
        if (AcceptDialog(state)) DestroyWindow(window);
        return 0;
      }
      if (LOWORD(wParam) == IDCANCEL && HIWORD(wParam) == BN_CLICKED) {
        DestroyWindow(window);
        return 0;
      }
      break;
    case WM_PAINT:
      PaintPreview(state);
      return 0;
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      state->closed = true;
      return 0;
    default:
      break;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterDialogClass(std::string* failure) {
  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = DialogWindowProc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.hIcon =
      LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
  windowClass.hCursor =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  windowClass.lpszClassName = kDialogClass;
  if (RegisterClassExW(&windowClass) != 0u ||
      GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
    return true;
  SetFailure(failure, "Windows could not register the save-slot dialog");
  return false;
}

}  // namespace

bool RecoveredSaveSlotDialog_Show(
    const SRecoveredSaveSlotDialogInput& input,
    SRecoveredSaveSlotDialogResult* result, std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (result == nullptr || input.parent == nullptr ||
      input.slot >= LevelSaveSlot_Count() ||
      (input.mode != RECOVERED_SAVE_SLOT_DIALOG_SAVE &&
       input.mode != RECOVERED_SAVE_SLOT_DIALOG_LOAD)) {
    SetFailure(failure, "save-slot dialog arguments are invalid");
    return false;
  }
  *result = {};
  result->slot = input.slot;
  if (!RegisterDialogClass(failure)) return false;

  DialogState state;
  state.input = &input;
  state.output = result;
  state.liveTitle = input.readable ? input.archive.title
                                   : input.automaticTitle;
  state.liveDescription = input.readable
                              ? input.archive.description
                              : input.automaticDescription;
  SRecoveredSavePreviewImage preview;
  if (input.readable && !input.archive.previewPng.empty()) {
    result->previewAttempted = true;
    std::string previewFailure;
    if (RecoveredSavePreview_DecodePng(
            input.archive.previewPng, kPreviewWidth - 4,
            kPreviewHeight - 4, &preview, &previewFailure) &&
        CreatePreviewBitmap(preview, &state.preview)) {
      result->previewDisplayed = true;
      state.previewWidth = preview.width;
      state.previewHeight = preview.height;
    } else {
      result->previewDecodeFailed = true;
    }
  }

  RECT outer = {0, 0, kClientWidth, kClientHeight};
  const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
  const DWORD extendedStyle = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
  if (AdjustWindowRectEx(&outer, style, FALSE, extendedStyle) == FALSE) {
    if (state.preview != nullptr) DeleteObject(state.preview);
    SetFailure(failure, "Windows could not size the save-slot dialog");
    return false;
  }
  RECT parentRect = {};
  GetWindowRect(input.parent, &parentRect);
  const int width = outer.right - outer.left;
  const int height = outer.bottom - outer.top;
  int left = parentRect.left +
      ((parentRect.right - parentRect.left) - width) / 2;
  int top = parentRect.top +
      ((parentRect.bottom - parentRect.top) - height) / 2;
  MONITORINFO monitor = {};
  monitor.cbSize = sizeof(monitor);
  if (GetMonitorInfoW(
          MonitorFromWindow(input.parent, MONITOR_DEFAULTTONEAREST),
          &monitor) != FALSE) {
    const int workLeft = static_cast<int>(monitor.rcWork.left);
    const int workTop = static_cast<int>(monitor.rcWork.top);
    const int workRight = static_cast<int>(monitor.rcWork.right);
    const int workBottom = static_cast<int>(monitor.rcWork.bottom);
    left = (std::max)(workLeft,
                      (std::min)(left, workRight - width));
    top = (std::max)(workTop,
                     (std::min)(top, workBottom - height));
  }
  const std::wstring caption =
      std::wstring(L"RR2NW - ") +
      (input.mode == RECOVERED_SAVE_SLOT_DIALOG_SAVE
           ? L"Save game"
           : L"Load game");
  HWND window = CreateWindowExW(
      extendedStyle, kDialogClass, caption.c_str(), style,
      left, top, width, height, input.parent, nullptr,
      GetModuleHandleW(nullptr), &state);
  if (window == nullptr) {
    if (state.preview != nullptr) DeleteObject(state.preview);
    SetFailure(failure, "Windows could not create the save-slot dialog");
    return false;
  }

  const BOOL parentWasEnabled = IsWindowEnabled(input.parent);
  if (parentWasEnabled) EnableWindow(input.parent, FALSE);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  MSG message = {};
  bool messageFailure = false;
  while (!state.closed && IsWindow(window)) {
    const BOOL received = GetMessageW(&message, nullptr, 0u, 0u);
    if (received == -1) {
      messageFailure = true;
      DestroyWindow(window);
      break;
    }
    if (received == 0) {
      PostQuitMessage(static_cast<int>(message.wParam));
      DestroyWindow(window);
      break;
    }
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  if (parentWasEnabled && IsWindow(input.parent)) {
    EnableWindow(input.parent, TRUE);
    SetActiveWindow(input.parent);
  }
  if (state.preview != nullptr) DeleteObject(state.preview);
  if (messageFailure) {
    SetFailure(failure, "Windows save-slot dialog message loop failed");
    return false;
  }
  return true;
}
