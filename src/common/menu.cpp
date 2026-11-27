// settings menu overlay (F10 / on-screen menu button) and performance HUD
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>  // ClearActiveID, the drag scroll's

#include "app/app.h"
#include "audio/output.h"
#include "common/aspect.h"
#include "common/keys.h"
#include "common/menu.h"
#include "common/mouse_keyboard.h"
#include "common/options.h"
#include "common/pad_binds.h"
#include "common/translations.h"

// native/gyro.cpp: aiming by turning the phone
namespace gyro {
uint32_t CurrentMode();
const char* ModeName();
void SetMode(uint32_t mode);
float Sensitivity();
void SetSensitivity(float s);
}  // namespace gyro

// native/touch.cpp: the touch controls' layout
namespace touch {
const char* LayoutName();
void StepLayout(int step);
uint32_t ControlsOpacity();
void StepOpacity(int step);
uint32_t ControlsSize();
void StepSize(int step);
const char* StickModeName();
void StepStickMode(int step);
void EditLayout();
}  // namespace touch

// native/update.cpp & native/stats.cpp
namespace host {
void CheckForUpdates();
void SamplePerf();
float CpuLoad();
float GpuLoad();
float CpuTemp();
float GpuTemp();
float BatteryTemp();
}  // namespace host

namespace app {

Settings DefaultSettings();  // native/main.cpp

namespace {

std::atomic<bool> g_open{false};

// Road Rage theme: midnight asphalt with Springfield taxi yellow and sky-blue accents
constexpr ImVec4 kPanelBg{0.055f, 0.075f, 0.115f, 0.96f};
constexpr ImVec4 kSidebarBg{0.038f, 0.052f, 0.082f, 0.98f};
constexpr ImVec4 kCardBg{0.095f, 0.125f, 0.185f, 0.92f};
constexpr ImVec4 kCardHover{0.145f, 0.195f, 0.285f, 1.0f};
constexpr ImVec4 kAccent{1.0f, 0.83f, 0.16f, 1.0f};        // taxi yellow
constexpr ImVec4 kAccentDark{0.08f, 0.09f, 0.12f, 1.0f};   // text on yellow buttons
constexpr ImVec4 kAccentTint{1.0f, 0.83f, 0.16f, 0.16f};
constexpr ImVec4 kBlue{0.24f, 0.62f, 1.0f, 1.0f};
constexpr ImVec4 kBorder{1.0f, 0.83f, 0.16f, 0.65f};
constexpr ImVec4 kText{0.94f, 0.96f, 0.99f, 1.0f};
constexpr ImVec4 kDim{0.56f, 0.63f, 0.74f, 1.0f};
constexpr ImVec4 kButton{0.13f, 0.17f, 0.25f, 1.0f};
constexpr ImVec4 kButtonHovered{0.20f, 0.27f, 0.39f, 1.0f};
constexpr ImVec4 kLine{0.22f, 0.28f, 0.38f, 0.65f};
constexpr ImVec4 kToggleOn{0.18f, 0.78f, 0.44f, 1.0f};
constexpr ImVec4 kToggleOff{0.20f, 0.24f, 0.33f, 1.0f};

constexpr std::pair<ImGuiCol, ImVec4> kPanelColors[] = {
    {ImGuiCol_WindowBg, kPanelBg},
    {ImGuiCol_Border, kBorder},
    {ImGuiCol_Text, kText},
    {ImGuiCol_Header, kCardHover},
    {ImGuiCol_HeaderHovered, kCardHover},
    {ImGuiCol_HeaderActive, kCardHover},
    {ImGuiCol_Button, kButton},
    {ImGuiCol_ButtonHovered, kButtonHovered},
    {ImGuiCol_ButtonActive, kBlue},
    {ImGuiCol_FrameBg, kCardBg},
    {ImGuiCol_FrameBgHovered, kCardHover},
    {ImGuiCol_FrameBgActive, kCardHover},
    {ImGuiCol_ScrollbarBg, ImVec4(0.04f, 0.06f, 0.09f, 0.6f)},
    {ImGuiCol_ScrollbarGrab, ImVec4(0.26f, 0.33f, 0.46f, 1.0f)},
    {ImGuiCol_ScrollbarGrabHovered, kBlue},
    {ImGuiCol_ScrollbarGrabActive, kAccent},
    {ImGuiCol_Separator, kLine},
    {ImGuiCol_NavHighlight, kAccent},
};

constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"GAME DEFAULT", "2x", "4x", "8x", "16x"};
constexpr uint32_t kDistances[] = {1, 2, 4, 8};
const char* const kDistanceNames[] = {"GAME DEFAULT", "2x", "4x", "8x"};
constexpr uint32_t kFmvScales[] = {0, 1};
const char* const kFmvScaleNames[] = {"FILL", "4:3"};
constexpr uint32_t kScreenFilters[] = {0, 1, 2, 3, 4, 5};
const char* const kScreenFilterNames[] = {"OFF", "SHARPEN", "VIVID", "SCANLINES", "CRT", "HDR"};
constexpr uint32_t kAntiAliasings[] = {game::kAaOff, game::kAaFxaa, game::kAaSsaa};
const char* const kAntiAliasingNames[] = {"OFF", "FXAA", "SSAA 2X"};
enum FpsPosition : uint32_t { kFpsTop, kFpsTopLeft, kFpsTopRight, kFpsBottomLeft, kFpsBottom, kFpsBottomRight };
constexpr uint32_t kFpsPositions[] = {kFpsTop, kFpsTopLeft, kFpsTopRight, kFpsBottomLeft, kFpsBottom, kFpsBottomRight};
const char* const kFpsPositionNames[] = {"TOP CENTER", "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM CENTER",
                                         "BOTTOM RIGHT"};
constexpr uint32_t kAudioOutputs[] = {audio::kOutputAuto, audio::kOutputMono, audio::kOutputStereo,
                                      audio::kOutputSurround};
const char* const kAudioOutputNames[] = {"AUTOMATIC", "MONO", "2.0 STEREO", "5.1 SURROUND"};
const char* const kLanguageNames[] = {"ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "INDONESIAN"};
constexpr const char* kRepoUrl = "https://github.com/luhaouyu/RoadRage-Android";
constexpr uint32_t kFpsCaps[] = {30, 60, 75, 90, 120, 144, 165, 240, 360, kFpsCapRefresh, 0};

int StepIndex(int index, int step, int count) {
    if (count <= 0) return 0;
    index = (index + step) % count;
    return index < 0 ? index + count : index;
}

int NearestIndex(const uint32_t* values, int count, uint32_t value) {
    int best = 0;
    uint32_t best_d = UINT32_MAX;
    for (int i = 0; i < count; i++) {
        uint32_t d = values[i] > value ? values[i] - value : value - values[i];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

template <size_t N>
int StepTable(const uint32_t (&values)[N], uint32_t& value, int step) {
    int i = NearestIndex(values, int(N), value);
    if (step) {
        i = StepIndex(i, step, int(N));
        value = values[i];
    }
    return i;
}

int StepRange(int value, int lo, int hi, int inc, int step) {
    int count = (hi - lo) / inc + 1;
    return lo + inc * StepIndex((std::clamp(value, lo, hi) - lo) / inc, step, count);
}

void Section(const char* title) {
    float em = ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(0, em * 0.45f));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // small taxi-yellow accent pill beside section title
    dl->AddRectFilled(ImVec2(p.x, p.y + em * 0.12f), ImVec2(p.x + em * 0.24f, p.y + em * 0.96f),
                      ImGui::GetColorU32(kAccent), em * 0.12f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + em * 0.55f);
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted(Ui(title).c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, em * 0.18f));
}

constexpr float kValueEms = 11.5f;

struct RowBox {
    float em;
    float row_w;
    float row_h;
    float pad_x;
    float value_w;
    float block_w;
    float row_x;
    float row_y;
    ImVec2 screen_min;
    ImVec2 screen_max;
};

RowBox MeasureRow() {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box;
    box.em = ImGui::GetFontSize();
    box.row_w = ImGui::GetContentRegionAvail().x;
    box.row_h = ImGui::GetFrameHeight() + box.em * 0.42f;
    box.pad_x = box.em * 0.75f;
    box.value_w = box.em * kValueEms;
    float btn_w = ImGui::GetFrameHeight() * 1.1f;
    box.block_w = btn_w * 2 + box.value_w + style.ItemSpacing.x * 2;
    box.row_x = ImGui::GetCursorPosX();
    box.row_y = ImGui::GetCursorPosY();
    box.screen_min = ImGui::GetCursorScreenPos();
    box.screen_max = ImVec2(box.screen_min.x + box.row_w, box.screen_min.y + box.row_h);
    return box;
}

void DrawRowCard(const RowBox& box, bool lit) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float round = box.em * 0.42f;
    dl->AddRectFilled(box.screen_min, box.screen_max, ImGui::GetColorU32(lit ? kCardHover : kCardBg), round);
    if (lit) {
        dl->AddRect(box.screen_min, box.screen_max, ImGui::GetColorU32(kAccent), round, 0, 1.5f);
    }
}

void RowLabel(const RowBox& box, const char* label) {
    float text_y = box.row_y + (box.row_h - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(box.row_x + box.pad_x, text_y));
    ImGui::TextUnformatted(Ui(label).c_str());
}

void EndRow(const RowBox& box) {
    ImGui::SetCursorPos(ImVec2(box.row_x, box.row_y));
    ImGui::Dummy(ImVec2(box.row_w, box.row_h + box.em * 0.14f));
}

// interactive pill toggle switch for boolean settings
bool ToggleRow(const char* label, bool on) {
    RowBox box = MeasureRow();
    ImGui::PushID(label);
    bool toggled = ImGui::Selectable("##toggle_row", false, ImGuiSelectableFlags_AllowOverlap,
                                     ImVec2(box.row_w, box.row_h));
    bool focused = ImGui::IsItemFocused();
    bool hovered = ImGui::IsItemHovered();
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
            ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) {
            toggled = true;
        }
    }
    DrawRowCard(box, focused || hovered);
    RowLabel(box, label);

    // draw switch pill on the right
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float sw_h = box.em * 1.28f;
    float sw_w = sw_h * 2.15f;
    float sw_x1 = box.screen_max.x - box.pad_x;
    float sw_x0 = sw_x1 - sw_w;
    float sw_cy = (box.screen_min.y + box.screen_max.y) * 0.5f;
    float sw_y0 = sw_cy - sw_h * 0.5f;
    float sw_y1 = sw_cy + sw_h * 0.5f;
    dl->AddRectFilled(ImVec2(sw_x0, sw_y0), ImVec2(sw_x1, sw_y1),
                      ImGui::GetColorU32(on ? kToggleOn : kToggleOff), sw_h * 0.5f);
    dl->AddRect(ImVec2(sw_x0, sw_y0), ImVec2(sw_x1, sw_y1),
                ImGui::GetColorU32(on ? ImVec4(0.45f, 0.95f, 0.65f, 0.9f) : kLine), sw_h * 0.5f, 0, 1.2f);
    float knob_r = sw_h * 0.38f;
    float knob_x = on ? (sw_x1 - sw_h * 0.5f) : (sw_x0 + sw_h * 0.5f);
    dl->AddCircleFilled(ImVec2(knob_x, sw_cy), knob_r,
                        on ? IM_COL32(255, 255, 255, 255) : IM_COL32(195, 205, 222, 235), 24);

    // status label beside the switch
    std::string state = Ui(on ? "ON" : "OFF");
    ImVec2 ts = ImGui::CalcTextSize(state.c_str());
    dl->AddText(ImVec2(sw_x0 - ts.x - box.em * 0.65f, sw_cy - ts.y * 0.5f),
                ImGui::GetColorU32(on ? kAccent : kDim), state.c_str());

    ImGui::PopID();
    EndRow(box);
    return toggled;
}

