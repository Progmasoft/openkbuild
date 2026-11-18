# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# openkbuild.kern.mk: the rules of an OpenKBuild kernel, for BSD Make. A kernel's Makefile
# includes openkbuild.conf.mk first and this file last; openkbuild.conf.mk shows how.
#
# The sources are SRCS-y, .c and .S files: a Makefile adds the ones an option enables with
# SRCS-${CONFIG_NAME}+= file.c, which is SRCS-y when the option is y. Each is compiled with
# ${OBJ}/config.h included first, and they are linked in their order.
#
# Variables a Makefile may set:
#   KERNEL      the name of the output, ${OBJ}/${KERNEL} (required)
#   SRCS-y      the sources
#   KCC         the compiler; ${CC}
#   KCFLAGS     the flags of each compilation
#   KLD         the linker; ${KCC}
#   KLDFLAGS    the flags of the link
#
# Targets: all (the default), defconfig (writes ${KCONFIG} with every default), config (runs
# openkbuild generate) and clean (removes ${OBJ}; the configuration stays).

.if !defined(KERNEL)
.error openkbuild.kern.mk: set KERNEL before including it
.endif
.if !defined(_OKB_GENERATE)
.error openkbuild.kern.mk: include openkbuild.conf.mk first
.endif

KCC?=		${CC}
KCFLAGS?=
KLD?=		${KCC}
KLDFLAGS?=

all: ${OBJ}/${KERNEL}

KOBJS=
.for src in ${SRCS-y}
KOBJS+=		${OBJ}/${src:R}.o
${OBJ}/${src:R}.o: ${src} ${_OKB_H}
	@mkdir -p ${.TARGET:H}
	${KCC} ${KCFLAGS} -include ${_OKB_H} -MMD -MP -MF ${.TARGET:R}.d -c ${src} -o ${.TARGET}
.endfor

.for dep in ${KOBJS:R:S/$/.d/}
.-include "${dep}"
.endfor

${OBJ}/${KERNEL}: ${KOBJS}
	${KLD} ${KLDFLAGS} -o ${.TARGET} ${KOBJS}

defconfig:
	${OPENKBUILD} defconfig --options ${KCONF} --output ${KCONFIG}

config:
	${_OKB_GENERATE}

clean:
	rm -rf ${OBJ}

.PHONY: all defconfig config clean
