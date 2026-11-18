# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# A program built the way a kernel is, for the make test of OpenKBuild.

.include <openkbuild.conf.mk>

KERNEL=			program
SRCS-y+=		main.c
SRCS-${CONFIG_SERIAL}+=	serial.c
SRCS-${CONFIG_EXTRA}+=	extra.c

.include <openkbuild.kern.mk>