// card row with rounded [ - ] VALUE [ + ] stepper on the right
int OptionRow(const char* label, const char* value) {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box = MeasureRow();
    ImGui::PushID(label);
    int step = 0;
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(box.row_w, box.row_h));
    bool focused = ImGui::IsItemFocused();
    bool hovered = ImGui::IsItemHovered();
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) step = 1;
    }
    DrawRowCard(box, focused || hovered);
    RowLabel(box, label);

    float btn_h = ImGui::GetFrameHeight();
    float btn_w = btn_h * 1.1f;
    float btn_y = box.row_y + (box.row_h - btn_h) * 0.5f;
    float block_x = box.row_x + box.row_w - box.pad_x - box.block_w;

    ImGui::PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_ButtonRepeat, true);
    ImGui::SetCursorPos(ImVec2(block_x, btn_y));
    if (ImGui::Button("-##dec", ImVec2(btn_w, btn_h))) step = -1;

    float value_x = block_x + btn_w + style.ItemSpacing.x;
    ImVec2 ts = ImGui::CalcTextSize(value);
    float pad = (box.value_w - ts.x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(value_x + std::max(pad, 0.0f), box.row_y + (box.row_h - ts.y) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(value_x + box.value_w + style.ItemSpacing.x, btn_y));
    if (ImGui::Button("+##inc", ImVec2(btn_w, btn_h))) step = 1;
    ImGui::PopItemFlag();

    ImGui::PopID();
    EndRow(box);
    return step;
}

