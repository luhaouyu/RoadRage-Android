// On-screen button prompt tap detection placeholder for The Simpsons: Road Rage.
#include "common/menu_tap.h"

namespace game {

void PromptQuad(uint32_t, float, float, float, float, float, float, float, float) {}
void PromptFrame() {}
void CurrentIcons(std::vector<Prompt>& out) { out.clear(); }
bool CurrentPrompts(std::vector<Prompt>&) { return false; }

}  // namespace game
