# fooyin-scrobbler

The Scrobbler plugin from [fooyin](https://github.com/fooyin/fooyin), imported unchanged from [830d6f6bf492](https://github.com/fooyin/fooyin/commit/830d6f6bf49209b9eb02e99d5aeca369f94e05f4). The original source files, icons, copyright notices and GPL-3.0-or-later licence are preserved.

The import retains 93 upstream commits by 8 authors, including the original author and committer dates and commit messages. Filtering rewrites commit IDs; [the commit map](docs/upstream-commits.tsv) links them to the original history.

The filtered history joins the empty `Init` commit through an unrelated-history merge. Standalone build integration and feature changes are separate from this baseline.

![Import verification](docs/import-checks.png)

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

Released under GPL-3.0-or-later. Original copyright notices are retained. See [COPYING](COPYING).
