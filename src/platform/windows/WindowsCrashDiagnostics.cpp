#include "WindowsCrashDiagnostics.h"

#include <windows.h>
#include <dbghelp.h>
#include <winternl.h>

#include <array>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

namespace rr2nw {
namespace {

constexpr std::size_t kPathCapacity = 32768u;
constexpr std::size_t kManifestCapacity = 64u * 1024u;
constexpr std::size_t kIdentityCapacity = 96u;
constexpr std::size_t kBreadcrumbCapacity = 16u;
constexpr std::size_t kBreadcrumbTextCapacity = 160u;
constexpr ULONG kStackGuaranteeBytes = 128u * 1024u;

struct CrashContext {
  char level[kIdentityCapacity] = "unavailable";
  char mod[kIdentityCapacity] = "base";
  std::uint64_t contentFingerprint = 0;
  std::uint64_t modFingerprint = 0;
  std::uint64_t frame = 0;
  unsigned int modCount = 0;
  unsigned int activeActions = 0;
  int windowMode = 0;
  int windowScale = 1;
  double mouseSensitivityX = 0.5;
  double mouseSensitivityY = 0.5;
  bool mouseInvertY = false;
  bool safeMode = false;
  bool developerMode = false;
  bool nativeDiagnosticMenu = false;
  bool mapOpen = false;
  bool shellOpen = false;
};

struct CrashBreadcrumb {
  volatile LONG sequence = 0;
  char owner[48] = {};
  char event[kBreadcrumbTextCapacity] = {};
};

struct CrashOwner {
  bool installed = false;
  wchar_t bundleDirectory[kPathCapacity] = {};
  wchar_t manifestPath[kPathCapacity] = {};
  wchar_t manifestTemporaryPath[kPathCapacity] = {};
  wchar_t dumpPath[kPathCapacity] = {};
  char bundleName[128] = {};
  char version[32] = "unknown";
  char revision[64] = "unknown";
  char configuration[32] = "unknown";
  char compiler[48] = "unknown";
  char osVersion[64] = "unknown";
  char nativeArchitecture[24] = "unknown";
  char pdbSignature[48] = "unavailable";
  char pdbName[64] = "rr2nw.pdb";
  DWORD pdbAge = 0;
  DWORD imageTimestamp = 0;
  DWORD imageSize = 0;
  bool stackGuaranteeReady = false;
  bool pdbPresent = false;
  bool mapPresent = false;
  LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
  volatile LONG writing = 0;
  volatile LONG activeContext = 0;
  volatile LONG nextBreadcrumb = 0;
  bool legacyFatal = false;
  int legacyFatalLine = 0;
  char legacyFatalKind[24] = "none";
  char legacyFatalAssertion[160] = "unavailable";
  char legacyFatalSource[96] = "unavailable";
  char legacyFatalMessage[256] = "unavailable";
  std::array<CrashContext, 2> contexts = {};
  std::array<CrashBreadcrumb, kBreadcrumbCapacity> breadcrumbs = {};
};

CrashOwner g_owner;

void ResetOwner() {
  g_owner.~CrashOwner();
  new (&g_owner) CrashOwner();
}

void CopySanitized(char* destination, std::size_t capacity,
                   const char* source) {
  if (destination == nullptr || capacity == 0u) return;
  destination[0] = '\0';
  if (source == nullptr) return;
  std::size_t written = 0u;
  for (const unsigned char* cursor =
           reinterpret_cast<const unsigned char*>(source);
       *cursor != 0 && written + 1u < capacity; ++cursor) {
    unsigned char value = *cursor;
    if (value < 0x20u || value == 0x7fu || value == '\\' || value == '/' ||
        value == ':' || value == '=' || value == '|')
      value = '_';
    destination[written++] = static_cast<char>(value);
  }
  destination[written] = '\0';
}

void CopySourceBasename(char* destination, std::size_t capacity,
                        const char* source) {
  const char* basename = source;
  if (source != nullptr) {
    for (const char* cursor = source; *cursor != '\0'; ++cursor)
      if (*cursor == '\\' || *cursor == '/') basename = cursor + 1;
  }
  CopySanitized(destination, capacity, basename);
}

bool JoinPath(const wchar_t* base, const wchar_t* child,
              wchar_t* destination, std::size_t capacity) {
  if (base == nullptr || child == nullptr || destination == nullptr ||
      capacity == 0u)
    return false;
  const std::size_t baseLength = std::wcslen(base);
  const bool separator = baseLength != 0u && base[baseLength - 1u] != L'\\' &&
                         base[baseLength - 1u] != L'/';
  const int result = _snwprintf_s(destination, capacity, _TRUNCATE,
                                  separator ? L"%ls\\%ls" : L"%ls%ls",
                                  base, child);
  return result >= 0;
}

const char* ProcessArchitecture() {
#if defined(_M_IX86)
  return "x86";
#elif defined(_M_X64)
  return "x64";
#elif defined(_M_ARM64)
  return "arm64";
#else
  return "unknown";
#endif
}

void CaptureSystemIdentity() {
  using RtlGetVersionProc = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  const auto rtlGetVersion = ntdll == nullptr
      ? nullptr
      : reinterpret_cast<RtlGetVersionProc>(
            GetProcAddress(ntdll, "RtlGetVersion"));
  RTL_OSVERSIONINFOW version = {};
  version.dwOSVersionInfoSize = sizeof(version);
  if (rtlGetVersion != nullptr && rtlGetVersion(&version) == 0) {
    std::snprintf(g_owner.osVersion, sizeof(g_owner.osVersion), "%lu.%lu.%lu",
                  version.dwMajorVersion, version.dwMinorVersion,
                  version.dwBuildNumber);
  }

  SYSTEM_INFO system = {};
  GetNativeSystemInfo(&system);
  const char* architecture = "unknown";
  if (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
    architecture = "x64";
  else if (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    architecture = "x86";
  else if (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
    architecture = "arm64";
  CopySanitized(g_owner.nativeArchitecture,
                sizeof(g_owner.nativeArchitecture), architecture);
}

void CaptureSymbolAvailability() {
  const HMODULE module = GetModuleHandleW(nullptr);
  if (module != nullptr) {
    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(module);
    const IMAGE_DOS_HEADER* dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
      const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
          base + static_cast<std::size_t>(dos->e_lfanew));
      if (nt->Signature == IMAGE_NT_SIGNATURE) {
        g_owner.imageTimestamp = nt->FileHeader.TimeDateStamp;
        g_owner.imageSize = nt->OptionalHeader.SizeOfImage;
        const IMAGE_DATA_DIRECTORY& directory =
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (directory.VirtualAddress != 0u &&
            directory.Size >= sizeof(IMAGE_DEBUG_DIRECTORY) &&
            directory.VirtualAddress <= g_owner.imageSize &&
            directory.Size <= g_owner.imageSize - directory.VirtualAddress) {
          const IMAGE_DEBUG_DIRECTORY* entries =
              reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
                  base + directory.VirtualAddress);
          const std::size_t count =
              directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
          for (std::size_t index = 0u; index < count; ++index) {
            const IMAGE_DEBUG_DIRECTORY& entry = entries[index];
            if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
                entry.AddressOfRawData == 0u || entry.SizeOfData < 25u ||
                entry.AddressOfRawData > g_owner.imageSize ||
                entry.SizeOfData > g_owner.imageSize - entry.AddressOfRawData)
              continue;
            const unsigned char* record = base + entry.AddressOfRawData;
            DWORD signature = 0u;
            std::memcpy(&signature, record, sizeof(signature));
            if (signature != 0x53445352u) continue;  // RSDS
            GUID guid = {};
            DWORD age = 0u;
            std::memcpy(&guid, record + 4u, sizeof(guid));
            std::memcpy(&age, record + 20u, sizeof(age));
            std::snprintf(
                g_owner.pdbSignature, sizeof(g_owner.pdbSignature),
                "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
                guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                guid.Data4[5], guid.Data4[6], guid.Data4[7]);
            g_owner.pdbAge = age;
            const char* embeddedName =
                reinterpret_cast<const char*>(record + 24u);
            const std::size_t nameCapacity = entry.SizeOfData - 24u;
            const void* terminator = std::memchr(embeddedName, '\0',
                                                 nameCapacity);
            if (terminator != nullptr) {
              const char* name = embeddedName;
              for (const char* cursor = embeddedName;
                   cursor < static_cast<const char*>(terminator); ++cursor) {
                if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
              }
              CopySanitized(g_owner.pdbName, sizeof(g_owner.pdbName), name);
            }
            break;
          }
        }
      }
    }
  }

