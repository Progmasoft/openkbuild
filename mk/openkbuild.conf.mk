# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# openkbuild.conf.mk: the configuration of an OpenKBuild kernel, for BSD Make.
#
# A kernel's Makefile includes it first, so that the CONFIG_ variables exist when the sources are
# listed, and openkbuild.kern.mk last:
#
#   .include <openkbuild.conf.mk>
#
#   KERNEL=			opentala
#   SRCS-y+=		kern/Kern.Main.c
#   SRCS-${CONFIG_SERIAL}+=	amd64/Amd64.Serial.c
#
#   .include <openkbuild.kern.mk>
#
# When ${OBJ}/config.mk is missing or older than OpenKBuild.conf.toml or the configuration,
# openkbuild generate writes it and ${OBJ}/config.h again; then config.mk is read. The
# configuration is ${KCONFIG}; while it does not exist, every option takes its default.
#
# Variables a Makefile may set before including it:
#   OBJ         the output directory; build
#   KCONF       the option definitions; OpenKBuild.conf.toml
#   KCONFIG     the configuration; .config.toml
#   OPENKBUILD  the openkbuild command

OBJ?=		build
KCONF?=		OpenKBuild.conf.toml
KCONFIG?=	.config.toml
OPENKBUILD?=	openkbuild

_OKB_MK=	${OBJ}/config.mk
_OKB_H=		${OBJ}/config.h
_OKB_GENERATE=	${OPENKBUILD} generate --options ${KCONF} --output-dir ${OBJ}
.if exists(${KCONFIG})
_OKB_GENERATE+=	--config ${KCONFIG}
.endif

# clean and defconfig do not read the configuration.
.if !make(clean) && !make(defconfig)
_OKB_STALE!=	if [ ! -f ${_OKB_MK} ] || [ ${KCONF} -nt ${_OKB_MK} ] \
		    || { [ -f ${KCONFIG} ] && [ ${KCONFIG} -nt ${_OKB_MK} ]; }; then echo yes; fi
.  if ${_OKB_STALE} == "yes"
_OKB_RESULT!=	${_OKB_GENERATE} >&2 && echo ok || echo failed
.    if ${_OKB_RESULT} != "ok"
.error openkbuild generate failed
.    endif
.  endif
.include "${_OKB_MK}"
.endif
