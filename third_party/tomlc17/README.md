<!-- SPDX-FileCopyrightText: 2024-2026 CK Tan -->
<!-- SPDX-License-Identifier: MIT -->

# tomlc17

The TOML parser OpenKBuild reads its files with, copied unchanged from
[cktan/tomlc17](https://github.com/cktan/tomlc17), release `R261116`, under its [MIT license](LICENSE). It is C17
and is compiled with `-std=c17`; OpenKBuild's own code is GNU99 and only includes `tomlc17.h`.

| File | SHA-256 |
| --- | --- |
| `tomlc17.c` | `41cedef3c1214a5bbfac750bd2017ac7ba2952429b436af50f2582decf6e29cf` |
| `tomlc17.h` | `29c0909092f4b23c7edce705167064f4a90548db5334b4aa2cd95d7ad801adb9` |
| `LICENSE` | `f949d0976f85076c96fc5122f0632b5a1b806d8b2badbe968c42c97230e6e79e` |

To update, copy `src/tomlc17.c`, `src/tomlc17.h` and `LICENSE` of a newer release and change the release and the
sums here.
