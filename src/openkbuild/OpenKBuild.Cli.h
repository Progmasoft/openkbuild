/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "openkbuild/OpenKBuild.Options.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The version of openkbuild. */
#define OPENKBUILD_VERSION "0.1.0"

/** The commands of openkbuild. */
typedef enum OpenKBuild_Command
{
        OpenKBuild_kHelp,
        OpenKBuild_kVersion,
        OpenKBuild_kCheck,
        OpenKBuild_kDefconfig,
        OpenKBuild_kGenerate,
} OpenKBuild_tCommand;

/** A command line that was accepted; later stages read only this, never the arguments. */
typedef struct OpenKBuild_CommandLine
{
        OpenKBuild_tCommand command;
        /** --options: the option definitions; OpenKBuild.conf.toml by default. */
        const char* pOptionsPath;
        /** --config: the values, for generate; .config.toml by default. */
        const char* pConfigPath;
        /**
         * Whether --config was given: a missing default file means the defaults, a missing given
         * one an error.
         */
        bool isConfigGiven;
        /** --output: the file defconfig writes; .config.toml by default. */
        const char* pOutputPath;
        /** --output-dir: where generate writes config.h and config.mk; build by default. */
        const char* pOutputDirectory;
} OpenKBuild_tCommandLine;

/**
 * @brief Reads the arguments of openkbuild into a command line.
 *
 * The forms are: help, --help, -h; version, --version; check [--options FILE];
 * defconfig [--options FILE] [--output FILE]; generate [--options FILE] [--config FILE]
 * [--output-dir DIR]. A flag takes its value as the next argument or after "=". A flag that the
 * command does not take, a flag given twice, a missing value or an extra argument is an error.
 *
 * @param[in]  aArgumentCount    The number of arguments, the program name included.
 * @param[in]  aIn_ppArguments   The arguments; the command line points into them.
 * @param[out] aOut_pCommandLine The command line.
 * @param[out] aOut_pDiagnostic  The reason, when it fails.
 * @return true when the arguments form a command line.
 */
bool
OpenKBuild_Cli_Parse(int                      aArgumentCount,
                     char* const*             aIn_ppArguments,
                     OpenKBuild_tCommandLine* aOut_pCommandLine,
                     OpenKBuild_tDiagnostic*  aOut_pDiagnostic);

/**
 * @brief Writes how to call openkbuild.
 *
 * @param[in,out] aInOut_pFile The stream.
 */
void
OpenKBuild_Cli_WriteUsage(FILE* aInOut_pFile);

#ifdef __cplusplus
}
#endif