const char* OnOff(bool on) { return on ? "ON" : "OFF"; }

void Flip(bool& on, int step) {
    if (step) on = !on;
}

void Changed() {
    audio::SetOutputMode(game::Opt().audio_output);
    Save();
}

enum RowId {
    kRenderResolution, kMaxFps, kVsync, kFpsCounter, kAniso, kReloadBlur, kSkipIntros, kSensitivity,
    kInvertMouse, kWalkSpeed, kFov, kViewmodelFov, kHoldAim, kDrawDistance, kAudioOutput, kLanguage,
    kFmvScale, kGyro, kGyroSensitivity, kTouchLayout, kTouchOpacity, kTouchSize, kTouchEdit,
    kInfiniteAmmo, kRapidFire, kNoRecoil, kGameSpeed, kGodMode, kOneHitKill, kInfiniteGrenades,
    kCheckUpdates, kTouchSensitivity, kUnlockCharacters, kPs2Buttons,
    kUnlockExtras, kInfiniteFatality, kMaxExperience, kMaxHealth, kMaxMana, kAspectRatio, kSkipCutscenes,
    kScreenFilter, kAntiAliasing, kFrameSkip, kFpsPosition, kFpsTemps, kUltrawide, kStickMode, kBloom, kFog,
    kInfiniteEnergy, kGithub, kFpsUsage
};

using rows::Kind;
using rows::Row;

constexpr Row kGraphicsRows[] = {
    {Kind::Heading, -1, "DISPLAY"},
    {Kind::Value, kRenderResolution, "RENDER RESOLUTION"},
    {Kind::Value, kAspectRatio, "ASPECT RATIO"},
    {Kind::Value, kUltrawide, "ULTRAWIDE"},
    {Kind::Heading, -1, "PERFORMANCE"},
    {Kind::Value, kVsync, "VERTICAL SYNC"},
    {Kind::Value, kMaxFps, "MAX FRAME RATE"},
    {Kind::Value, kFrameSkip, "FRAME SKIP"},
    {Kind::Value, kFpsCounter, "FPS COUNTER"},
    {Kind::Value, kFpsPosition, "FPS COUNTER POSITION"},
    {Kind::Value, kFpsUsage, "SHOW CPU / GPU USAGE"},
    {Kind::Value, kFpsTemps, "SHOW TEMPERATURES"},
    {Kind::Heading, -1, "QUALITY"},
    {Kind::Value, kAniso, "ANISOTROPIC FILTERING"},
    {Kind::Value, kAntiAliasing, "ANTI-ALIASING"},
    {Kind::Value, kBloom, "BLOOM"},
    {Kind::Value, kFog, "FOG"},
    {Kind::Value, kScreenFilter, "SCREEN FILTER"},
};

constexpr Row kTouchRows[] = {
    {Kind::Heading, -1, "LAYOUT"},
    {Kind::Value, kTouchLayout, "CONTROLS LAYOUT"},
    {Kind::Link, kTouchEdit, "EDIT CONTROLS POSITION"},
    {Kind::Heading, -1, "CUSTOMIZATION"},
    {Kind::Value, kStickMode, "STICK MODE"},
    {Kind::Value, kTouchOpacity, "CONTROLS OPACITY"},
    {Kind::Value, kTouchSize, "CONTROLS SIZE"},
    {Kind::Value, kPs2Buttons, "PS2 BUTTON ICONS"},
};

constexpr Row kGeneralRows[] = {
    {Kind::Heading, -1, "AUDIO & LANGUAGE"},
    {Kind::Value, kAudioOutput, "AUDIO OUTPUT"},
    {Kind::Value, kLanguage, "LANGUAGE"},
};

const char* const kCreditNames[] = {
    "luhaouyu",
    "Rangga Casper",
    "TGP482",
    "SDL3, glslang, toml++, Dear ImGui",
    "xemu, Cxbx-Reloaded",
};

constexpr Row kCreditRows[] = {
    {Kind::Heading, -1, "ABOUT"},
    {Kind::Info, 0, "ROAD RAGE PORT"},
    {Kind::Info, 1, "HOST ARCHITECTURE"},
    {Kind::Info, 2, "RECOMPILER AND RUNTIME"},
    {Kind::Info, 3, "LIBRARIES"},
    {Kind::Info, 4, "XBOX REFERENCES"},
    {Kind::Heading, -1, "PROJECT"},
    {Kind::Link, kGithub, "GITHUB REPOSITORY"},
};

constexpr Row kCheatRows[] = {
    {Kind::Heading, -1, "GAME"},
    {Kind::Value, kGameSpeed, "GAME SPEED"},
};

const std::vector<Row>& InputRows() {
    static const std::vector<Row> list = [] {
        std::vector<Row> out = {{Kind::Heading, -1, "KEYBOARD"}};
        size_t count = 0;
        const input::Action* actions = input::Actions(&count);
        for (size_t i = 0; i < count; i++) out.push_back({Kind::Bind, int(i), actions[i].label});
        return out;
    }();
    return list;
}

const std::vector<Row>& PadRows() {
    static const std::vector<Row> list = [] {
        std::vector<Row> out = {{Kind::Heading, -1, "GAMEPAD"}};
        size_t count = 0;
        const input::Action* actions = input::PadActions(&count);
        for (size_t i = 0; i < count; i++) out.push_back({Kind::PadBind, int(i), actions[i].label});
        return out;
    }();
    return list;
}

