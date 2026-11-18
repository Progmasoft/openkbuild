# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Builds openkbuild with BSD Make (bmake on Linux).
#
#   make            build/openkbuild
#   make test       the Criterion tests and the make test of mk/
#   make format     checks the layout of the C files with clang-format 23
#   make install    PREFIX/bin/openkbuild and PREFIX/share/openkbuild/mk/*.mk
#
# SANITIZE=address,undefined builds everything with those sanitizers.

PREFIX?=	/usr/local
DESTDIR?=
CC?=		cc
CFLAGS?=	-O2 -g
LDFLAGS?=
CLANG_FORMAT?=	clang-format
INSTALL?=	install

BUILD=		build
TOOL=		${BUILD}/openkbuild
TESTS=		${BUILD}/openkbuild-tests

.if defined(SANITIZE) && !empty(SANITIZE)
_SANITIZE=	-fsanitize=${SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all
.else
_SANITIZE=
.endif

# OpenKBuild's own code is GNU99; tomlc17 is C17 and keeps its own warnings.
OKB_CFLAGS=	-std=gnu99 -Wall -Wextra -Werror -I. -Isrc -Ithird_party/tomlc17 ${_SANITIZE}
TOML_CFLAGS=	-std=c17 -Ithird_party/tomlc17 ${_SANITIZE}

LIB_SRCS=	src/openkbuild/OpenKBuild.Options.c src/openkbuild/OpenKBuild.Config.c \
		src/openkbuild/OpenKBuild.Output.c src/openkbuild/OpenKBuild.Cli.c
MAIN_SRCS=	src/openkbuild/OpenKBuild.Main.c
TEST_SRCS=	tests/OptionsTests.c tests/ConfigTests.c tests/OutputTests.c tests/CliTests.c
HDRS=		src/openkbuild/OpenKBuild.Options.h src/openkbuild/OpenKBuild.Config.h \
		src/openkbuild/OpenKBuild.Output.h src/openkbuild/OpenKBuild.Cli.h \
		tests/Tests.h
TOML_OBJ=	${BUILD}/third_party/tomlc17.o
MK_FILES=	mk/openkbuild.conf.mk mk/openkbuild.kern.mk

all: ${TOOL}

LIB_OBJS=
MAIN_OBJS=
TEST_OBJS=
.for src in ${LIB_SRCS} ${MAIN_SRCS} ${TEST_SRCS}
.  if !empty(LIB_SRCS:M${src})
LIB_OBJS+=	${BUILD}/${src:R}.o
.  elif !empty(MAIN_SRCS:M${src})
MAIN_OBJS+=	${BUILD}/${src:R}.o
.  else
TEST_OBJS+=	${BUILD}/${src:R}.o
.  endif
${BUILD}/${src:R}.o: ${src} ${HDRS} third_party/tomlc17/tomlc17.h
	@mkdir -p ${.TARGET:H}
	${CC} ${CFLAGS} ${OKB_CFLAGS} -c ${src} -o ${.TARGET}
.endfor

${TOML_OBJ}: third_party/tomlc17/tomlc17.c third_party/tomlc17/tomlc17.h
	@mkdir -p ${.TARGET:H}
	${CC} ${CFLAGS} ${TOML_CFLAGS} -c third_party/tomlc17/tomlc17.c -o ${.TARGET}

${TOOL}: ${LIB_OBJS} ${MAIN_OBJS} ${TOML_OBJ}
	${CC} ${CFLAGS} ${_SANITIZE} ${LDFLAGS} -o ${.TARGET} ${MAIN_OBJS} ${LIB_OBJS} ${TOML_OBJ}

${TESTS}: ${LIB_OBJS} ${TEST_OBJS} ${TOML_OBJ}
	${CC} ${CFLAGS} ${_SANITIZE} ${LDFLAGS} -o ${.TARGET} ${TEST_OBJS} ${LIB_OBJS} ${TOML_OBJ} \
		-lcriterion

test: ${TOOL} ${TESTS}
	${TESTS}
	sh tests/mk/run.sh ${TOOL}

format:
	${CLANG_FORMAT} --style=file --dry-run --Werror ${LIB_SRCS} ${MAIN_SRCS} ${TEST_SRCS} \
		${HDRS}

install: ${TOOL}
	${INSTALL} -d ${DESTDIR}${PREFIX}/bin ${DESTDIR}${PREFIX}/share/openkbuild/mk
	${INSTALL} -m 755 ${TOOL} ${DESTDIR}${PREFIX}/bin/openkbuild
	${INSTALL} -m 644 ${MK_FILES} ${DESTDIR}${PREFIX}/share/openkbuild/mk/

clean:
	rm -rf ${BUILD}

.PHONY: all test format install clean
