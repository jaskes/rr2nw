#pragma once

#include <cstdint>
#include <string>

namespace rr2nw {

// Process-level owner for unexpected Windows SEH failures. Normal runtime
// errors and explicit legacy fatal exits remain outside this contract.
bool WindowsCrashDiagnostics_Install(const std::wstring& diagnosticsDirectory,
                                     const char* version,
                                     const char* revision,
                                     const char* configuration);
void WindowsCrashDiagnostics_Uninstall();
bool WindowsCrashDiagnostics_IsInstalled();

void WindowsCrashDiagnostics_SetModIdentity(const char* identity,
                                            unsigned int count,
                                            std::uint64_t fingerprint);
void WindowsCrashDiagnostics_SetLevelIdentity(
    const char* identity, std::uint64_t contentFingerprint);
void WindowsCrashDiagnostics_SetSanitizedSettings(
    int windowMode, int windowScale, bool safeMode, bool developerMode,
    bool nativeDiagnosticMenu, double mouseSensitivityX,
    double mouseSensitivityY, bool mouseInvertY);
void WindowsCrashDiagnostics_SetRuntimeState(std::uint64_t frame,
                                             unsigned int activeActions,
                                             bool mapOpen, bool shellOpen);
void WindowsCrashDiagnostics_RecordBreadcrumb(const char* owner,
                                               const char* event);

// Hidden acceptance-only trigger. It deliberately bypasses ordinary and
// Developer UI and raises one noncontinuable SEH exception.
[[noreturn]] void WindowsCrashDiagnostics_TriggerControlledCrash();

constexpr unsigned long kWindowsCrashDiagnosticsControlledCode = 0xE0425252ul;

}  // namespace rr2nw
