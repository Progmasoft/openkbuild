<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Changelog

Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- `openkbuild` reads `OpenKBuild.conf.toml` and `.config.toml` (bool, tristate, int and string options, with
  defaults, ranges and dependencies) and writes `config.h` and `config.mk`; `openkbuild.conf.mk` and
  `openkbuild.kern.mk` build a kernel from them with BSD Make.
- The repository of OpenKBuild: README, MIT license, Patent Grant and code of conduct.
