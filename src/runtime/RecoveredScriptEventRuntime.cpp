#include "RecoveredScriptEventRuntime.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ActiveWorldSemanticEvents.h"
#include "RecoveredModRuntime.h"
#include "kernel/h/context.h"
#include "message/explmsg.h"
#include "message/SPARKMSG.H"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/spark/SparkSubjectState.h"
#include "storage/h/subject.h"

namespace {

constexpr const char kEventTarget[] = "RR2NW/script-events.json";
constexpr const char kObjectPrefix[] = "RR2NW.Event.";
constexpr long kMaximumDocumentBytes = 256 * 1024;
constexpr int kSchemaVersion = 1;
constexpr std::size_t kMaximumEvents = 32;
constexpr std::size_t kMaximumEventId = 47;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

enum class EventKind { Explosion, Spark };

struct EventDefinition {
  std::string id;
  EventKind kind = EventKind::Explosion;
  std::string attribute;
  double position[3] = {};
  double delay = 0.0;
};

struct Document {
  int schema = 0;
  std::vector<EventDefinition> events;
};

struct QueuedEvent {
  EventKind kind;
  KR_ObjectID object;
};

SRecoveredScriptEventSummary g_summary;
std::vector<QueuedEvent> g_queued;
SimulationContext* g_context = nullptr;
unsigned int g_issues = 0;
char g_lastError[512] = {};
bool g_active = false;

void Fail(unsigned int issue, const std::string& message) {
  g_issues |= issue;
  std::snprintf(g_lastError, sizeof(g_lastError), "%s", message.c_str());
}

void ClearState() {
  g_summary = SRecoveredScriptEventSummary();
  g_queued.clear();
  g_context = nullptr;
  g_issues = 0;
  g_lastError[0] = '\0';
  g_active = false;
}

std::string FoldAscii(std::string value) {
  for (char& character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (byte >= 'A' && byte <= 'Z')
      character = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

bool ValidIdentifier(const std::string& value, std::size_t maximum) {
  if (value.empty() || value.size() > maximum) return false;
  for (char character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (!((byte >= 'A' && byte <= 'Z') ||
          (byte >= 'a' && byte <= 'z') ||
          (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' ||
          byte == '-'))
      return false;
  }
  return true;
}

bool InRange(double value, double minimum, double maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

class JsonCursor {
 public:
  explicit JsonCursor(const std::string& text) : text_(text) {}

  bool Consume(char expected) {
    Space();
    if (position_ >= text_.size() || text_[position_] != expected)
      return Error("expected JSON punctuation");
    ++position_;
    return true;
  }

  bool TryConsume(char value) {
    Space();
    if (position_ >= text_.size() || text_[position_] != value) return false;
    ++position_;
    return true;
  }

  bool Integer(int* result) {
    double value = 0.0;
    const std::size_t begin = position_;
    if (!Number(&value)) return false;
    if (std::floor(value) != value || value < -2147483648.0 ||
        value > 2147483647.0) {
      position_ = begin;
      return Error("expected bounded JSON integer");
    }
    *result = static_cast<int>(value);
    return true;
  }

  bool Number(double* result) {
    Space();
    const std::size_t begin = position_;
    if (position_ < text_.size() && text_[position_] == '-') ++position_;
    if (position_ >= text_.size()) return Error("expected JSON number");
    if (text_[position_] == '0') {
      ++position_;
      if (position_ < text_.size() &&
          std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        return Error("JSON number has a leading zero");
    } else {
      if (text_[position_] < '1' || text_[position_] > '9')
        return Error("expected JSON number");
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
    }
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      const std::size_t fraction = position_;
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
      if (fraction == position_) return Error("JSON fraction has no digits");
    }
    if (position_ < text_.size() &&
        (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() &&
          (text_[position_] == '+' || text_[position_] == '-'))
        ++position_;
      const std::size_t exponent = position_;
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
      if (exponent == position_) return Error("JSON exponent has no digits");
    }
    const std::string token = text_.substr(begin, position_ - begin);
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (errno != 0 || end == token.c_str() || *end != '\0' ||
        !std::isfinite(value))
      return Error("JSON number is not finite");
    *result = value;
    return true;
  }

  bool String(std::string* result) {
    Space();
    if (position_ >= text_.size() || text_[position_] != '"')
      return Error("expected JSON string");
    ++position_;
    result->clear();
    while (position_ < text_.size()) {
      const unsigned char character =
          static_cast<unsigned char>(text_[position_++]);
      if (character == '"') return true;
      if (character < 0x20u || character > 0x7eu)
        return Error("script-event strings must use printable ASCII");
      if (character != '\\') {
        result->push_back(static_cast<char>(character));
        continue;
      }
      if (position_ >= text_.size()) return Error("unterminated JSON escape");
      const char escaped = text_[position_++];
      if (escaped == '"' || escaped == '\\' || escaped == '/')
        result->push_back(escaped);
      else
        return Error("unsupported JSON string escape");
    }
    return Error("unterminated JSON string");
  }

  bool Finished() {
    Space();
    return position_ == text_.size() || Error("trailing JSON data");
  }

  const std::string& error() const { return error_; }

 private:
  void Space() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
      ++position_;
  }

  bool Error(const char* message) {
    if (error_.empty())
      error_ = std::string(message) + " at byte " +
               std::to_string(position_);
    return false;
  }

  const std::string& text_;
  std::size_t position_ = 0;
  std::string error_;
};

bool ParsePosition(JsonCursor* cursor, double position[3]) {
  if (!cursor->Consume('[')) return false;
  for (int index = 0; index < 3; ++index) {
    if (!cursor->Number(&position[index])) return false;
    if (index != 2 && !cursor->Consume(',')) return false;
  }
  return cursor->Consume(']');
}

bool ParseEvent(JsonCursor* cursor, EventDefinition* event,
                std::string* failure) {
  if (!cursor->Consume('{')) return false;
  std::set<std::string> keys;
  std::string type;
  if (cursor->TryConsume('}')) {
    *failure = "script event object is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (!keys.insert(key).second) {
      *failure = "duplicate script event key: " + key;
      return false;
    }
    bool accepted = true;
    if (key == "id") accepted = cursor->String(&event->id);
    else if (key == "type") accepted = cursor->String(&type);
    else if (key == "attribute")
      accepted = cursor->String(&event->attribute);
    else if (key == "position")
      accepted = ParsePosition(cursor, event->position);
    else if (key == "delay") accepted = cursor->Number(&event->delay);
    else {
      *failure = "script event contains unknown key: " + key;
      return false;
    }
    if (!accepted) return false;
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  if (keys.size() != 5 || keys.find("id") == keys.end() ||
      keys.find("type") == keys.end() ||
      keys.find("attribute") == keys.end() ||
      keys.find("position") == keys.end() ||
      keys.find("delay") == keys.end()) {
    *failure = "script event requires id, type, attribute, position and delay";
    return false;
  }
  if (!ValidIdentifier(event->id, kMaximumEventId)) {
    *failure = "script event id is not a bounded symbolic identifier";
    return false;
  }
  if (!ValidIdentifier(event->attribute, 63)) {
    *failure = "script event attribute is not a symbolic identifier";
    return false;
  }
  if (type == "explosion") event->kind = EventKind::Explosion;
  else if (type == "spark") event->kind = EventKind::Spark;
  else {
    *failure = "script event type must be explosion or spark";
    return false;
  }
  for (double coordinate : event->position)
    if (!InRange(coordinate, -1000000.0, 1000000.0)) {
      *failure = "script event position is outside the schema-1 range";
      return false;
    }
  if (!InRange(event->delay, 0.0, 3600.0)) {
    *failure = "script event delay is outside the schema-1 range";
    return false;
  }
  return true;
}

bool ParseDocument(const std::string& text, Document* document,
                   std::string* failure) {
  JsonCursor cursor(text);
  if (!cursor.Consume('{')) {
    *failure = cursor.error();
    return false;
  }
  bool schemaSeen = false;
  bool eventsSeen = false;
  if (cursor.TryConsume('}')) {
    *failure = "script-event document is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor.String(&key) || !cursor.Consume(':')) break;
    if (key == "schema") {
      if (schemaSeen || !cursor.Integer(&document->schema)) break;
      schemaSeen = true;
    } else if (key == "events") {
      if (eventsSeen || !cursor.Consume('[')) break;
      eventsSeen = true;
      if (!cursor.TryConsume(']')) {
        for (;;) {
          if (document->events.size() >= kMaximumEvents) {
            *failure = "script-event document exceeds 32 events";
            return false;
          }
          EventDefinition event;
          if (!ParseEvent(&cursor, &event, failure)) break;
          document->events.push_back(std::move(event));
          if (cursor.TryConsume(']')) break;
          if (!cursor.Consume(',')) break;
        }
        if (!cursor.error().empty()) break;
      }
    } else {
      *failure = "script-event document contains unknown key: " + key;
      return false;
    }
    if (cursor.TryConsume('}')) {
      if (!cursor.Finished()) break;
      if (!schemaSeen || !eventsSeen || document->events.empty()) {
        *failure = "script-event document requires schema and non-empty events";
        return false;
      }
      return true;
    }
    if (!cursor.Consume(',')) break;
  }
  if (failure->empty()) *failure = cursor.error();
  if (failure->empty()) *failure = "malformed script-event document";
  return false;
}

bool ValidateUniqueIds(const Document& document, std::string* failure) {
  std::set<std::string> ids;
  for (const EventDefinition& event : document.events) {
    if (!ids.insert(FoldAscii(event.id)).second) {
      *failure = "duplicate script event id: " + event.id;
      return false;
    }
  }
  return true;
}

std::string ObjectName(const EventDefinition& event) {
  return std::string(kObjectPrefix) + event.id;
}

std::uint64_t Fingerprint(const std::string& text) {
  std::uint64_t hash = kFnvOffset;
  for (unsigned char byte : text) {
    hash ^= byte;
    hash *= kFnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

bool ResolveAttribute(SimulationContext* context,
                      const EventDefinition& event,
                      ct_ClassTableID* subjectTable,
                      int* attributeIndex) {
  if (context == nullptr || subjectTable == nullptr ||
      attributeIndex == nullptr ||
      !context->isExist(event.attribute.c_str()))
    return false;
  const char* subjectName = event.kind == EventKind::Explosion
                                ? "Explosion" : "Spark";
  const char* attributeName = event.kind == EventKind::Explosion
                                  ? "ExplosionAttr" : "SparkAttr";
  *subjectTable = g_arena.searchSeanceClassTable(subjectName);
  const ct_ClassTableID attributeTable =
      g_arena.searchSeanceClassTable(attributeName);
  if (*subjectTable == ct_NULLID || attributeTable == ct_NULLID) return false;
  const KR_ObjectID object = context->searchObject(event.attribute.c_str());
  *attributeIndex = g_arena.getAttributeIndex(attributeTable, object);
  if (*attributeIndex < 0) return false;
  if (event.kind == EventKind::Explosion) {
    AttributeExplosion* resolved = nullptr;
    return ExplosionAttributeState_ResolveEncodedIndex(
               context, *attributeIndex, &resolved) && resolved != nullptr &&
           resolved->getObjectID() == object;
  }
  AttributeSpark* resolved = nullptr;
  return SparkAttributeState_ResolveEncodedIndex(
             context, *attributeIndex, &resolved) && resolved != nullptr &&
         resolved->getObjectID() == object;
}

bool RollbackQueued(SimulationContext* context,
                    std::vector<QueuedEvent>* queued) {
  bool clean = context != nullptr && queued != nullptr;
  if (queued == nullptr) return false;
  for (std::vector<QueuedEvent>::reverse_iterator event = queued->rbegin();
       event != queued->rend(); ++event) {
    const bool removed = event->kind == EventKind::Explosion
        ? ExplosionSubjectState_RollbackQueued(context, &event->object, 1)
        : SparkSubjectState_RollbackQueued(context, event->object);
    clean = removed && clean;
  }
  queued->clear();
  return clean;
}

bool SemanticCaptureMatches(SimulationContext* context,
                            const Document& document,
                            unsigned int* proofs,
                            std::string* failure) {
  std::vector<SActiveWorldEvent> events;
  if (!ActiveWorldSemanticEvents_Capture(context, &events, failure))
    return false;
  *proofs = 0;
  for (const EventDefinition& definition : document.events) {
    const std::string destination = ObjectName(definition);
    const int expectedLabel = definition.kind == EventKind::Explosion
                                  ? EXPLOSION_START : sp_EV_CREATE;
    unsigned int matches = 0;
    for (const SActiveWorldEvent& event : events)
      if (event.destination == destination && event.label == expectedLabel)
        ++matches;
    if (matches != 1) {
      *failure = "EVT1 did not capture exactly one event for " + definition.id;
      return false;
    }
    ++*proofs;
  }
  return true;
}

}  // namespace

bool RecoveredScriptEvents_Apply(SimulationContext* context,
                                 double startTime) {
  RecoveredScriptEvents_Release(context);
  if (context == nullptr || !std::isfinite(startTime)) {
    Fail(RECOVERED_SCRIPT_EVENT_RUNTIME_UNAVAILABLE,
         "script events received an invalid runtime boundary");
    return false;
  }
  if (!RecoveredModRuntime_HasOverlayTarget(kEventTarget)) return true;

  std::vector<QueuedEvent> transaction;
  try {
    long length = 0;
    FILE* file = RecoveredModRuntime_OpenOverlayTarget(kEventTarget, &length);
    if (file == nullptr || length < 0) {
      if (file != nullptr) std::fclose(file);
      Fail(RECOVERED_SCRIPT_EVENT_IO_FAILURE,
           "could not open declared RR2NW/script-events.json");
      return false;
    }
    if (length == 0 || length > kMaximumDocumentBytes) {
      std::fclose(file);
      Fail(RECOVERED_SCRIPT_EVENT_TOO_LARGE,
           "script events must contain 1..262144 bytes");
      return false;
    }
    std::string text(static_cast<std::size_t>(length), '\0');
    const std::size_t read =
        std::fread(&text[0], 1, static_cast<std::size_t>(length), file);
    const bool readFailed = read != static_cast<std::size_t>(length) ||
                            std::ferror(file) != 0;
    std::fclose(file);
    if (readFailed) {
      Fail(RECOVERED_SCRIPT_EVENT_IO_FAILURE,
           "could not read complete RR2NW/script-events.json");
      return false;
    }

    Document document;
    std::string failure;
    if (!ParseDocument(text, &document, &failure)) {
      Fail(RECOVERED_SCRIPT_EVENT_MALFORMED,
           "invalid script events: " + failure);
      return false;
    }
    if (document.schema != kSchemaVersion) {
      Fail(RECOVERED_SCRIPT_EVENT_UNSUPPORTED_SCHEMA,
           "unsupported script-event schema: " +
               std::to_string(document.schema));
      return false;
    }
    if (!ValidateUniqueIds(document, &failure)) {
      Fail(RECOVERED_SCRIPT_EVENT_DUPLICATE_ID, failure);
      return false;
    }

    unsigned int explosions = 0;
    unsigned int sparks = 0;
    std::vector<ct_ClassTableID> subjectTables;
    std::vector<int> attributeIndices;
    subjectTables.reserve(document.events.size());
    attributeIndices.reserve(document.events.size());
    for (const EventDefinition& event : document.events) {
      const std::string name = ObjectName(event);
      if (context->isExist(name.c_str())) {
        Fail(RECOVERED_SCRIPT_EVENT_TRANSACTION_FAILURE,
             "script event object identity already exists: " + name);
        return false;
      }
      ct_ClassTableID subjectTable = ct_NULLID;
      int attributeIndex = -1;
      if (!ResolveAttribute(context, event, &subjectTable, &attributeIndex)) {
        Fail(RECOVERED_SCRIPT_EVENT_UNKNOWN_ATTRIBUTE,
             "script event attribute is unavailable for its type: " +
                 event.attribute);
        return false;
      }
      subjectTables.push_back(subjectTable);
      attributeIndices.push_back(attributeIndex);
      if (event.kind == EventKind::Explosion) ++explosions;
      else ++sparks;
    }
    if (context->eventFreeCount() < static_cast<int>(document.events.size()) ||
        ExplosionSubjectState_Capacity() - ExplosionSubjectState_LiveCount() <
            static_cast<int>(explosions) ||
        SparkSubjectState_Capacity() - SparkSubjectState_LiveCount() <
            static_cast<int>(sparks)) {
      Fail(RECOVERED_SCRIPT_EVENT_CAPACITY_FAILURE,
           "script-event preflight found insufficient event/subject capacity");
      return false;
    }

    transaction.reserve(document.events.size());
    const double boundary = (std::max)(startTime, 0.1);
    for (std::size_t index = 0; index < document.events.size(); ++index) {
      const EventDefinition& event = document.events[index];
      const std::string name = ObjectName(event);
      KR_ObjectID child = KR_ObjectID::NUL();
      bool committed = false;
      if (event.kind == EventKind::Explosion) {
        const ExplosionImpactRequest request = {
            CFVector3(event.position[0], event.position[1], event.position[2]),
            boundary + event.delay, KR_ObjectID::NUL(), subjectTables[index],
            attributeIndices[index], name.c_str()};
        committed = ExplosionSubjectState_QueueBatch(
            context, &request, 1, &child);
      } else {
        const SparkCreateRequest request = {
            CFVector3(event.position[0], event.position[1], event.position[2]),
            boundary + event.delay, subjectTables[index],
            attributeIndices[index], name.c_str()};
        committed = SparkSubjectState_QueueCreate(context, request, &child);
      }
      if (!committed) {
        const bool clean = RollbackQueued(context, &transaction);
        Fail(RECOVERED_SCRIPT_EVENT_TRANSACTION_FAILURE,
             std::string("script-event queue rejected ") + event.id +
                 (clean ? " and rolled back" : " and rollback failed"));
        return false;
      }
      transaction.push_back({event.kind, child});
    }

    unsigned int semanticProofs = 0;
    if (!SemanticCaptureMatches(context, document, &semanticProofs,
                                &failure)) {
      const bool clean = RollbackQueued(context, &transaction);
      Fail(RECOVERED_SCRIPT_EVENT_SEMANTIC_PROOF_FAILURE,
           std::string("script-event EVT1 proof failed: ") + failure +
               (clean ? "" : "; rollback failed"));
      return false;
    }

    g_summary.schemaVersion = document.schema;
    g_summary.eventCount = static_cast<unsigned int>(document.events.size());
    g_summary.explosionCount = explosions;
    g_summary.sparkCount = sparks;
    g_summary.queuedCount = static_cast<unsigned int>(transaction.size());
    g_summary.semanticProofCount = semanticProofs;
    g_summary.eventFingerprint = Fingerprint(text);
    g_summary.minimumDelay = document.events.front().delay;
    g_summary.maximumDelay = document.events.front().delay;
    for (const EventDefinition& event : document.events) {
      g_summary.minimumDelay =
          (std::min)(g_summary.minimumDelay, event.delay);
      g_summary.maximumDelay =
          (std::max)(g_summary.maximumDelay, event.delay);
    }
    g_queued = std::move(transaction);
    g_context = context;
    g_active = true;
    return true;
  } catch (const std::bad_alloc&) {
    if (!transaction.empty()) RollbackQueued(context, &transaction);
    Fail(RECOVERED_SCRIPT_EVENT_ALLOCATION_FAILURE,
         "script-event application allocation failed");
  } catch (...) {
    if (!transaction.empty()) RollbackQueued(context, &transaction);
    Fail(RECOVERED_SCRIPT_EVENT_TRANSACTION_FAILURE,
         "script-event application raised an exception");
  }
  return false;
}

void RecoveredScriptEvents_Release(SimulationContext* context) {
  (void)context;
  // The Arena seance owns both committed subjects and queued events. Release
  // only forgets diagnostic handles; failed Apply paths roll back before
  // commit, and normal seance teardown destroys the complete owner graph.
  ClearState();
}

bool RecoveredScriptEvents_IsActive() { return g_active; }

unsigned int RecoveredScriptEvents_Issues() { return g_issues; }

const char* RecoveredScriptEvents_LastError() { return g_lastError; }

const SRecoveredScriptEventSummary* RecoveredScriptEvents_Summary() {
  return g_active ? &g_summary : nullptr;
}

bool RecoveredScriptEvents_ValidateText(const char* text,
                                        std::size_t length,
                                        char* error,
                                        std::size_t errorSize) {
  if (error != nullptr && errorSize != 0) error[0] = '\0';
  std::string failure;
  bool valid = false;
  try {
    if (text == nullptr || length == 0 ||
        length > static_cast<std::size_t>(kMaximumDocumentBytes)) {
      failure = "script-event text has an invalid size";
    } else {
      Document document;
      valid = ParseDocument(std::string(text, length), &document, &failure) &&
              document.schema == kSchemaVersion &&
              ValidateUniqueIds(document, &failure);
      if (!valid && failure.empty()) failure = "unsupported script-event schema";
    }
  } catch (const std::bad_alloc&) {
    failure = "script-event validation allocation failed";
  } catch (...) {
    failure = "script-event validation raised an exception";
  }
  if (!valid && error != nullptr && errorSize != 0)
    std::snprintf(error, errorSize, "%s", failure.c_str());
  return valid;
}