bool* ToggleFlag(Settings& s, int id) {
    game::Options& o = game::Opt();
    switch (id) {
        case kVsync: return &s.vsync;
        case kFpsCounter: return &o.fps_counter;
        case kFrameSkip: return &o.frame_skip;
        case kUltrawide: return &o.ultrawide;
        case kFpsTemps: return &o.fps_temps;
        case kFpsUsage: return &o.fps_usage;
        case kReloadBlur: return &o.reload_blur;
        case kBloom: return &o.bloom;
        case kFog: return &o.fog;
        case kSkipIntros: return &o.skip_intros;
        case kInvertMouse: return &o.invert_mouse;
        case kHoldAim: return &o.hold_aim;
        case kInfiniteAmmo: return &o.infinite_ammo;
        case kNoRecoil: return &o.no_recoil;
        case kGodMode: return &o.god_mode;
        case kOneHitKill: return &o.one_hit_kill;
        case kInfiniteGrenades: return &o.infinite_grenades;
        case kUnlockCharacters: return &o.unlock_characters;
        case kPs2Buttons: return &o.ps2_buttons;
        case kUnlockExtras: return &o.unlock_extras;
        case kInfiniteFatality: return &o.infinite_fatality;
        case kInfiniteEnergy: return &o.infinite_energy;
        case kMaxExperience: return &o.max_experience;
        case kMaxHealth: return &o.max_health;
        case kMaxMana: return &o.max_mana;
        case kSkipCutscenes: return &o.skip_cutscenes;
        default: return nullptr;
    }
}

constexpr int kFovMin = 45;
constexpr int kFovMax = 140;
constexpr uint32_t kGameFov = 70;

void FovText(uint32_t fov, char (&text)[64]) {
    std::snprintf(text, sizeof(text), fov == kGameFov ? "%u (DEFAULT)" : "%u", fov);
}

bool StepDisplayRow(int id, int step, Settings& s, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kRenderResolution: {
            if (step) s.render_percent = uint32_t(StepRange(int(s.render_percent), 20, 200, 5, step));
            Mode out = Output();
            std::snprintf(text, sizeof(text), "%u%% (%ux%u)", s.render_percent, out.width * s.render_percent / 100,
                          out.height * s.render_percent / 100);
            return true;
        }
        case kAspectRatio: {
            constexpr int count = int(std::size(game::kAspects));
            int i = count - 1;
            for (int j = 0; j < count; j++)
                if (game::kAspects[j].code == o.aspect_ratio) i = j;
            if (step) {
                i = StepIndex(i, step, count);
                o.aspect_ratio = game::kAspects[i].code;
            }
            std::snprintf(text, sizeof(text), "%s", game::kAspects[i].name);
            return true;
        }
        case kFmvScale:
            std::snprintf(text, sizeof(text), "%s", kFmvScaleNames[StepTable(kFmvScales, o.fmv_scale, step)]);
            return true;
        case kFov:
            if (step) o.fov = uint32_t(StepRange(int(o.fov), kFovMin, kFovMax, 1, step));
            FovText(o.fov, text);
            return true;
        case kViewmodelFov:
            if (step) o.viewmodel_fov = uint32_t(StepRange(int(o.viewmodel_fov), kFovMin, kFovMax, 1, step));
            FovText(o.viewmodel_fov, text);
            return true;
        case kMaxFps: {
            uint32_t cap = kFpsCaps[StepTable(kFpsCaps, s.fps_cap, step)];
            if (cap == kFpsCapRefresh) std::snprintf(text, sizeof(text), "REFRESH RATE");
            else if (cap) std::snprintf(text, sizeof(text), cap == 60 ? "%u (DEFAULT)" : "%u", cap);
            else std::snprintf(text, sizeof(text), "UNLIMITED");
            return true;
        }
        default:
            return false;
    }
}

bool StepGraphicsRow(int id, int step, Settings& s, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kAniso:
            std::snprintf(text, sizeof(text), "%s", kAnisoNames[StepTable(kAnisoValues, s.aniso, step)]);
            return true;
        case kAntiAliasing:
            std::snprintf(text, sizeof(text), "%s", kAntiAliasingNames[StepTable(kAntiAliasings, o.anti_aliasing, step)]);
            return true;
        case kScreenFilter:
            std::snprintf(text, sizeof(text), "%s", kScreenFilterNames[StepTable(kScreenFilters, o.screen_filter, step)]);
            return true;
        case kFpsPosition:
            std::snprintf(text, sizeof(text), "%s", kFpsPositionNames[StepTable(kFpsPositions, o.fps_position, step)]);
            return true;
        case kDrawDistance:
            std::snprintf(text, sizeof(text), "%s", kDistanceNames[StepTable(kDistances, o.draw_distance, step)]);
            return true;
        default:
            return false;
    }
}

bool StepCheatRow(int id, int step, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kRapidFire:
            if (step) o.rapid_fire = uint32_t(StepRange(int(o.rapid_fire), 1, 4, 1, step));
            if (o.rapid_fire <= 1) std::snprintf(text, sizeof(text), "OFF");
            else std::snprintf(text, sizeof(text), "%uX", o.rapid_fire);
            return true;
        case kGameSpeed:
            if (step) o.game_speed = uint32_t(StepRange(int(o.game_speed), 25, 200, 25, step));
            std::snprintf(text, sizeof(text), o.game_speed == 100 ? "%u%% (DEFAULT)" : "%u%%", o.game_speed);
            return true;
        default:
            return false;
    }
}

