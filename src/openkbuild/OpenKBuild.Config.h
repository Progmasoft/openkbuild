/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "openkbuild/OpenKBuild.Options.h"
#include "tomlc17.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The resolved value of every option: the configured value or the default, limited by the
 * dependencies.
 */
typedef struct OpenKBuild_Config
{
        const OpenKBuild_tOptions* pOptions;
        /** The value of each option, in the order of the options. */
        OpenKBuild_tValue* pValues;
        /** Whether each option is set: not when a dependency is n, nor a bool or tristate at n. */
        bool*         pIsSet;
        const char*   pPath;
        toml_result_t document;
} OpenKBuild_tConfig;

/**
 * @brief Resolves the options with the values of a configuration file.
 *
 * The file holds NAME = value lines, one for each option it sets; an option it leaves out takes
 * its default. An unknown name, a wrong type or a number outside the range is an error.
 *
 * The dependencies then limit the values, in an order where each option follows those it depends
 * on: an option one of whose dependencies is n is not set; a tristate option is at most the lowest
 * of its dependencies, so it is m when one of them is m. A bool option needs its dependencies to
 * be y or m. A configured value that a dependency turns off or lowers is reported on aInOut_pNotes.
 *
 * @param[out]    aOut_pConfig      The configuration; deinitialize with OpenKBuild_Config_Deinit().
 * @param[in]     aIn_pOptions      The options; they must outlive the configuration.
 * @param[in]     aIn_pPath         The configuration file, or NULL for the defaults alone.
 * @param[in,out] aInOut_pNotes     Where to report lowered values, or NULL.
 * @param[out]    aOut_pDiagnostic  The reason, when it fails.
 * @return true when the file was read and every value is valid.
 */
bool
OpenKBuild_Config_Init(OpenKBuild_tConfig*        aOut_pConfig,
                       const OpenKBuild_tOptions* aIn_pOptions,
                       const char*                aIn_pPath,
                       FILE*                      aInOut_pNotes,
                       OpenKBuild_tDiagnostic*    aOut_pDiagnostic);

/**
 * @brief Resolves the options with values from text, as OpenKBuild_Config_Init() does.
 *
 * @param[out]    aOut_pConfig      The configuration; deinitialize with OpenKBuild_Config_Deinit().
 * @param[in]     aIn_pOptions      The options; they must outlive the configuration.
 * @param[in]     aIn_pText         The TOML text, ending in a null character.
 * @param[in]     aIn_pName         The name given to the text in diagnostics.
 * @param[in,out] aInOut_pNotes     Where to report lowered values, or NULL.
 * @param[out]    aOut_pDiagnostic  The reason, when it fails.
 * @return true when every value is valid.
 */
bool
OpenKBuild_Config_InitFromText(OpenKBuild_tConfig*        aOut_pConfig,
                               const OpenKBuild_tOptions* aIn_pOptions,
                               const char*                aIn_pText,
                               const char*                aIn_pName,
                               FILE*                      aInOut_pNotes,
                               OpenKBuild_tDiagnostic*    aOut_pDiagnostic);

/**
 * @brief Releases what the configuration holds, but not the structure itself.
 *
 * @param[in,out] aInOut_pConfig The configuration.
 */
void
OpenKBuild_Config_Deinit(OpenKBuild_tConfig* aInOut_pConfig);

#ifdef __cplusplus
}
#endif
