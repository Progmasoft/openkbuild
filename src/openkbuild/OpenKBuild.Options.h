/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tomlc17.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The size of the text of a diagnostic, its final null character included. */
#define OPENKBUILD_DIAGNOSTIC_SIZE 512U

/** What went wrong while reading a file, with its place: "file:line:column: message". */
typedef struct OpenKBuild_Diagnostic
{
        char text[ OPENKBUILD_DIAGNOSTIC_SIZE ];
} OpenKBuild_tDiagnostic;

/** The type of an option. */
typedef enum OpenKBuild_OptionType
{
        OpenKBuild_kBool,
        OpenKBuild_kTristate,
        OpenKBuild_kInt,
        OpenKBuild_kString,
} OpenKBuild_tOptionType;

/** The value of a tristate option; a bool option uses kNo and kYes only. */
typedef enum OpenKBuild_State
{
        OpenKBuild_kNo,
        OpenKBuild_kModule,
        OpenKBuild_kYes,
} OpenKBuild_tState;

/** A value of one option; which member holds it depends on the type of the option. */
typedef struct OpenKBuild_Value
{
        OpenKBuild_tState state;
        int64_t           number;
        const char*       pText;
} OpenKBuild_tValue;

/** One option of OpenKBuild.conf.toml. The strings belong to the parsed document. */
typedef struct OpenKBuild_Option
{
        const char*            pName;
        OpenKBuild_tOptionType type;
        OpenKBuild_tValue      defaultValue;
        bool                   hasRange;
        int64_t                minimum;
        int64_t                maximum;
        const char*            pHelp;
        size_t*                pDependsOn;
        size_t                 dependsOnCount;
        int                    line;
} OpenKBuild_tOption;

/** The options of one OpenKBuild.conf.toml, in the order of the file. */
typedef struct OpenKBuild_Options
{
        const char*         pPath;
        toml_result_t       document;
        OpenKBuild_tOption* pItems;
        size_t              count;
        /** The options in an order where each comes after those it depends on. */
        size_t* pOrder;
} OpenKBuild_tOptions;

/**
 * @brief Reads and checks the option definitions of a file.
 *
 * Each option is a table [options.NAME] with type ("bool", "tristate", "int" or "string"),
 * default, and optionally help, depends-on (names of bool or tristate options) and, for int,
 * range = [minimum, maximum]. Names are upper case letters, digits and underscores, starting with
 * a letter. Unknown keys, wrong types, unknown or cyclic dependencies and defaults out of range
 * are errors.
 *
 * @param[out] aOut_pOptions     The options; deinitialize with OpenKBuild_Options_Deinit().
 * @param[in]  aIn_pPath         The file.
 * @param[out] aOut_pDiagnostic  The reason, when it fails.
 * @return true when the file was read and every option is valid.
 *
 * @post On failure *aOut_pOptions holds nothing that needs to be released.
 */
bool
OpenKBuild_Options_Init(OpenKBuild_tOptions*    aOut_pOptions,
                        const char*             aIn_pPath,
                        OpenKBuild_tDiagnostic* aOut_pDiagnostic);

/**
 * @brief Reads and checks option definitions from text, as OpenKBuild_Options_Init() does.
 *
 * @param[out] aOut_pOptions     The options; deinitialize with OpenKBuild_Options_Deinit().
 * @param[in]  aIn_pText         The TOML text, ending in a null character.
 * @param[in]  aIn_pName         The name given to the text in diagnostics.
 * @param[out] aOut_pDiagnostic  The reason, when it fails.
 * @return true when every option is valid.
 */
bool
OpenKBuild_Options_InitFromText(OpenKBuild_tOptions*    aOut_pOptions,
                                const char*             aIn_pText,
                                const char*             aIn_pName,
                                OpenKBuild_tDiagnostic* aOut_pDiagnostic);

/**
 * @brief Releases what the options hold, but not the structure itself.
 *
 * @param[in,out] aInOut_pOptions The options.
 */
void
OpenKBuild_Options_Deinit(OpenKBuild_tOptions* aInOut_pOptions);

/**
 * @brief Finds an option by its name.
 *
 * @param[in]  aIn_pOptions The options.
 * @param[in]  aIn_pName    The name.
 * @param[out] aOut_pIndex  The index of the option, when it exists.
 * @return true when the option exists.
 */
bool
OpenKBuild_Options_Find(const OpenKBuild_tOptions* aIn_pOptions,
                        const char*                aIn_pName,
                        size_t*                    aOut_pIndex);

/**
 * @brief Reads a TOML value as the value of an option of a type, the default or a configured one.
 *
 * A bool is a TOML boolean, a tristate the string "y", "m" or "n", an int a TOML integer, and a
 * string a TOML string without control characters, since config.h and config.mk keep it on one
 * line.
 *
 * @param[in]  aIn_pDatum       The TOML value.
 * @param[in]  aType            The type of the option.
 * @param[in]  aIn_pPath        The file, for the diagnostic.
 * @param[in]  aIn_pName        The option, for the diagnostic.
 * @param[out] aOut_pValue      The value.
 * @param[out] aOut_pDiagnostic The reason, when it fails.
 * @return true when the value fits the type.
 */
bool
OpenKBuild_Options_ReadValue(const toml_datum_t*     aIn_pDatum,
                             OpenKBuild_tOptionType  aType,
                             const char*             aIn_pPath,
                             const char*             aIn_pName,
                             OpenKBuild_tValue*      aOut_pValue,
                             OpenKBuild_tDiagnostic* aOut_pDiagnostic);

/**
 * @brief Names a type as OpenKBuild.conf.toml writes it.
 *
 * @param[in] aType The type.
 * @return "bool", "tristate", "int" or "string".
 */
const char*
OpenKBuild_Options_TypeName(OpenKBuild_tOptionType aType);

/**
 * @brief Writes a diagnostic for a place in a file.
 *
 * @param[out] aOut_pDiagnostic The diagnostic.
 * @param[in]  aIn_pPath        The file.
 * @param[in]  aLine            The line, 1-based; 0 leaves out the line and the column.
 * @param[in]  aColumn          The column, 1-based.
 * @param[in]  aIn_pFormat      A printf format, then its arguments.
 */
__attribute__((format(printf, 5, 6))) void
OpenKBuild_Diagnose(OpenKBuild_tDiagnostic* aOut_pDiagnostic,
                    const char*             aIn_pPath,
                    int                     aLine,
                    int                     aColumn,
                    const char*             aIn_pFormat,
                    ...);

#ifdef __cplusplus
}
#endif
