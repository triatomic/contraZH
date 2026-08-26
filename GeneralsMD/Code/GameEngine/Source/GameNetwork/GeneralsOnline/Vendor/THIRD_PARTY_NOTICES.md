# Third-party notices - Generals Online vendored dependencies

The `Vendor/` tree carries prebuilt Win32 (x86) libraries imported from
GeneralsOnlineDevelopmentTeam/GameClient. They are used only when the game is
configured with `RTS_BUILD_GENERALS_ONLINE=ON`. Each ships under its own license:

| Component | Directory | License |
|---|---|---|
| Valve GameNetworkingSockets | `ValveNetworkingSockets/` | BSD-3-Clause (Valve Corporation) |
| WebRTC (webrtc-lite, steamwebrtc) | `ValveNetworkingSockets/` | BSD-3-Clause (The WebRTC project authors / Google) |
| Abseil | `ValveNetworkingSockets/abseil_dll.*` | Apache-2.0 (Google) |
| Protocol Buffers | `ValveNetworkingSockets/libprotobuf.*` | BSD-3-Clause (Google) |
| OpenSSL 3 (libcrypto-3, libssl-3) | `ValveNetworkingSockets/`, `libcurl/` | Apache-2.0 (The OpenSSL Project) |
| libcurl | `libcurl/` | curl license (MIT-like, Daniel Stenberg and contributors) |
| zlib (zlib1.dll) | `libcurl/` | zlib license (Jean-loup Gailly, Mark Adler) |
| sentry-native | `sentry/` | MIT (Functional Software, Inc. / Sentry) |
| Discord RPC | `DiscordRPC/` | MIT (Discord Inc.) - full text in `DiscordRPC/LICENSE.txt`, deployed beside the exe |
| libsodium | `libsodium/` | ISC (Frank Denis) |
| nlohmann/json (`json.hpp`, one level up) | `../json.hpp` | MIT (Niels Lohmann) |
| stb_image, stb_image_resize2, stb_image_write | `stb_image/` (headers) | MIT / public domain (Sean Barrett) |

Statically linked OpenSSL alongside the GPL-3.0 game code relies on OpenSSL 3's
Apache-2.0 licensing (GPLv3-compatible). The upstream Generals Online project ships
the same combination.

When redistributing a build made with `RTS_BUILD_GENERALS_ONLINE=ON`, include this
notice and the Discord RPC license file that the build copies next to the exe.
