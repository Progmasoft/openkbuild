/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openkbuild/OpenKBuild.Config.h"
#include "openkbuild/OpenKBuild.Options.h"
#include "tomlc17.h"

/**
 * @brief Names a tristate value as a configuration writes it.
 *
 * @param[in] aState The value.
 * @return "y", "m" or "n".
 */
static const char*
StateName(OpenKBuild_tState aState)
{
        switch(aState)
        {
        case OpenKBuild_kYes:
                return ("y");
        case OpenKBuild_kModule:
                return ("m");
        case OpenKBuild_kNo:
                return ("n");
        }
        return ("?");
}

/**
 * @brief Reads the configured values over the defaults.
 *
 * @param[in,out] aInOut_pConfig   The configuration, with the defaults and the parsed document.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return true when every value names an option and fits it.
 */
static bool
ReadValues(OpenKBuild_tConfig* aInOut_pConfig, OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        const toml_datum_t* pTop = &aInOut_pConfig->document.toptab;

        for(int32_t index = 0; index < pTop->u.tab.size; index++)
        {
                const char*               pName  = pTop->u.tab.key[ index ];
                const toml_datum_t*       pDatum = &pTop->u.tab.value[ index ];
                size_t                    option = 0;
                const OpenKBuild_tOption* pOption;

                if(!OpenKBuild_Options_Find(aInOut_pConfig->pOptions, pName, &option))
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aInOut_pConfig->pPath,
                                            pDatum->lineno,
                                            pDatum->colno,
                                            "%s is no option of %s",
                                            pName,
                                            aInOut_pConfig->pOptions->pPath);
                        return (false);
                }
                pOption = &aInOut_pConfig->pOptions->pItems[ option ];
                if(!OpenKBuild_Options_ReadValue(pDatum,
                                                 pOption->type,
                                                 aInOut_pConfig->pPath,
                                                 pName,
                                                 &aInOut_pConfig->pValues[ option ],
                                                 aOut_pDiagnostic))
                        return (false);
                if(pOption->hasRange
                   && (aInOut_pConfig->pValues[ option ].number < pOption->minimum
                       || aInOut_pConfig->pValues[ option ].number > pOption->maximum))
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aInOut_pConfig->pPath,
                                            pDatum->lineno,
                                            pDatum->colno,
                                            "%s: %lld is outside [%lld, %lld]",
                                            pName,
                                            (long long) aInOut_pConfig->pValues[ option ].number,
                                            (long long) pOption->minimum,
                                            (long long) pOption->maximum);
                        return (false);
                }
        }
        return (true);
}

/**
 * @brief Limits every value by its dependencies, in dependency order.
 *
 * @param[in,out] aInOut_pConfig The configuration, with the values read.
 * @param[in,out] aInOut_pNotes  Where to report lowered values, or NULL.
 */
static void
Resolve(OpenKBuild_tConfig* aInOut_pConfig, FILE* aInOut_pNotes)
{
        const OpenKBuild_tOptions* pOptions = aInOut_pConfig->pOptions;

        for(size_t position = 0; position < pOptions->count; position++)
        {
                size_t                    index   = pOptions->pOrder[ position ];
                const OpenKBuild_tOption* pOption = &pOptions->pItems[ index ];
                OpenKBuild_tValue*        pValue  = &aInOut_pConfig->pValues[ index ];
                OpenKBuild_tState         limit   = OpenKBuild_kYes;
                const char*               pOff    = NULL;

                for(size_t dependency = 0; dependency < pOption->dependsOnCount; dependency++)
                {
                        size_t            other = pOption->pDependsOn[ dependency ];
                        OpenKBuild_tState state = aInOut_pConfig->pIsSet[ other ]
                                ? aInOut_pConfig->pValues[ other ].state
                                : OpenKBuild_kNo;

                        if(state < limit)
                        {
                                limit = state;
                                pOff  = pOptions->pItems[ other ].pName;
                        }
                }
                if(limit == OpenKBuild_kNo)
                {
                        bool wasOn = pOption->type == OpenKBuild_kInt
                                || pOption->type == OpenKBuild_kString
                                || pValue->state != OpenKBuild_kNo;

                        if(wasOn && aInOut_pNotes != NULL && pOption->dependsOnCount > 0)
                                fprintf(aInOut_pNotes,
                                        "openkbuild: %s is not set: it depends on %s, which is n\n",
                                        pOption->pName,
                                        pOff);
                        aInOut_pConfig->pIsSet[ index ] = false;
                        continue;
                }
                if(pOption->type == OpenKBuild_kTristate && pValue->state > limit)
                {
                        if(aInOut_pNotes != NULL)
                                fprintf(aInOut_pNotes,
                                        "openkbuild: %s is %s, not %s: it depends on %s, which is "
                                        "%s\n",
                                        pOption->pName,
                                        StateName(limit),
                                        StateName(pValue->state),
                                        pOff,
                                        StateName(limit));
                        pValue->state = limit;
                }
                aInOut_pConfig->pIsSet[ index ] = (pOption->type != OpenKBuild_kBool
                                                   && pOption->type != OpenKBuild_kTristate)
                        || pValue->state != OpenKBuild_kNo;
        }
}

