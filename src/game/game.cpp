// The Simpsons: Road Rage (XDK 4034) game hooks and console language bridge.
#include <string>

#include "common/options.h"
#include "xdk/hle.h"

#if HAS_OutputDebugStringA
// XDK 4034 OutputDebugStringA uses int 0x2D followed by an int3 skip byte.
HLE_STDCALL(OutputDebugStringA, 1) {
    if (a[0]) {
        std::string s = kernel::ReadCString(a[0]);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) LOGI("dbg: %s", s.c_str());
    }
    return 0;
}
#endif

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
