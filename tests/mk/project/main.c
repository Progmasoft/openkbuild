/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "parts.h"

/**
 * @brief Prints the greeting and calls the parts the configuration builds.
 *
 * @return 0.
 */
int
main(void)
{
        puts(CONFIG_GREETING);
#ifdef CONFIG_SERIAL
        Serial();
#endif
#ifdef CONFIG_EXTRA
        Extra();
#endif
        return (0);
}
