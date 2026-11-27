// The Simpsons: Road Rage (XDK 4034) game hooks and console language bridge.
#include <string>

#include "common/options.h"

namespace game {

bool LanguageAvailable(const std::string& code) { return KnownLanguage(code); }

// No HUD hook yet: keep touch controls visible
bool HudShown() { return true; }

}  // namespace game

namespace app {

uint32_t ConsoleLanguage() {
    const std::string& l = game::BootLanguage();
    if (l == "deu") return 3;
    if (l == "fra") return 4;
    if (l == "esp") return 5;
    if (l == "ita") return 6;
    return 1;  // English
}

}  // namespace app
