<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Development

OpenKBuild needs BSD Make (`bmake` on Linux), a C compiler and Criterion.

```sh
bmake                                  # build/openkbuild
bmake test                             # the Criterion tests and tests/mk/run.sh
bmake CC=clang SANITIZE=address,undefined test
bmake format                           # clang-format 23 on the C files
bmake install PREFIX=/usr/local
```

| Tests | What they check |
| --- | --- |
| `tests/CliTests.c` | the command line, accepted and refused |
| `tests/OptionsTests.c` | the option definitions and their errors |
| `tests/ConfigTests.c` | the configuration and the defaults |
| `tests/OutputTests.c` | `config.h`, `config.mk` and `defconfig` |
| `tests/mk/run.sh` | the BSD Make files, on the project in `tests/mk/project/` |

CI builds and tests with GCC and Clang, with AddressSanitizer and UndefinedBehaviorSanitizer, checks the format with
clang-format 23, and checks the workflows with zizmor and actionlint.