void StepInputRow(int id, int step, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kAudioOutput:
            std::snprintf(text, sizeof(text), "%s", kAudioOutputNames[StepTable(kAudioOutputs, o.audio_output, step)]);
            break;
        case kSensitivity:
            if (step) {
                int tenths = StepRange(int(std::lround(o.mouse_sensitivity * 10.0f)), 1, 50, 1, step);
                o.mouse_sensitivity = float(tenths) / 10.0f;
            }
            std::snprintf(text, sizeof(text), "%.1f", o.mouse_sensitivity);
            break;
        case kTouchSensitivity:
            if (step) {
                int tenths = StepRange(int(std::lround(o.touch_sensitivity * 10.0f)), 1, 50, 1, step);
                o.touch_sensitivity = float(tenths) / 10.0f;
            }
            std::snprintf(text, sizeof(text), "%.1f", o.touch_sensitivity);
            break;
        case kWalkSpeed:
            if (step) o.walk_speed = uint32_t(StepRange(int(o.walk_speed), 10, 100, 5, step));
            std::snprintf(text, sizeof(text), "%u%%", o.walk_speed);
            break;
        case kGyro:
            if (step) gyro::SetMode(uint32_t(StepIndex(int(gyro::CurrentMode()), step, 3)));
            std::snprintf(text, sizeof(text), "%s", gyro::CurrentMode() == 0 ? "OFF"
                                                    : gyro::CurrentMode() == 1 ? "ALWAYS"
                                                                               : "WHILE FIRING");
            break;
        case kTouchLayout:
            if (step) touch::StepLayout(step);
            std::snprintf(text, sizeof(text), "%s", touch::LayoutName());
            break;
        case kStickMode:
            if (step) touch::StepStickMode(step);
            std::snprintf(text, sizeof(text), "%s", touch::StickModeName());
            break;
        case kTouchOpacity:
            if (step) touch::StepOpacity(step);
            std::snprintf(text, sizeof(text), "%u%%", touch::ControlsOpacity());
            break;
        case kTouchSize:
            if (step) touch::StepSize(step);
            std::snprintf(text, sizeof(text), "%u%%", touch::ControlsSize());
            break;
        case kCheckUpdates:
            if (step) host::CheckForUpdates();
            std::snprintf(text, sizeof(text), "CHECK");
            break;
        case kGyroSensitivity:
            if (step) gyro::SetSensitivity(float(StepRange(int(std::lround(gyro::Sensitivity() * 10.0f)), 1, 50, 1, step)) / 10.0f);
            std::snprintf(text, sizeof(text), "%.1f", gyro::Sensitivity());
            break;
        case kLanguage: {
            int count = int(std::size(game::kLanguages));
            int i = 0;
            while (i < count - 1 && o.language != game::kLanguages[i]) i++;
            if (step) {
                do i = StepIndex(i, step, count);
                while (!game::LanguageAvailable(game::kLanguages[i]));
                o.language = game::kLanguages[i];
            }
            const char* format = o.language == game::BootLanguage() ? "%s" : "%s (RESTART)";
            std::snprintf(text, sizeof(text), format, kLanguageNames[i]);
            break;
        }
    }
}

std::string RowText(int id, int step, Settings& s = Get()) {
    char text[64] = "";
    if (bool* on = ToggleFlag(s, id)) {
        Flip(*on, step);
        std::snprintf(text, sizeof(text), "%s", OnOff(*on));
    } else if (!StepDisplayRow(id, step, s, text) && !StepGraphicsRow(id, step, s, text) &&
               !StepCheatRow(id, step, text)) {
        StepInputRow(id, step, text);
    }
    if (step && &s == &Get()) Changed();
    return text;
}

constexpr const char* kUnbound = "UNBOUND";

std::string KeyText(std::string keys) {
    if (keys.empty()) return kUnbound;
    for (char& c : keys) c = c == ',' ? '/' : char(std::toupper(uint8_t(c)));
    return keys;
}

// sidebar pages
const char* const kPages[] = {"GRAPHICS", "TOUCH PAD", "AUDIO & LANG", "KEYBOARD", "GAMEPAD", "ABOUT"};
constexpr int kSettingsPage = 0;
constexpr int kTouchPage = 1;
constexpr int kGeneralPage = 2;
constexpr int kKeyboardPage = 3;
constexpr int kControllerPage = 4;
constexpr int kCreditsPage = 5;
constexpr int kCheatsPage = -1;

void RestoreDefaults(int page) {
    if (page == kCreditsPage) return;
    game::Options& o = game::Opt();
    if (page == kCheatsPage) {
        game::Options none;
        o.game_speed = none.game_speed;
        Changed();
        return;
    }
    if (page == kKeyboardPage) {
        o.mouse_sensitivity = game::Options{}.mouse_sensitivity;
        o.invert_mouse = false;
        o.hold_aim = true;
        o.walk_speed = game::Options{}.walk_speed;
        input::ResetBindings();
        return;
    }
    if (page == kControllerPage) {
        input::ResetPadBindings();
        return;
    }
    game::Options keep = o;
    Get() = DefaultSettings();
    o = game::Options{};
    o.mouse_sensitivity = keep.mouse_sensitivity;
    o.invert_mouse = keep.invert_mouse;
    o.hold_aim = keep.hold_aim;
    o.walk_speed = keep.walk_speed;
    o.keys = keep.keys;
    o.pad = keep.pad;
    o.game_speed = keep.game_speed;
    Changed();
}

std::atomic<int> g_capture{-1};
int g_capture_slot = 0;
int g_hovered_box = -1;
bool g_capture_armed = false;
int g_capture_done = 0;
std::atomic<int> g_pad_capture{-1};
bool g_pad_capture_armed = false;
std::chrono::steady_clock::time_point g_pad_capture_since;
constexpr std::chrono::seconds kPadCaptureTimeout{5};
int g_page = kSettingsPage;

std::vector<std::string> KeyBoxes(size_t action) {
    std::string keys = input::Binding(action);
    size_t comma = keys.find(',');
    std::string second;
    if (comma != std::string::npos) second = keys.substr(comma + 1);
    second = second.substr(0, second.find(','));
    if (!second.empty()) second = KeyText(second);
    return {KeyText(keys.substr(0, comma)), second};
}

constexpr float kKeyBoxEms = 9.0f;

