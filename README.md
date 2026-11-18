<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# OpenKBuild

OpenKBuild is the build system of the [OpenTala](https://github.com/Progmasoft/opentala) kernel. It is based on BSD
Make, not on GNU Make:

| Linux | OpenKBuild | Syntax |
| --- | --- | --- |
| Kbuild | OpenKBuild files | BSD Make |
| Kconfig | `OpenKBuild.conf.toml` | TOML |

It has two parts:

- **`openkbuild`**, a command in GNU99 that reads the option definitions and a configuration and writes `config.h`
  and `config.mk`;
- **`openkbuild.conf.mk` and `openkbuild.kern.mk`**, the BSD Make files a kernel's Makefile includes.

## Option definitions: `OpenKBuild.conf.toml`

Each option is a table `[options.NAME]`. The name is upper case letters, digits and underscores, starting with a
letter.

```toml
[options.SERIAL]
type = "bool"
default = true
help = "Write kernel messages to the first serial port."

[options.SERIAL_BAUD]
type = "int"
default = 115200
range = [9600, 115200]
depends-on = ["SERIAL"]

[options.NET]
type = "tristate"
default = "m"

[options.LOCALVERSION]
type = "string"
default = "-dev"
```

| Key | Meaning |
| --- | --- |
| `type` | `"bool"` (`true`, `false`), `"tristate"` (`"y"`, `"m"`, `"n"`), `"int"` or `"string"`; required |
| `default` | the value when the configuration does not set it; `false`, `"n"`, `0` or `""` when left out |
| `help` | what the option does; `openkbuild defconfig` writes it above the option |
| `depends-on` | bool or tristate options this one needs |
| `range` | `[minimum, maximum]`, for an int |

A string holds no control characters. Unknown keys, wrong types, unknown or cyclic dependencies and defaults out of
range are errors, reported as `file:line:column: message`.

## The configuration: `.config.toml`

The configuration sets the options it changes, one `NAME = value` line each; the others take their defaults. It
lives at the root of the kernel, as `.config` does for Linux, so `make clean` leaves it. While it does not exist, every
option takes its default.

```toml
SERIAL_BAUD = 9600
NET = "y"
```

The dependencies then limit the values, each option after those it depends on:

- an option with a dependency at `n` is not set, and a value the configuration gave it is reported;
- a tristate option is at most the lowest of its dependencies, so it is `m` when one of them is `m`;
- a bool option needs its dependencies at `y` or `m`.

## What `openkbuild generate` writes

For each option that is set, `config.h` has a macro and `config.mk` a variable:

| Option | `config.h` | `config.mk` |
| --- | --- | --- |
| bool or tristate `y` | `#define CONFIG_SERIAL 1` | `CONFIG_SERIAL= y` |
| tristate `m` | `#define CONFIG_NET_MODULE 1` | `CONFIG_NET= m` |
| int | `#define CONFIG_SERIAL_BAUD 115200` | `CONFIG_SERIAL_BAUD= 115200` |
| string | `#define CONFIG_LOCALVERSION "-dev"` | `CONFIG_LOCALVERSION= "-dev"` |

An option that is not set has neither. Strings are C string literals in both, with `$` and `#` escaped for make.

## A kernel's Makefile

```make
.include <openkbuild.conf.mk>

KERNEL=                 opentala
KCC=                    clang
KCFLAGS=                -std=gnu99 -ffreestanding -O2
SRCS-y+=                kern/Kern.Main.c
SRCS-${CONFIG_SERIAL}+= amd64/Amd64.Serial.c

.include <openkbuild.kern.mk>
```

`openkbuild.conf.mk` runs `openkbuild generate` when `build/config.mk` is missing or older than
`OpenKBuild.conf.toml` or `.config.toml`, then reads it, so the `CONFIG_` variables exist when the sources are listed.
`openkbuild.kern.mk` compiles `SRCS-y` with `build/config.h` included first, keeps the header dependencies, and links
`build/${KERNEL}`. The sources an option at `m` adds land in `SRCS-m`, which the Makefile builds as modules.

| Target | What it does |
| --- | --- |
| `all` | the kernel; the default |
| `defconfig` | writes `.config.toml` with every default and its help |
| `config` | runs `openkbuild generate` |
| `clean` | removes `build`; `.config.toml` stays |

The variables are documented at the top of each `.mk` file: `OBJ`, `KCONF`, `KCONFIG`, `OPENKBUILD`, `KCC`, `KCFLAGS`,
`KLD`, `KLDFLAGS`.

## The command

```text
openkbuild check     [--options FILE]
openkbuild defconfig [--options FILE] [--output FILE]
openkbuild generate  [--options FILE] [--config FILE] [--output-dir DIR]
openkbuild version | help
```

`--options` is `OpenKBuild.conf.toml` by default, `--config` and `--output` `.config.toml`, `--output-dir` `build`.
Files are written whole or not at all. The exit status is 0 on success, 1 when an input is wrong or a file cannot be
written, 2 for a command line it does not take.

## Building

It needs BSD Make (`bmake` on Linux), a C compiler and, for the tests, [Criterion](https://github.com/Snaipe/Criterion).
TOML is read by [tomlc17](https://github.com/cktan/tomlc17), copied into `third_party/tomlc17`.

```sh
bmake                               # build/openkbuild
bmake test                          # the Criterion tests and the make test of mk/
bmake SANITIZE=address,undefined test
bmake format                        # clang-format 23
bmake install PREFIX=/usr/local     # bin/openkbuild, share/openkbuild/mk/
```

A Makefile finds the `.mk` files when make searches `PREFIX/share/openkbuild/mk`. `-m` replaces the search path, so
the system's own directory comes after it: `bmake -m PREFIX/share/openkbuild/mk -m "$(bmake -V .SYSPATH -f /dev/null)"`.

## License

[MIT](LICENSE.txt), also covered by the [Progmasoft Patent Grant 1.1](PATENTS.md). Participation follows the
[Code of Conduct](CODE_OF_CONDUCT.md).
