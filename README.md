# The Simpsons: Road Rage for Android

An unofficial native Android (arm64) port of the original Xbox version of *The Simpsons: Road Rage* (USA, Title ID `0x45410013`, XDK `4034`), built by static recompilation using [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) and the Android Vulkan/SDL3 host architecture from [RanggaCasper/ShaolinMonks-Android](https://github.com/RanggaCasper/ShaolinMonks-Android) & [RanggaCasper/Black-Android](https://github.com/RanggaCasper/Black-Android).

> [!NOTE]
> This repository contains **no game assets or recompiled game code**. You must supply your own legally dumped `default.xbe` or `.iso` (USA release, `default.xbe` SHA-1: `fd450329f44853575adf56baa13e79d938a9a7ca`).

## Building via GitHub Actions

1. Encrypt your `default.xbe` locally with a passphrase of your choice:
   ```bash
   openssl enc -aes-256-cbc -md sha256 -pbkdf2 -iter 200000 -salt \
     -in assets/default.xbe -out ci/default.xbe.enc -pass pass:<YOUR_SECRET_KEY>
   ```
2. Push this repository (including `ci/default.xbe.enc`) to GitHub.
3. In your GitHub repo: **Settings → Secrets and variables → Actions → New repository secret**:
   - `XBE_KEY` = `<YOUR_SECRET_KEY>`
   - *(Optional, for release signing)* `KEYSTORE_BASE64`, `KEYSTORE_PASSWORD`, `KEY_ALIAS`, `KEY_PASSWORD` (without these, the APK is signed with the debug key and installs directly).
4. Go to **Actions → Build → Run workflow** (or push a `v*` tag to also publish a GitHub Release).
5. Download the `RoadRage-*.apk` artifact when the build finishes.

## Installation on Android

1. Install the built APK (Android 10+, arm64, Vulkan 1.1+).
2. Open **Road Rage** and grant **All files access** when prompted.
3. Copy your game ISO (`*.iso`) or extracted game files (`default.xbe` + `*.rcf`, `art/`, `img/`, `movies/`, `sound/`, `typ/`) into `/storage/emulated/0/RoadRage`.

## XDK 4034 Notes

- Custom `sdk/xdk/4034/layout.toml` and `config/game.toml` verified directly against the USA `default.xbe` (XDK 4034 / DSOUND 3936).
- `src/d3d/d3d8.cpp` translates XDK 4034 render state indices (`0..81` simple, `82..116` deferred, `117..145` complex) to the runtime's internal enum and implements XDK 4034's `D3DDevice_GetBackBuffer`, `D3DDevice_SetVertexShaderConstant`, `D3DDevice_IsBusy`, `D3DDevice_SetRenderState_ZBias`, and 1-arg `D3D_MakeRequestedSpace`.
- `src/sound/` implements DSOUND 3936's bitmask `SetMixBins` and 3-argument `SetMixBinVolumes`.