/**
 * @brief Allocates the values and fills them with the defaults.
 *
 * @param[out] aOut_pConfig     The configuration.
 * @param[in]  aIn_pOptions     The options.
 * @param[in]  aIn_pPath        The name of the configuration, for diagnostics.
 * @param[out] aOut_pDiagnostic The reason, when it fails.
 * @return false when memory runs out.
 */
static bool
Begin(OpenKBuild_tConfig*        aOut_pConfig,
      const OpenKBuild_tOptions* aIn_pOptions,
      const char*                aIn_pPath,
      OpenKBuild_tDiagnostic*    aOut_pDiagnostic)
{
        memset(aOut_pConfig, 0, sizeof(*aOut_pConfig));
        aOut_pConfig->pOptions = aIn_pOptions;
        aOut_pConfig->pPath    = aIn_pPath != NULL ? aIn_pPath : aIn_pOptions->pPath;
        if(aIn_pOptions->count == 0)
                return (true);
        aOut_pConfig->pValues = calloc(aIn_pOptions->count, sizeof(OpenKBuild_tValue));
        aOut_pConfig->pIsSet  = calloc(aIn_pOptions->count, sizeof(bool));
        if(aOut_pConfig->pValues == NULL || aOut_pConfig->pIsSet == NULL)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic, aOut_pConfig->pPath, 0, 0, "out of memory");
                OpenKBuild_Config_Deinit(aOut_pConfig);
                return (false);
        }
        for(size_t index = 0; index < aIn_pOptions->count; index++)
                aOut_pConfig->pValues[ index ] = aIn_pOptions->pItems[ index ].defaultValue;
        return (true);
}

/**
 * @brief Reads the values of a parsed document, if any, and resolves the dependencies.
 *
 * @param[in,out] aInOut_pConfig   The configuration, after Begin(), with its document.
 * @param[in]     aIsParsed        Whether a document was parsed.
 * @param[in,out] aInOut_pNotes    Where to report lowered values, or NULL.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return true when every value is valid.
 */
static bool
Finish(OpenKBuild_tConfig*     aInOut_pConfig,
       bool                    aIsParsed,
       FILE*                   aInOut_pNotes,
       OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        if(aIsParsed && !aInOut_pConfig->document.ok)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pConfig->pPath,
                                    0,
                                    0,
                                    "%s",
                                    aInOut_pConfig->document.errmsg);
                toml_free(aInOut_pConfig->document);
                memset(&aInOut_pConfig->document, 0, sizeof(aInOut_pConfig->document));
                OpenKBuild_Config_Deinit(aInOut_pConfig);
                return (false);
        }
        if(aIsParsed && !ReadValues(aInOut_pConfig, aOut_pDiagnostic))
        {
                OpenKBuild_Config_Deinit(aInOut_pConfig);
                return (false);
        }
        Resolve(aInOut_pConfig, aInOut_pNotes);
        return (true);
}

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
                       OpenKBuild_tDiagnostic*    aOut_pDiagnostic)
{
        if(!Begin(aOut_pConfig, aIn_pOptions, aIn_pPath, aOut_pDiagnostic))
                return (false);
        if(aIn_pPath != NULL)
        {
                FILE* pFile = fopen(aIn_pPath, "r");

                if(pFile == NULL)
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aIn_pPath,
                                            0,
                                            0,
                                            "%s",
                                            strerror(errno));
                        OpenKBuild_Config_Deinit(aOut_pConfig);
                        return (false);
                }
                aOut_pConfig->document = toml_parse_file_named(pFile, aIn_pPath);
                fclose(pFile);
        }
        return (Finish(aOut_pConfig, aIn_pPath != NULL, aInOut_pNotes, aOut_pDiagnostic));
}

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
                               OpenKBuild_tDiagnostic*    aOut_pDiagnostic)
{
        if(!Begin(aOut_pConfig, aIn_pOptions, aIn_pName, aOut_pDiagnostic))
                return (false);
        aOut_pConfig->document = toml_parse_named(aIn_pText, (int) strlen(aIn_pText), aIn_pName);
        return (Finish(aOut_pConfig, true, aInOut_pNotes, aOut_pDiagnostic));
}

/**
 * @brief Releases what the configuration holds, but not the structure itself.
 *
 * @param[in,out] aInOut_pConfig The configuration.
 */
void
OpenKBuild_Config_Deinit(OpenKBuild_tConfig* aInOut_pConfig)
{
        free(aInOut_pConfig->pValues);
        free(aInOut_pConfig->pIsSet);
        if(aInOut_pConfig->document.ok)
                toml_free(aInOut_pConfig->document);
        memset(aInOut_pConfig, 0, sizeof(*aInOut_pConfig));
}
