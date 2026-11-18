<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Architecture

OpenKBuild has two halves that meet in two generated files, `build/config.h` and `build/config.mk`.

## The command, `openkbuild`

GNU99, in `src/openkbuild/`, one module per responsibility:

| File | What it does |
| --- | --- |
| `OpenKBuild.Main.c` | The entry point: reads the command line and runs the command it names. |
| `OpenKBuild.Cli.c` | The command line, read by hand into a typed value: `check`, `defconfig`, `generate` and their options. |
| `OpenKBuild.Options.c` | Reads `OpenKBuild.conf.toml` through tomlc17 and checks every option: its type, default, range and dependencies, refusing unknown keys and cycles with `file:line:column` messages. |
| `OpenKBuild.Config.c` | Reads `.config.toml`, the configuration, and gives every option its value or its default. |
| `OpenKBuild.Output.c` | Writes `config.h` (`#define CONFIG_X`) and `config.mk` (`CONFIG_X=`), and `defconfig`'s commented configuration. |

tomlc17 is vendored in `third_party/tomlc17/`, built as C17 with its own warnings.

## The BSD Make files

| File | Included | What it does |
| --- | --- | --- |
| `mk/openkbuild.conf.mk` | first | Runs `openkbuild generate` when the definitions or the configuration change, and includes `config.mk`. |
| `mk/openkbuild.kern.mk` | last | Builds the kernel from `SRCS` and the `SRCS-${CONFIG_X}` lists of the options that are on. |

A kernel's Makefile includes the first, lists its sources, and includes the second; `tests/mk/project/` is such a
kernel in miniature.
