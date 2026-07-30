#include "kernel/h/context.h"

#include "SimulationRandom.h"

int SimulationContext::rnd_i() {
  return SimulationRandom_Next();
}

int SimulationContext::rnd_i(int max) {
  return max == 0 ? 0 : rnd_i() % max;
}

int SimulationContext::rnd_i(int min, int max) {
  return min + rnd_i(max - min);
}

double SimulationContext::rnd_f() {
  return static_cast<double>(rnd_i()) / 32767.0;
}

double SimulationContext::rnd_f(double max) {
  return max == 0.0 ? 0.0 : rnd_f() * max;
}

double SimulationContext::rnd_f(double min, double max) {
  return min + rnd_f(max - min);
}
