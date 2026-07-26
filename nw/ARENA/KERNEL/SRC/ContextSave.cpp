// Keep one recovered implementation while giving the modern archive a
// separate member for world-save dependencies. The Watcom build still
// compiles Context.cpp directly with both sections enabled.
#define RR2NW_CONTEXT_SAVE_ONLY
#include "Context.cpp"