int BindRow(const char* label, const std::vector<std::string>& keys, int capturing, int& cleared,
            const char* prompt = "PRESS A KEY") {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox row = MeasureRow();
    DrawRowCard(row, false);
    float value_end = row.row_x + row.row_w - row.pad_x;
    int n = int(keys.size());
    int pressed = -1;
    float box_w = n > 1 ? row.em * kKeyBoxEms : row.value_w;
    float btn_h = ImGui::GetFrameHeight();
    float btn_y = row.row_y + (row.row_h - btn_h) * 0.5f;
    ImGui::PushID(label);
    RowLabel(row, label);
    for (int i = 0; i < n; i++) {
        float x = value_end - float(n - i) * box_w - float(n - 1 - i) * style.ItemSpacing.x;
        ImGui::SetCursorPos(ImVec2(x, btn_y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##key", ImVec2(box_w, btn_h))) pressed = i;
        bool deleted = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || deleted) cleared = i;
        bool pointed = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        if (pointed && !keys[i].empty() && keys[i] != kUnbound) g_hovered_box = i;
        bool lit = capturing == i || pointed;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(capturing == i ? kAccentTint : lit ? kButtonHovered : kButton),
                          row.em * 0.35f);
        dl->AddRect(r0, r1, ImGui::GetColorU32(capturing == i ? kAccent : lit ? kBlue : kLine),
                    row.em * 0.35f, 0, capturing == i ? 1.8f : 1.0f);
        std::string value = keys[i];
        if (capturing == i) value = Ui(prompt);
        else if (value == kUnbound) value = Ui(kUnbound);
        ImVec2 ts = ImGui::CalcTextSize(value.c_str());
        float pad = (box_w - ts.x) * 0.5f;
        ImGui::SetCursorPos(ImVec2(x + std::max(pad, 0.0f), row.row_y + (row.row_h - ts.y) * 0.5f));
        if (capturing == i) ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        else if (keys[i].empty() || keys[i] == kUnbound) ImGui::PushStyleColor(ImGuiCol_Text, kDim);
        ImGui::TextUnformatted(value.c_str());
        if (capturing == i || keys[i].empty() || keys[i] == kUnbound) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::PopID();
    EndRow(row);
    return pressed;
}

void PollCapture() {
    if (g_capture < 0) return;
    int vk = input::PressedKey();
    if (!g_capture_armed) {
        g_capture_armed = !vk;
        return;
    }
    if (!vk) return;
    if (vk != VK_ESCAPE) input::Bind(size_t(g_capture.load()), vk, g_capture_slot);
    g_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
}

void PollPadCapture() {
    if (g_pad_capture < 0) return;
    std::string input = input::PressedPadInput();
    bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                  std::chrono::steady_clock::now() - g_pad_capture_since > kPadCaptureTimeout;
    if (!cancel && !g_pad_capture_armed) {
        g_pad_capture_armed = input.empty();
        return;
    }
    if (!cancel && input.empty()) return;
    if (!cancel) input::PadBind(size_t(g_pad_capture.load()), input);
    g_pad_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
}

bool CaptureFree() { return g_capture < 0 && g_pad_capture < 0 && ImGui::GetFrameCount() > g_capture_done + 1; }

void ValueRow(const Row& r) {
    if (bool* flag = ToggleFlag(Get(), r.id)) {
        if (ToggleRow(r.label, *flag)) {
            *flag = !*flag;
            Changed();
        }
        return;
    }
    int step = OptionRow(r.label, Ui(RowText(r.id, 0)).c_str());
    if (step) RowText(r.id, step);
}

const char* LinkUrl(int id) {
    switch (id) {
        case kGithub: return kRepoUrl;
        default: return nullptr;
    }
}

void LinkRow(const Row& r) {
    RowBox box = MeasureRow();
    ImGui::PushID(r.label);
    bool open = ImGui::Selectable("##link", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(box.row_w, box.row_h));
    bool lit = ImGui::IsItemFocused() || ImGui::IsItemHovered();
    DrawRowCard(box, lit);
    RowLabel(box, r.label);

    std::string badge = Ui(r.id == kTouchEdit ? "EDIT" : "OPEN");
    ImVec2 ts = ImGui::CalcTextSize(badge.c_str());
    float pill_h = ImGui::GetFrameHeight();
    float pill_w = std::max(box.em * 6.5f, ts.x + box.em * 1.6f);
    float px1 = box.screen_max.x - box.pad_x;
    float px0 = px1 - pill_w;
    float py0 = (box.screen_min.y + box.screen_max.y - pill_h) * 0.5f;
    float py1 = py0 + pill_h;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(px0, py0), ImVec2(px1, py1),
                      ImGui::GetColorU32(lit ? kAccent : kAccentTint), pill_h * 0.45f);
    dl->AddRect(ImVec2(px0, py0), ImVec2(px1, py1), ImGui::GetColorU32(kAccent), pill_h * 0.45f, 0, 1.3f);
    dl->AddText(ImVec2((px0 + px1 - ts.x) * 0.5f, (py0 + py1 - ts.y) * 0.5f),
                ImGui::GetColorU32(lit ? kAccentDark : kAccent), badge.c_str());

    ImGui::PopID();
    EndRow(box);
    if (!open) return;
    if (r.id == kTouchEdit) touch::EditLayout();
    else if (const char* url = LinkUrl(r.id)) SDL_OpenURL(url);
}

void InfoRow(const Row& r) {
    RowBox box = MeasureRow();
    DrawRowCard(box, false);
    RowLabel(box, r.label);
    if (r.id >= 0 && r.id < int(std::size(kCreditNames))) {
        const char* name = kCreditNames[r.id];
        ImVec2 ts = ImGui::CalcTextSize(name);
        float x = box.row_x + box.row_w - box.pad_x - ts.x;
        float y = box.row_y + (box.row_h - ts.y) * 0.5f;
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
    }
    EndRow(box);
}

void KeyRow(const Row& r) {
    int cleared = -1;
    int box = BindRow(r.label, KeyBoxes(size_t(r.id)), g_capture == r.id ? g_capture_slot : -1, cleared);
    if (cleared >= 0 && CaptureFree()) input::Bind(size_t(r.id), 0, cleared);
    if (box < 0 || !CaptureFree()) return;
    g_capture_slot = box;
    g_capture = r.id;
    g_capture_armed = false;
}

void PadRow(const Row& r) {
    size_t count = 0;
    const input::Action& action = input::PadActions(&count)[r.id];
    std::string bound = input::PadBinding(size_t(r.id));
    int cleared = -1;
    int box = BindRow(r.label, {bound.empty() ? action.defaults : bound}, g_pad_capture == r.id ? 0 : -1, cleared,
                      "PRESS A BUTTON");
    if (cleared >= 0 && CaptureFree()) input::PadBind(size_t(r.id), "");
    if (box < 0 || !CaptureFree()) return;
    g_pad_capture = r.id;
    g_pad_capture_armed = false;
    g_pad_capture_since = std::chrono::steady_clock::now();
}

void DrawRows(int page) {
    size_t count = 0;
    const Row* list = rows::Page(page, &count);
    for (size_t i = 0; i < count; i++) {
        const Row& r = list[i];
        if (r.kind == Kind::Heading) Section(r.label);
        else if (r.kind == Kind::Value) ValueRow(r);
        else if (r.kind == Kind::Link) LinkRow(r);
        else if (r.kind == Kind::Info) InfoRow(r);
        else if (r.kind == Kind::PadBind) PadRow(r);
        else KeyRow(r);
    }
}

void Defaults() {
    g_capture = -1;
    g_pad_capture = -1;
    RestoreDefaults(g_page);
}

