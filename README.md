# fooyin-scrobbler

This repository provides the standalone Scrobbler plugin for [fooyin](https://github.com/fooyin/fooyin). The plugin retains the upstream icons, copyright notices and GPL-3.0-or-later licence.

## Remote playback

Now Playing and completed remote scrobbles run entirely in the plugin through fooyin’s public playback-progress API. The existing listened-time threshold, service queues, metadata requirements and per-playback deduplication are preserved. Local tracks retain the existing `trackPlayed` path.

## Standalone build

Requires fooyin 0.15, its development SDK, CMake 3.25 or later, Ninja, a C++23 compiler and Qt 6. Tests also require GoogleTest and Qt Test.

## Build

```sh
mkdir --parents tmp/runtime
export TMPDIR="$PWD/tmp/runtime"
cmake -S . -B tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$FOOYIN_PREFIX" -DCMAKE_INSTALL_PREFIX="$PWD/tmp/install"
cmake --build tmp/build --parallel 2
ctest --test-dir tmp/build --output-on-failure --parallel 1
```

Set `FOOYIN_PREFIX` to the matching fooyin SDK installation. Use `-DBUILD_TESTING=OFF` when only building the module. The module is built as `fyplugin_scrobbler`, retaining the upstream plugin identity and settings names. Do not load both this module and the bundled scrobbler in the same fooyin process.

Tests use synthetic metadata, isolated account/cache files and in-memory network replies. They cover remote submission without a core `trackPlayed` event, native library URIs, local playback, seeking, pause/replay, restored progress, duplicate events and metadata eligibility. No real service account is required.

## macOS

Use the same fooyin SDK, Qt installation and compiler as the host app. For Jellyfin playback, use the [`external-libraries` host branch](https://github.com/nkbt/fooyin/tree/external-libraries) and the [Jellyfin macOS instructions](https://github.com/nkbt/fooyin-jellyfin#macos). Configure the host with `-DINSTALL_HEADERS=ON -DPLUGIN_SELECTION=-scrobbler` to exclude the bundled scrobbler.

Set `FOOYIN_PREFIX` to the development SDK prefix and `FOOYIN_INSTALL_PREFIX` to the app installation prefix. The Jellyfin instructions define both variables. Keep the development SDK separate from the deployed app so tests load one copy of Qt:

```sh
cmake -S . -B tmp/macos-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$FOOYIN_PREFIX;$(brew --prefix)" -DCMAKE_INSTALL_PREFIX="$FOOYIN_INSTALL_PREFIX" -DBUILD_TESTING=ON
QT_QPA_PLATFORM=offscreen cmake --build tmp/macos-build --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir tmp/macos-build --output-on-failure --parallel 1
cmake --install tmp/macos-build
```

The installed module is `fooyin.app/Contents/PlugIns/fyplugin_scrobbler.so`. Keep one scrobbler module in the app bundle. After installing both external plugins, run the host bundle deployment and sign the complete app as described in the Jellyfin instructions. Complete each installation command before starting the next command.

The host bundle includes the SDL3 runtime when Homebrew provides SDL2-compat. Enable the SDL2 plugin if the source profile disables it. Use `SDL2 (coreaudio)` with the default device when copying audio settings from Linux.

The module retains the scrobbler settings and service identities. Copy `fooyin.conf` and the account database in `fooyin.db` when moving a profile. Current authentication data resides in the database. The host imports legacy credentials from `fooyin.conf` when no database account exists. Keep profile directories and backups private because `AccountStore` stores account values without encryption.

Released under GPL-3.0-or-later. Original copyright notices are retained. See [COPYING](COPYING).