  wchar_t executable[kPathCapacity] = {};
  const DWORD length = GetModuleFileNameW(nullptr, executable,
                                          static_cast<DWORD>(kPathCapacity));
  if (length == 0u || length >= kPathCapacity) return;
  wchar_t* extension = std::wcsrchr(executable, L'.');
  if (extension == nullptr) return;
  _snwprintf_s(extension, kPathCapacity - (extension - executable), _TRUNCATE,
               L".pdb");
  g_owner.pdbPresent =
      GetFileAttributesW(executable) != INVALID_FILE_ATTRIBUTES;
  _snwprintf_s(extension, kPathCapacity - (extension - executable), _TRUNCATE,
               L".map");
  g_owner.mapPresent =
      GetFileAttributesW(executable) != INVALID_FILE_ATTRIBUTES;
}

CrashContext CurrentContext() {
  const LONG index = InterlockedCompareExchange(&g_owner.activeContext, 0, 0);
  return g_owner.contexts[index == 0 ? 0u : 1u];
}

void PublishContext(const CrashContext& context) {
  const LONG current =
      InterlockedCompareExchange(&g_owner.activeContext, 0, 0);
  const LONG target = current == 0 ? 1 : 0;
  g_owner.contexts[static_cast<std::size_t>(target)] = context;
  MemoryBarrier();
  InterlockedExchange(&g_owner.activeContext, target);
}

bool Append(char* destination, std::size_t capacity, std::size_t* length,
            const char* format, ...) {
  if (destination == nullptr || length == nullptr || *length >= capacity)
    return false;
  va_list arguments;
  va_start(arguments, format);
  const int result = _vsnprintf_s(destination + *length, capacity - *length,
                                  _TRUNCATE, format, arguments);
  va_end(arguments);
  if (result < 0) {
    *length = capacity;
    return false;
  }
  *length += static_cast<std::size_t>(result);
  return true;
}

bool WriteAll(HANDLE file, const void* bytes, DWORD size) {
  const unsigned char* cursor = static_cast<const unsigned char*>(bytes);
  DWORD remaining = size;
  while (remaining != 0u) {
    DWORD written = 0u;
    if (WriteFile(file, cursor, remaining, &written, nullptr) == FALSE ||
        written == 0u)
      return false;
    cursor += written;
    remaining -= written;
  }
  return true;
}

bool WriteManifestAtomic(const char* bytes, DWORD size) {
  HANDLE file = CreateFileW(g_owner.manifestTemporaryPath, GENERIC_WRITE, 0,
                            nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  const bool written = WriteAll(file, bytes, size) &&
                       FlushFileBuffers(file) != FALSE;
  CloseHandle(file);
  if (!written) {
    DeleteFileW(g_owner.manifestTemporaryPath);
    return false;
  }
  if (MoveFileExW(g_owner.manifestTemporaryPath, g_owner.manifestPath,
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
    DeleteFileW(g_owner.manifestTemporaryPath);
    return false;
  }
  return true;
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* exceptionPointers) {
  if (!g_owner.installed ||
      InterlockedCompareExchange(&g_owner.writing, 1, 0) != 0)
    return EXCEPTION_EXECUTE_HANDLER;

  const bool directoryReady =
      CreateDirectoryW(g_owner.bundleDirectory, nullptr) != FALSE ||
      GetLastError() == ERROR_ALREADY_EXISTS;
  bool dumpWritten = false;
  DWORD dumpError = ERROR_PATH_NOT_FOUND;
  std::uint64_t dumpBytes = 0;
  if (directoryReady) {
    HANDLE dump = CreateFileW(g_owner.dumpPath, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION information = {};
      information.ThreadId = GetCurrentThreadId();
      information.ExceptionPointers = exceptionPointers;
      information.ClientPointers = FALSE;
      dumpWritten = MiniDumpWriteDump(
          GetCurrentProcess(), GetCurrentProcessId(), dump, MiniDumpNormal,
          exceptionPointers == nullptr ? nullptr : &information,
          nullptr, nullptr) != FALSE;
      dumpError = dumpWritten ? ERROR_SUCCESS : GetLastError();
      if (dumpWritten) {
        LARGE_INTEGER size = {};
        if (GetFileSizeEx(dump, &size) != FALSE)
          dumpBytes = static_cast<std::uint64_t>(size.QuadPart);
        FlushFileBuffers(dump);
      }
      CloseHandle(dump);
      if (!dumpWritten) DeleteFileW(g_owner.dumpPath);
    } else {
      dumpError = GetLastError();
    }
  }

  char manifest[kManifestCapacity] = {};
  std::size_t length = 0u;
  SYSTEMTIME utc = {};
  GetSystemTime(&utc);
  const CrashContext context = CurrentContext();
  const DWORD exceptionCode = exceptionPointers != nullptr &&
                                      exceptionPointers->ExceptionRecord != nullptr
                                  ? exceptionPointers->ExceptionRecord
                                        ->ExceptionCode
                                  : 0u;
  const void* exceptionAddress =
      exceptionPointers != nullptr &&
              exceptionPointers->ExceptionRecord != nullptr
          ? exceptionPointers->ExceptionRecord->ExceptionAddress
          : nullptr;

  bool complete =
      Append(manifest, sizeof(manifest), &length, "format=RR2CRASH1\r\n") &&
      Append(manifest, sizeof(manifest), &length,
             "timestamp_utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\r\n",
             utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute,
             utc.wSecond, utc.wMilliseconds) &&
      Append(manifest, sizeof(manifest), &length, "bundle=%s\r\n",
             g_owner.bundleName) &&
      Append(manifest, sizeof(manifest), &length, "version=%s\r\n",
             g_owner.version) &&
      Append(manifest, sizeof(manifest), &length, "revision=%s\r\n",
             g_owner.revision) &&
      Append(manifest, sizeof(manifest), &length, "configuration=%s\r\n",
             g_owner.configuration) &&
      Append(manifest, sizeof(manifest), &length, "compiler=%s\r\n",
             g_owner.compiler) &&
      Append(manifest, sizeof(manifest), &length,
             "process_architecture=%s\r\nnative_architecture=%s\r\n",
             ProcessArchitecture(), g_owner.nativeArchitecture) &&
      Append(manifest, sizeof(manifest), &length, "os_version=%s\r\n",
             g_owner.osVersion) &&
      Append(manifest, sizeof(manifest), &length,
             "process_id=%lu\r\nthread_id=%lu\r\n",
             GetCurrentProcessId(), GetCurrentThreadId()) &&
      Append(manifest, sizeof(manifest), &length,
             "main_thread_stack_guarantee=%d\r\n"
             "main_thread_stack_guarantee_bytes=%lu\r\n",
             g_owner.stackGuaranteeReady ? 1 : 0,
             kStackGuaranteeBytes) &&
      Append(manifest, sizeof(manifest), &length,
             "exception_code=0x%08lX\r\nexception_address=%p\r\n",
             exceptionCode, const_cast<void*>(exceptionAddress)) &&
      Append(manifest, sizeof(manifest), &length,
             "minidump=crash.dmp\r\nminidump_type=MiniDumpNormal\r\n"
             "minidump_written=%d\r\nminidump_bytes=%llu\r\n"
             "minidump_error=%lu\r\n",
             dumpWritten ? 1 : 0,
             static_cast<unsigned long long>(dumpBytes), dumpError) &&
      Append(manifest, sizeof(manifest), &length,
             "image_timestamp=0x%08lX\r\nimage_size=%lu\r\n"
             "symbol_pdb=%s\r\nsymbol_pdb_signature=%s\r\n"
             "symbol_pdb_age=%lu\r\nsymbol_pdb_present=%d\r\n"
             "symbol_map=rr2nw.map\r\nsymbol_map_present=%d\r\n",
             g_owner.imageTimestamp, g_owner.imageSize, g_owner.pdbName,
             g_owner.pdbSignature, g_owner.pdbAge,
             g_owner.pdbPresent ? 1 : 0, g_owner.mapPresent ? 1 : 0) &&
      Append(manifest, sizeof(manifest), &length,
             "level=%s\r\ncontent_fingerprint=%llu\r\n"
             "mod_identity=%s\r\nmod_count=%u\r\nmod_fingerprint=%llu\r\n",
             context.level,
             static_cast<unsigned long long>(context.contentFingerprint),
             context.mod, context.modCount,
             static_cast<unsigned long long>(context.modFingerprint)) &&
      Append(manifest, sizeof(manifest), &length,
             "settings_sanitized=1\r\nsettings_window_mode=%d\r\n"
             "settings_window_scale=%d\r\nsettings_safe_mode=%d\r\n"
             "settings_developer_mode=%d\r\n"
             "settings_native_diagnostic_menu=%d\r\n"
             "settings_mouse_x=%.3f\r\nsettings_mouse_y=%.3f\r\n"
             "settings_mouse_invert_y=%d\r\n",
             context.windowMode, context.windowScale,
             context.safeMode ? 1 : 0, context.developerMode ? 1 : 0,
             context.nativeDiagnosticMenu ? 1 : 0,
             context.mouseSensitivityX, context.mouseSensitivityY,
             context.mouseInvertY ? 1 : 0) &&
      Append(manifest, sizeof(manifest), &length,
             "runtime_frame=%llu\r\nruntime_active_actions=%u\r\n"
             "runtime_map_open=%d\r\nruntime_shell_open=%d\r\n",
             static_cast<unsigned long long>(context.frame),
             context.activeActions, context.mapOpen ? 1 : 0,
             context.shellOpen ? 1 : 0) &&
      Append(manifest, sizeof(manifest), &length,
             "legacy_fatal=%d\r\nlegacy_fatal_kind=%s\r\n"
             "legacy_fatal_assertion=%s\r\nlegacy_fatal_source=%s\r\n"
             "legacy_fatal_line=%d\r\nlegacy_fatal_message=%s\r\n",
             g_owner.legacyFatal ? 1 : 0, g_owner.legacyFatalKind,
             g_owner.legacyFatalAssertion, g_owner.legacyFatalSource,
             g_owner.legacyFatalLine, g_owner.legacyFatalMessage);

  const LONG newestSequence =
      InterlockedCompareExchange(&g_owner.nextBreadcrumb, 0, 0);
  const LONG oldestSequence =
      newestSequence <= static_cast<LONG>(kBreadcrumbCapacity)
          ? 1
          : newestSequence - static_cast<LONG>(kBreadcrumbCapacity) + 1;
  const unsigned int breadcrumbCount = newestSequence <= 0
      ? 0u
      : static_cast<unsigned int>(newestSequence - oldestSequence + 1);
  complete = complete &&
      Append(manifest, sizeof(manifest), &length, "breadcrumb_count=%u\r\n",
             breadcrumbCount);
  unsigned int outputIndex = 0u;
  for (LONG expected = oldestSequence; expected <= newestSequence;
       ++expected) {
    const std::size_t index = static_cast<std::size_t>(
        (expected - 1) % static_cast<LONG>(kBreadcrumbCapacity));
    CrashBreadcrumb& breadcrumb = g_owner.breadcrumbs[index];
    const LONG sequence =
        InterlockedCompareExchange(&breadcrumb.sequence, 0, 0);
    if (sequence != expected) continue;
    complete = complete &&
        Append(manifest, sizeof(manifest), &length,
               "breadcrumb_%u=%ld|%s|%s\r\n", outputIndex++, sequence,
               breadcrumb.owner, breadcrumb.event);
  }
  complete = complete && Append(manifest, sizeof(manifest), &length,
                                "manifest_complete=1\r\n");
  const bool manifestWritten =
      directoryReady && complete && length < sizeof(manifest) &&
      WriteManifestAtomic(manifest, static_cast<DWORD>(length));
  if (!manifestWritten)
    OutputDebugStringA("RR2NW crash bundle manifest could not be committed\n");

  if (!dumpWritten && !manifestWritten) {
    if (g_owner.previousFilter != nullptr &&
        g_owner.previousFilter != CrashFilter)
      return g_owner.previousFilter(exceptionPointers);
    return EXCEPTION_CONTINUE_SEARCH;
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

bool WindowsCrashDiagnostics_Install(const std::wstring& diagnosticsDirectory,
                                     const char* version,
                                     const char* revision,
                                     const char* configuration) {
  if (g_owner.installed || diagnosticsDirectory.empty() ||
      diagnosticsDirectory.size() >= kPathCapacity)
    return false;

  ResetOwner();
  CopySanitized(g_owner.version, sizeof(g_owner.version), version);
  CopySanitized(g_owner.revision, sizeof(g_owner.revision), revision);
  CopySanitized(g_owner.configuration, sizeof(g_owner.configuration),
                configuration);
#if defined(_MSC_FULL_VER)
  std::snprintf(g_owner.compiler, sizeof(g_owner.compiler), "msvc-%ld",
                static_cast<long>(_MSC_FULL_VER));
#endif
  CaptureSystemIdentity();
  CaptureSymbolAvailability();
  ULONG stackGuarantee = kStackGuaranteeBytes;
  g_owner.stackGuaranteeReady =
      SetThreadStackGuarantee(&stackGuarantee) != FALSE;

  SYSTEMTIME utc = {};
  GetSystemTime(&utc);
  wchar_t bundleName[128] = {};
  _snwprintf_s(bundleName, sizeof(bundleName) / sizeof(bundleName[0]),
               _TRUNCATE,
               L"crash-%04u%02u%02uT%02u%02u%02u-%03u-p%lu",
               utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute,
               utc.wSecond, utc.wMilliseconds, GetCurrentProcessId());
  char bundleNameUtf8[128] = {};
  WideCharToMultiByte(CP_UTF8, 0, bundleName, -1, bundleNameUtf8,
                      static_cast<int>(sizeof(bundleNameUtf8)), nullptr,
                      nullptr);
  CopySanitized(g_owner.bundleName, sizeof(g_owner.bundleName), bundleNameUtf8);
  if (!JoinPath(diagnosticsDirectory.c_str(), bundleName,
                g_owner.bundleDirectory, kPathCapacity) ||
      !JoinPath(g_owner.bundleDirectory, L"manifest.txt",
                g_owner.manifestPath, kPathCapacity) ||
      !JoinPath(g_owner.bundleDirectory, L"manifest.tmp",
                g_owner.manifestTemporaryPath, kPathCapacity) ||
      !JoinPath(g_owner.bundleDirectory, L"crash.dmp", g_owner.dumpPath,
                kPathCapacity)) {
    ResetOwner();
    return false;
  }

  g_owner.contexts[0] = {};
  g_owner.contexts[1] = g_owner.contexts[0];
  g_owner.previousFilter = SetUnhandledExceptionFilter(CrashFilter);
  g_owner.installed = true;
  WindowsCrashDiagnostics_RecordBreadcrumb("process", "crash-owner-installed");
  return true;
}

void WindowsCrashDiagnostics_Uninstall() {
  if (!g_owner.installed) return;
  SetUnhandledExceptionFilter(g_owner.previousFilter);
  g_owner.installed = false;
}

bool WindowsCrashDiagnostics_IsInstalled() { return g_owner.installed; }

void WindowsCrashDiagnostics_SetModIdentity(const char* identity,
                                            unsigned int count,
                                            std::uint64_t fingerprint) {
  CrashContext context = CurrentContext();
  CopySanitized(context.mod, sizeof(context.mod),
                identity == nullptr || identity[0] == '\0' ? "base" : identity);
  context.modCount = count;
  context.modFingerprint = fingerprint;
  PublishContext(context);
}

void WindowsCrashDiagnostics_SetLevelIdentity(
    const char* identity, std::uint64_t contentFingerprint) {
  CrashContext context = CurrentContext();
  CopySanitized(context.level, sizeof(context.level), identity);
  context.contentFingerprint = contentFingerprint;
  PublishContext(context);
}

void WindowsCrashDiagnostics_SetSanitizedSettings(
    int windowMode, int windowScale, bool safeMode, bool developerMode,
    bool nativeDiagnosticMenu, double mouseSensitivityX,
    double mouseSensitivityY, bool mouseInvertY) {
  CrashContext context = CurrentContext();
  context.windowMode = windowMode;
  context.windowScale = windowScale;
  context.safeMode = safeMode;
  context.developerMode = developerMode;
  context.nativeDiagnosticMenu = nativeDiagnosticMenu;
  context.mouseSensitivityX = mouseSensitivityX;
  context.mouseSensitivityY = mouseSensitivityY;
  context.mouseInvertY = mouseInvertY;
  PublishContext(context);
}

void WindowsCrashDiagnostics_SetRuntimeState(std::uint64_t frame,
                                             unsigned int activeActions,
                                             bool mapOpen, bool shellOpen) {
  CrashContext context = CurrentContext();
  context.frame = frame;
  context.activeActions = activeActions;
  context.mapOpen = mapOpen;
  context.shellOpen = shellOpen;
  PublishContext(context);
}

void WindowsCrashDiagnostics_RecordBreadcrumb(const char* owner,
                                               const char* event) {
  if (!g_owner.installed) return;
  const LONG sequence = InterlockedIncrement(&g_owner.nextBreadcrumb);
  const std::size_t index = static_cast<std::size_t>(
      (sequence - 1) % static_cast<LONG>(kBreadcrumbCapacity));
  CrashBreadcrumb& breadcrumb = g_owner.breadcrumbs[index];
  InterlockedExchange(&breadcrumb.sequence, 0);
  CopySanitized(breadcrumb.owner, sizeof(breadcrumb.owner), owner);
  CopySanitized(breadcrumb.event, sizeof(breadcrumb.event), event);
  MemoryBarrier();
  InterlockedExchange(&breadcrumb.sequence, sequence);
}

void WindowsCrashDiagnostics_SetLegacyFatalContext(
    const char* assertion, const char* sourceFile, int sourceLine,
    const char* message) {
  if (!g_owner.installed) return;
  g_owner.legacyFatal = true;
  g_owner.legacyFatalLine = sourceLine < 0 ? 0 : sourceLine;
  CopySanitized(g_owner.legacyFatalKind, sizeof(g_owner.legacyFatalKind),
                assertion == nullptr || assertion[0] == '\0'
                    ? "runtime"
                    : "assertion");
  CopySanitized(g_owner.legacyFatalAssertion,
                sizeof(g_owner.legacyFatalAssertion),
                assertion == nullptr || assertion[0] == '\0'
                    ? "unavailable"
                    : assertion);
  CopySourceBasename(g_owner.legacyFatalSource,
                     sizeof(g_owner.legacyFatalSource),
                     sourceFile == nullptr || sourceFile[0] == '\0'
                         ? "unavailable"
                         : sourceFile);
  CopySanitized(g_owner.legacyFatalMessage,
                sizeof(g_owner.legacyFatalMessage),
                message == nullptr || message[0] == '\0'
                    ? "unavailable"
                    : message);
  WindowsCrashDiagnostics_RecordBreadcrumb("legacy-fatal",
                                           g_owner.legacyFatalMessage);
}

[[noreturn]] void WindowsCrashDiagnostics_TriggerLegacyFatal() {
  WindowsCrashDiagnostics_RecordBreadcrumb("legacy-fatal",
                                           "raising-diagnostic-exception");
  SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS |
               SEM_NOGPFAULTERRORBOX);
  RaiseException(kWindowsCrashDiagnosticsLegacyFatalCode,
                 EXCEPTION_NONCONTINUABLE, 0u, nullptr);
  TerminateProcess(GetCurrentProcess(),
                   kWindowsCrashDiagnosticsLegacyFatalCode);
  __assume(0);
}

[[noreturn]] void WindowsCrashDiagnostics_TriggerControlledCrash() {
  WindowsCrashDiagnostics_RecordBreadcrumb("acceptance",
                                           "controlled-crash-trigger");
  SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS |
               SEM_NOGPFAULTERRORBOX);
  RaiseException(kWindowsCrashDiagnosticsControlledCode,
                 EXCEPTION_NONCONTINUABLE, 0u, nullptr);
  TerminateProcess(GetCurrentProcess(),
                   kWindowsCrashDiagnosticsControlledCode);
  __assume(0);
}

}  // namespace rr2nw
