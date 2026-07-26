#include <cstdio>
#include <cstdlib>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "i/carrier.i"
#include "message/unitmsg.h"
#include "storage/h/savefile.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-carrier-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

class ProbeCarrier final : public ICarrier {
 public:
  CFMatrix3x4 matrix;

  ProbeCarrier() {
    matrix.LoadIdentity();
    matrix.TranslateR(CFVector3(3.0, 4.0, 5.0));
  }

  void carrierLoadMatrix(CFMatrix3x4& output) override { output = matrix; }
};

class ProbeArtefact final : public IArtefact {
 public:
  int move_count = 0;
  int drop_count = 0;
  double drop_time = 0.0;
  CFMatrix3x4 last_matrix;

  void moveTo(CFMatrix3x4& matrix) override {
    ++move_count;
    last_matrix = matrix;
  }

  int attachTo(KR_ObjectID, ICarrier*) override { return 0; }

  void drop(CFMatrix3x4& matrix, double time) override {
    ++drop_count;
    drop_time = time;
    last_matrix = matrix;
  }

  void artefactMove(const CFVector3&) override {}
  bool dump(PIN_SaveFile&) override { return true; }
  bool load(PIN_SaveFile&) override { return true; }
  void loadNotify() override {}
};

bool SameOffset(const CFMatrix3x4& left, const CFMatrix3x4& right) {
  const CFVector3 left_offset = left.Offset();
  const CFVector3 right_offset = right.Offset();
  return left_offset.x == right_offset.x && left_offset.y == right_offset.y &&
         left_offset.z == right_offset.z;
}

bool ExerciseCarryLifecycle() {
  ProbeCarrier carrier;
  ProbeArtefact artefact;
  const KR_ObjectID artefact_id(42, 7);

  carrier.carrierTakeArtefact(artefact_id, &artefact);
  if (carrier.m_artefact != &artefact || carrier.m_artefactID != artefact_id) {
    return false;
  }

  carrier.carrierOnMove();
  if (artefact.move_count != 1 ||
      !SameOffset(artefact.last_matrix, carrier.matrix)) {
    return false;
  }

  carrier.carrierDropArtefact(8.25);
  if (artefact.drop_count != 1 || artefact.drop_time != 8.25 ||
      !SameOffset(artefact.last_matrix, carrier.matrix) ||
      carrier.m_artefact != nullptr || !carrier.m_artefactID.isNUL()) {
    return false;
  }

  carrier.carrierTakeArtefact(artefact_id, &artefact);
  carrier.carrierOnRemoveArtefact();
  return artefact.drop_count == 1 && carrier.m_artefact == nullptr &&
         carrier.m_artefactID.isNUL();
}

bool ExerciseCarrierSave(const char* path) {
  ProbeCarrier source;
  source.m_artefactID = KR_ObjectID(1999, 11);
  ProbeArtefact source_artefact;
  source_artefact.m_carrierID = KR_ObjectID(2000, 12);

  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(path)) || !source.dump(output) ||
      !source_artefact.IArtefact::dump(output)) {
    output.Close();
    return false;
  }
  output.Close();

  ProbeCarrier restored;
  ProbeArtefact restored_artefact;
  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(path)) || !restored.load(input) ||
      !restored_artefact.IArtefact::load(input)) {
    input.Close();
    return false;
  }
  const bool at_end = input.GetCurrentData() == nullptr;
  input.Close();

  if (!at_end || restored.m_artefactID != source.m_artefactID ||
      restored.m_artefact != nullptr ||
      restored_artefact.m_carrierID != source_artefact.m_carrierID ||
      restored_artefact.m_carrier != nullptr) {
    return false;
  }

  restored.carrierAddNotify(nullptr, 0.0);
  KR_Event collision;
  collision.label = t_EV_ONCOLLISION;
  return restored.m_artefactID.isNUL() && restored.m_artefact == nullptr &&
         restored.carrierReceiveEvent(collision) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "carrier contract requires Win32");
  static_assert(sizeof(KR_ObjectID) == 8,
                "carrier object-ID save layout changed");
  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  std::remove(argv[1]);
  if (!ExerciseCarryLifecycle()) {
    return Fail("carry lifecycle diverged");
  }
  if (!ExerciseCarrierSave(argv[1])) {
    return Fail("carrier save round-trip diverged");
  }

  std::remove(argv[1]);
  std::cout << "legacy-carrier-smoke: OK\n";
  return EXIT_SUCCESS;
}