void DragScroll() {
    static bool dragging = false;
    const ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging = false;
        return;
    }
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!dragging) {
        bool touch = io.MouseSource == ImGuiMouseSource_TouchScreen;
        bool inside = window->Rect().Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);
        bool scrollbar = ImGui::GetActiveID() == ImGui::GetWindowScrollbarID(window, ImGuiAxis_Y);
        float moved = std::fabs(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).y);
        dragging = touch && inside && !scrollbar && moved > ImGui::GetFontSize() * 0.6f;
        if (!dragging) return;
    }
    ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
    ImGui::ClearActiveID();
}

// taxi-cab checkered racing strip along the top of the settings window
void DrawCheckeredTop(ImDrawList* dl, ImVec2 win_pos, float win_w, float strip_h, float rounding) {
    float cell = strip_h * 1.35f;
    int cols = std::max(1, int(std::ceil(win_w / cell)));
    dl->PushClipRect(win_pos, ImVec2(win_pos.x + win_w, win_pos.y + strip_h), true);
    dl->AddRectFilled(win_pos, ImVec2(win_pos.x + win_w, win_pos.y + strip_h), IM_COL32(18, 22, 32, 255), rounding,
                      ImDrawFlags_RoundCornersTop);
    for (int i = 0; i < cols; i++) {
        if (i % 2 == 0) {
            float x0 = win_pos.x + float(i) * cell;
            float x1 = std::min(x0 + cell, win_pos.x + win_w);
            dl->AddRectFilled(ImVec2(x0, win_pos.y), ImVec2(x1, win_pos.y + strip_h),
                              ImGui::GetColorU32(kAccent));
        }
    }
    dl->PopClipRect();
}

void DrawSidebar(float em, float sidebar_w) {
    int step = ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) - ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
    int pages = int(std::size(kPages));
    if (step && CaptureFree()) {
        g_page = StepIndex(g_page, step, pages);
        g_capture = -1;
        g_pad_capture = -1;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kSidebarBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 0.85f, em * 0.9f));
    ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, 0),
                      ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding);

    // brand badge
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted("ROAD RAGE");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextUnformatted(Ui("SETTINGS").c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, em * 0.45f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, em * 0.45f));

    // vertical category pills
    float avail_w = ImGui::GetContentRegionAvail().x;
    float nav_h = ImGui::GetFrameHeight() + em * 0.55f;
    for (int i = 0; i < pages; i++) {
        bool selected = (i == g_page);
        ImGui::PushID(i);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1(p0.x + avail_w, p0.y + nav_h);
        bool clicked = ImGui::InvisibleButton("##nav", ImVec2(avail_w, nav_h));
        bool lit = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        if (clicked && i != g_page) {
            g_page = i;
            g_capture = -1;
            g_pad_capture = -1;
        }
        float round = em * 0.42f;
        if (selected) {
            dl->AddRectFilled(p0, p1, ImGui::GetColorU32(kAccentTint), round);
            dl->AddRectFilled(ImVec2(p0.x, p0.y + nav_h * 0.18f), ImVec2(p0.x + em * 0.28f, p1.y - nav_h * 0.18f),
                              ImGui::GetColorU32(kAccent), em * 0.14f);
        } else if (lit) {
            dl->AddRectFilled(p0, p1, ImGui::GetColorU32(kCardBg), round);
        }
        std::string label = Ui(kPages[i]);
        ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        dl->AddText(ImVec2(p0.x + em * 0.85f, p0.y + (nav_h - ts.y) * 0.5f),
                    ImGui::GetColorU32(selected ? kAccent : lit ? kText : kDim), label.c_str());
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, em * 0.14f));
    }

    // anchor action buttons at bottom of sidebar
    float btn_h = ImGui::GetFrameHeight() + em * 0.35f;
    float bottom_need = (g_page == kCreditsPage ? btn_h : btn_h * 2 + em * 0.5f) + em * 0.3f;
    float remain = ImGui::GetContentRegionAvail().y - bottom_need;
    if (remain > 0.0f) ImGui::Dummy(ImVec2(0, remain));

    if (g_page != kCreditsPage) {
        if (ImGui::Button(Ui("DEFAULTS").c_str(), ImVec2(avail_w, btn_h))) Defaults();
        ImGui::Dummy(ImVec2(0, em * 0.25f));
    }
    ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.90f, 0.36f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.72f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccentDark);
    if (ImGui::Button(Ui("RESUME").c_str(), ImVec2(avail_w, btn_h))) ToggleMenu();
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DrawContentPane(float em) {
    ImGui::BeginChild("##content_wrap", ImVec2(0, 0), ImGuiChildFlags_NavFlattened);

    // category header bar
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::TextUnformatted(Ui(kPages[g_page]).c_str());
    ImGui::PopStyleColor();

    const char* hint = "";
    if (g_capture >= 0) hint = "Press a key or mouse button to bind, Escape cancels";
    else if (g_pad_capture >= 0) hint = "Press a button on the controller to bind, wait 5 seconds to cancel";
    else if (g_page == kKeyboardPage && g_hovered_box >= 0) hint = "Right click or Delete clears the key";
    else if (g_page == kControllerPage && g_hovered_box >= 0) hint = "Right click or Delete restores the default";

    if (hint[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, kBlue);
        ImGui::TextUnformatted(Ui(hint, false).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, em * 0.2f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, em * 0.25f));

    ImGui::BeginChild("##rows", ImVec2(0, 0), ImGuiChildFlags_NavFlattened);
    g_hovered_box = -1;
    DrawRows(g_page);
    DragScroll();
    ImGui::EndChild();

    ImGui::EndChild();
}

void DrawPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float em = ImGui::GetFontSize();
    float win_w = std::min(vp->Size.x * 0.92f, em * 56.0f);
    float win_h = std::min(vp->Size.y * 0.88f, em * 34.0f);
    float rounding = em * 0.8f;

    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
    for (const auto& [col, color] : kPanelColors) ImGui::PushStyleColor(col, color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, em * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, em * 0.42f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.1f, em * 1.1f));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, em * 1.25f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, em * 2.0f);

    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##settings", nullptr, kFlags)) {
        float strip_h = em * 0.42f;
        DrawCheckeredTop(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize().x, strip_h, rounding);
        ImGui::Dummy(ImVec2(0, strip_h * 0.8f));

        float sidebar_w = std::clamp(win_w * 0.26f, em * 11.5f, em * 15.0f);
        DrawSidebar(em, sidebar_w);
        ImGui::SameLine(0, em * 0.9f);
        DrawContentPane(em);
    }
    ImGui::End();
    ImGui::PopStyleVar(7);
    ImGui::PopStyleColor(int(std::size(kPanelColors)));
}

