#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "RecoveredScriptEventRuntime.h"

namespace {

bool Valid(const char* text) {
  char error[256] = {};
  return RecoveredScriptEvents_ValidateText(
      text, std::strlen(text), error, sizeof(error));
}

bool Invalid(const char* text) { return !Valid(text); }

}  // namespace

int main() {
  const char* complete = R"JSON({
    "schema": 1,
    "events": [
      {
        "id": "arrival-flash",
        "type": "spark",
        "attribute": "Spark.Flash",
        "position": [12.5, 3, -7.25],
        "delay": 30
      },
      {
        "id": "arrival-blast",
        "type": "explosion",
        "attribute": "Expl.Small",
        "position": [15, 3, -7],
        "delay": 3.5e1
      }
    ]
  })JSON";
  if (!Valid(complete) ||
      !Invalid(R"JSON({"schema":2,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[]})JSON") ||
      !Invalid(R"JSON({"schema":1,"extra":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"smoke","attribute":"Smoke.Default","position":[0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":3601}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"bad id","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark Flash","position":[0,0,0],"delay":1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1,"label":9001}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1},{"id":"X","type":"explosion","attribute":"Expl.Small","position":[0,0,0],"delay":2}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[NaN,0,0],"delay":1}]})JSON")) {
    std::fprintf(stderr, "strict script-event schema regression\n");
    return EXIT_FAILURE;
  }
  char error[32] = {};
  if (RecoveredScriptEvents_ValidateText(nullptr, 0, error,
                                         sizeof(error)) ||
      error[0] == '\0') {
    std::fprintf(stderr, "invalid input did not produce diagnostics\n");
    return EXIT_FAILURE;
  }
  std::printf("script events schema=1 explosion+spark strict raw-label=closed\n");
  return EXIT_SUCCESS;
}
