#!/bin/sh
# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Usage: run.sh <openkbuild>
#
# Builds tests/mk/project with mk/openkbuild.conf.mk and mk/openkbuild.kern.mk, as a kernel is
# built, and checks that the configuration picks the sources and the macros.

set -eu

tool=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
here=$(cd "$(dirname "$0")" && pwd)
mk=$(cd "$here/../../mk" && pwd)
make=${BMAKE:-bmake}
# -m replaces the search path of <...> includes, so the system's own directory goes after mk/.
system=$("$make" -V .SYSPATH -f /dev/null)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$here"/project/* "$work"/
cd "$work"

build() {
        # Each directory of the system path is one word, and each gets its own -m.
        # shellcheck disable=SC2046,SC2086
        "$make" -m "$mk" $(printf -- '-m %s ' $system) OPENKBUILD="$tool" "$@"
}

fail() {
        echo "mk test: $*"
        exit 1
}

# No configuration: the defaults build serial.c and leave out extra.c.
build >build.log 2>&1 || { cat build.log; fail "the build with the defaults failed"; }
grep -q "does not exist; using the defaults" build.log || fail "no note about the defaults"
[ "$(./build/program | tr '\n' ' ')" = "hello serial " ] || fail "wrong output: $(./build/program)"
[ -f build/serial.o ] && [ ! -f build/extra.o ] || fail "the defaults picked the wrong sources"

# A configuration newer than config.mk: generated again, extra.c in, serial.c out.
build defconfig >/dev/null
# The greeting holds a $ that make and the shell must leave alone.
# shellcheck disable=SC2016
sed -e 's/^EXTRA = false/EXTRA = true/' -e 's/^SERIAL = true/SERIAL = false/' \
        -e 's/^GREETING = .*/GREETING = "hi $USER"/' .config.toml >config.new
mv config.new .config.toml
sleep 1
touch .config.toml
build >build.log 2>&1 || { cat build.log; fail "the build with .config.toml failed"; }
grep -q "CONFIG_EXTRA=" build/config.mk || fail "config.mk was not generated again"
output=$(./build/program | tr '\n' ' ')
# shellcheck disable=SC2016
[ "$output" = 'hi $USER extra ' ] || fail "wrong output: $output"
[ -f build/extra.o ] || fail "extra.c was not built"

# clean keeps the configuration.
build clean >/dev/null
[ -f .config.toml ] && [ ! -d build ] || fail "clean removed the configuration or kept build"

# A wrong configuration stops the build with the place of the error.
echo 'COLOR = true' >>.config.toml
if build >build.log 2>&1; then
        fail "a wrong configuration was accepted"
fi
grep -q ".config.toml:.*COLOR is no option of OpenKBuild.conf.toml" build.log \
        || { cat build.log; fail "no diagnostic for the wrong configuration"; }

echo "mk test: passed"
