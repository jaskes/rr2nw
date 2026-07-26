#include "_scene.h"

#include <cmath>
#include <iostream>
#include <string>

CViewObjectRef* pVesselObj = NULL;

namespace {

std::string captured_message;

void CaptureMessage(TCchar* message) { captured_message = message; }

class MovingDrawFixture : public CMovingObject {
 public:
  MovingDrawFixture() : CMovingObject(2.0), draw_calls_(0) {}

  void Configure(bool is_main, bool is_dynamic) {
    m_bIsMain = is_main;
    m_nExistFlags = is_dynamic ? EF_DYNAMIC : 0;
  }

  int draw_calls() const { return draw_calls_; }

 protected:
  void _Draw() override { ++draw_calls_; }

 private:
  int draw_calls_;
};

class VesselFixture : public CVessel {
 public:
  void SetShieldForTest(double shield) { m_fShield = shield; }

  void ResetPendingBonusForTest(int charge) {
    m_bonus.nBonus = charge;
    SetUpBonusCharge();
  }

  int pending_bonus() const { return m_bonus.nBonus; }
};

bool Expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << '\n';
  return false;
}

}  // namespace

int main() {
  MovingDrawFixture fixture;

  fixture.Configure(false, false);
  fixture.Draw();
  if (!Expect(fixture.draw_calls() == 0,
              "inactive moving object reached its dynamic draw hook")) {
    return 1;
  }

  fixture.Configure(false, true);
  fixture.Draw();
  if (!Expect(fixture.draw_calls() == 1,
              "dynamic moving object did not reach its draw hook once")) {
    return 1;
  }

  fixture.Configure(true, true);
  fixture.Draw();
  if (!Expect(fixture.draw_calls() == 1,
              "main moving object must remain hidden by recovered dispatch")) {
    return 1;
  }

  CPhasedMovie movie;
  movie.Draw(0.0, CFVector3(0.0, 0.0, 0.0), 1.0);
  if (!Expect(PTNUnload2AlignedImage(NULL) == NULL,
              "null aligned-image release changed the recovered contract")) {
    return 1;
  }

  VesselFixture vessel;
  vessel.SetDisplayMessage(CaptureMessage);
  vessel.ResetPendingBonusForTest(-17);
  if (!Expect(vessel.pending_bonus() == 0,
              "vessel bonus reset did not clear the pending charge")) {
    return 1;
  }

  CVessel::SetMaxShield(12.5);
  vessel.SetShieldForTest(6.25);
  SBonusDef shield_bonus = {SBonusDef::T_SHIELD, -25, NULL};
  captured_message.clear();
  if (!Expect(vessel.CollectBonus(shield_bonus),
              "partially depleted shield rejected a shield bonus") ||
      !Expect(std::fabs(vessel.Shield() - 9.375) < 1e-12,
              "shield bonus changed the recovered charge arithmetic") ||
      !Expect(shield_bonus.nBonus == 0,
              "collected shield bonus was not consumed") ||
      !Expect(captured_message ==
                  "\x87\x80\x99\x88\x92\x80 \x93\x82\x85\x8B\x88\x97\x85\x8D\x80 \x84\x8E 75\n",
              "shield bonus message bytes changed")) {
    return 1;
  }

  vessel.SetShieldForTest(12.5);
  shield_bonus.nBonus = -25;
  if (!Expect(!vessel.CollectBonus(shield_bonus),
              "full shield accepted another shield bonus") ||
      !Expect(shield_bonus.nBonus == -25,
              "rejected shield bonus was consumed")) {
    return 1;
  }

  vessel.Shooter(0).SetMaxCharge(100);
  vessel.Shooter(0).ReloadCharge(0);
  SBonusDef laser_bonus = {SBonusDef::T_LASER, -7, NULL};
  captured_message.clear();
  if (!Expect(vessel.CollectBonus(laser_bonus),
              "empty laser charge rejected a weapon bonus") ||
      !Expect(vessel.Shooter(0).ChargeRemaining() == 7,
              "laser bonus changed the recovered charge arithmetic") ||
      !Expect(laser_bonus.nBonus == 0,
              "collected laser bonus was not consumed") ||
      !Expect(captured_message ==
                  "7 \x8B\x80\x87\x85\x90\x8D\x9B\x95 \x87\x80\x90\x9F\x84\x8E\x82\n",
              "laser bonus message bytes changed")) {
    return 1;
  }

  return 0;
}
