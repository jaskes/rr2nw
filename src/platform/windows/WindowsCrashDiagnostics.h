#pragma once

#include <cstdint>
#include <string>

namespace rr2nw {

// Process-level owner for unexpected Windows SEH failures and unrecoverable
// CRT termination (SIGABRT, invalid-parameter and purecall). Normal runtime
// errors and explicit normal exits remain outside this contract.
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

// Bridges the archival RTCHECK/assert owner into the same local diagnostic
// bundle without treating typed runtime failures as crashes. Inputs are copied
// into fixed sanitized buffers; source paths are reduced to a basename.
void WindowsCrashDiagnostics_SetLegacyFatalContext(
    const char* assertion, const char* sourceFile, int sourceLine,
    const char* message);
[[noreturn]] void WindowsCrashDiagnostics_TriggerLegacyFatal();

// Hidden acceptance-only trigger. It deliberately bypasses ordinary and
// Developer UI and raises one noncontinuable SEH exception.
[[noreturn]] void WindowsCrashDiagnostics_TriggerControlledCrash();

constexpr unsigned long kWindowsCrashDiagnosticsControlledCode = 0xE0425252ul;
constexpr unsigned long kWindowsCrashDiagnosticsLegacyFatalCode = 0xE0425253ul;
constexpr unsigned long kWindowsCrashDiagnosticsCrtAbortCode = 0xE0425254ul;
constexpr unsigned long kWindowsCrashDiagnosticsCrtInvalidParameterCode =
    0xE0425255ul;
constexpr unsigned long kWindowsCrashDiagnosticsCrtPurecallCode = 0xE0425256ul;

}  // namespace rr2nw
