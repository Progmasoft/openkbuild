/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "openkbuild/OpenKBuild.Config.h"
#include "openkbuild/OpenKBuild.Options.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Writes config.h: a CONFIG_NAME macro for each option that is set.
 *
 * A bool or tristate y is 1, a tristate m defines CONFIG_NAME_MODULE as 1 instead, an int is its
 * number and a string a C string literal. An option that is not set has no macro.
 *
 * @param[in]     aIn_pConfig   The resolved configuration.
 * @param[in,out] aInOut_pFile  The stream.
 * @return false when writing fails.
 */
bool
OpenKBuild_Output_WriteHeader(const OpenKBuild_tConfig* aIn_pConfig, FILE* aInOut_pFile);

/**
 * @brief Writes config.mk: a CONFIG_NAME variable for BSD Make for each option that is set.
 *
 * A bool or tristate is y or m, an int its number and a string its C string literal, with $ and #
 * escaped for make. A makefile adds sources with SRCS-${CONFIG_NAME}+= file.c and builds SRCS-y.
 *
 * @param[in]     aIn_pConfig   The resolved configuration.
 * @param[in,out] aInOut_pFile  The stream.
 * @return false when writing fails.
 */
bool
OpenKBuild_Output_WriteMake(const OpenKBuild_tConfig* aIn_pConfig, FILE* aInOut_pFile);

/**
 * @brief Writes a configuration that sets every option to its default, with its help above it.
 *
 * @param[in]     aIn_pOptions  The options.
 * @param[in,out] aInOut_pFile  The stream.
 * @return false when writing fails.
 */
bool
OpenKBuild_Output_WriteDefaults(const OpenKBuild_tOptions* aIn_pOptions, FILE* aInOut_pFile);

#ifdef __cplusplus
}
#endif