uint32_t RefreshRate() {
    SDL_Window* w = static_cast<SDL_Window*>(Window());
    const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(w ? SDL_GetDisplayForWindow(w) : SDL_GetPrimaryDisplay());
    return m && m->refresh_rate > 1 ? uint32_t(std::lround(m->refresh_rate)) : 0;
}

void PaceAt(uint32_t cap) {
    using clock = std::chrono::steady_clock;
    static clock::time_point next = clock::now();
    auto now = clock::now();
    if (!cap) {
        next = now;
        return;
    }
    auto period = std::chrono::nanoseconds(1000000000ull / cap);
    next += period;
    if (next < now - period) next = now;
    while (clock::now() + std::chrono::milliseconds(2) < next) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    while (clock::now() < next) std::this_thread::yield();
}

}  // namespace

void PaceFrame() {
    if (Get().fps_cap != kFpsCapRefresh) {
        Pace();
        return;
    }
    constexpr uint32_t kRefreshMargin = 3;
    uint32_t rate = RefreshRate();
    PaceAt(rate > kRefreshMargin ? rate - kRefreshMargin : 0);
}

const rows::Row* rows::Page(int page, size_t* count) {
    if (page == kKeyboardPage) {
        *count = InputRows().size();
        return InputRows().data();
    }
    if (page == kControllerPage) {
        *count = PadRows().size();
        return PadRows().data();
    }
    if (page == kCheatsPage) {
        *count = std::size(kCheatRows);
        return kCheatRows;
    }
    if (page == kTouchPage) {
        *count = std::size(kTouchRows);
        return kTouchRows;
    }
    if (page == kGeneralPage) {
        *count = std::size(kGeneralRows);
        return kGeneralRows;
    }
    if (page == kCreditsPage) {
        *count = std::size(kCreditRows);
        return kCreditRows;
    }
    *count = std::size(kGraphicsRows);
    return kGraphicsRows;
}

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

bool Capturing() { return g_capture >= 0 || g_pad_capture >= 0; }

void ToggleMenu() {
    bool open = !MenuOpen();
    g_open.store(open, std::memory_order_relaxed);
    g_capture = -1;
    g_pad_capture = -1;
    if (open) input::ReleaseCursor();
}

void AppendPerf(std::string& line, const char* name, float load, float temp) {
    if (load < 0 && temp < 0) return;
    char part[48];
    int n = std::snprintf(part, sizeof(part), "%s%s", line.empty() ? "" : "  |  ", name);
    if (load >= 0) n += std::snprintf(part + n, sizeof(part) - n, " %.0f%%", load);
    if (temp >= 0) std::snprintf(part + n, sizeof(part) - n, " %.0f\xC2\xB0" "C", temp);
    line += part;
}

// compact pill HUD for FPS and device telemetry
void DrawFpsCounter(double fps) {
    const game::Options& o = game::Opt();
    if (!o.fps_counter || MenuOpen()) return;
    host::SamplePerf();
    char fps_text[24];
    std::snprintf(fps_text, sizeof(fps_text), "%.0f FPS", fps);
    std::string perf;
    AppendPerf(perf, "CPU", o.fps_usage ? host::CpuLoad() : -1, o.fps_temps ? host::CpuTemp() : -1);
    AppendPerf(perf, "GPU", o.fps_usage ? host::GpuLoad() : -1, o.fps_temps ? host::GpuTemp() : -1);
    if (o.fps_temps) AppendPerf(perf, "BAT", -1, host::BatteryTemp());

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    float big = ImGui::GetFontSize() * 1.25f;
    float small = ImGui::GetFontSize() * 0.92f;
    ImVec2 fs = ImGui::GetFont()->CalcTextSizeA(big, FLT_MAX, 0, fps_text);
    ImVec2 ps = perf.empty() ? ImVec2(0, 0) : ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0, perf.c_str());

    float pad_x = small * 0.75f;
    float pad_y = small * 0.42f;
    float accent_w = std::max(3.0f, small * 0.25f);
    float inner_gap = perf.empty() ? 0.0f : small * 0.25f;
    float box_w = accent_w + pad_x * 2.0f + std::max(fs.x, ps.x);
    float box_h = pad_y * 2.0f + fs.y + inner_gap + ps.y;

    float margin = screen.y * 0.025f;
    uint32_t pos = o.fps_position;
    bool bottom = pos == kFpsBottomLeft || pos == kFpsBottom || pos == kFpsBottomRight;
    float y = pos == kFpsTop ? margin
              : pos == kFpsBottom ? screen.y * 0.87f - box_h
              : bottom ? screen.y - margin - box_h
                       : margin;
    float side = bottom ? margin : screen.x * 0.12f;
    float x = (pos == kFpsTopLeft || pos == kFpsBottomLeft) ? side
              : (pos == kFpsTopRight || pos == kFpsBottomRight) ? screen.x - side - box_w
                                                                : (screen.x - box_w) * 0.5f;

    float round = small * 0.45f;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + box_w, y + box_h), IM_COL32(12, 17, 27, 205), round);
    dl->AddRect(ImVec2(x, y), ImVec2(x + box_w, y + box_h), IM_COL32(255, 212, 40, 120), round, 0, 1.2f);
    dl->AddRectFilled(ImVec2(x + 2.0f, y + pad_y * 0.7f),
                      ImVec2(x + 2.0f + accent_w, y + box_h - pad_y * 0.7f), IM_COL32(255, 212, 40, 240),
                      accent_w * 0.5f);

    float tx = x + accent_w + pad_x;
    dl->AddText(ImGui::GetFont(), big, ImVec2(tx, y + pad_y), IM_COL32(255, 212, 40, 255), fps_text);
    if (!perf.empty()) {
        dl->AddText(ImGui::GetFont(), small, ImVec2(tx, y + pad_y + fs.y + inner_gap),
                    IM_COL32(215, 224, 240, 235), perf.c_str());
    }
}

void DrawUi(double fps) {
    game::g_fps = float(fps);
    DrawFpsCounter(fps);
    ImGuiIO& io = ImGui::GetIO();
    if (MenuOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.KeyRepeatRate = 0.1f;
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 540.0f);
    if (MenuOpen()) {
        PollCapture();
        PollPadCapture();
        DrawPanel();
    }
}

}  // namespace app
