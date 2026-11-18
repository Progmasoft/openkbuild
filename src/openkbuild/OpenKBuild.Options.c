/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openkbuild/OpenKBuild.Options.h"
#include "tomlc17.h"

/** The marks of the depth-first walk that orders the options by their dependencies. */
enum
{
        kUnvisited,
        kVisiting,
        kVisited,
};

/**
 * @brief Writes a diagnostic for a place in a file.
 *
 * @param[out] aOut_pDiagnostic The diagnostic.
 * @param[in]  aIn_pPath        The file.
 * @param[in]  aLine            The line, 1-based; 0 leaves out the line and the column.
 * @param[in]  aColumn          The column, 1-based.
 * @param[in]  aIn_pFormat      A printf format, then its arguments.
 */
void
OpenKBuild_Diagnose(OpenKBuild_tDiagnostic* aOut_pDiagnostic,
                    const char*             aIn_pPath,
                    int                     aLine,
                    int                     aColumn,
                    const char*             aIn_pFormat,
                    ...)
{
        int     used = 0;
        va_list arguments;

        if(aLine > 0)
                used = snprintf(aOut_pDiagnostic->text,
                                sizeof(aOut_pDiagnostic->text),
                                "%s:%d:%d: ",
                                aIn_pPath,
                                aLine,
                                aColumn);
        else
                used = snprintf(aOut_pDiagnostic->text,
                                sizeof(aOut_pDiagnostic->text),
                                "%s: ",
                                aIn_pPath);
        if(used < 0 || (size_t) used >= sizeof(aOut_pDiagnostic->text))
                return;
        va_start(arguments, aIn_pFormat);
        vsnprintf(aOut_pDiagnostic->text + used,
                  sizeof(aOut_pDiagnostic->text) - (size_t) used,
                  aIn_pFormat,
                  arguments);
        va_end(arguments);
}

/**
 * @brief Names a type as OpenKBuild.conf.toml writes it.
 *
 * @param[in] aType The type.
 * @return "bool", "tristate", "int" or "string".
 */
const char*
OpenKBuild_Options_TypeName(OpenKBuild_tOptionType aType)
{
        switch(aType)
        {
        case OpenKBuild_kBool:
                return ("bool");
        case OpenKBuild_kTristate:
                return ("tristate");
        case OpenKBuild_kInt:
                return ("int");
        case OpenKBuild_kString:
                return ("string");
        }
        return ("unknown");
}

/**
 * @brief Tells whether a name may name an option: an upper case letter, then upper case letters,
 *        digits and underscores.
 *
 * @param[in] aIn_pName The name.
 * @return true when it may.
 */
static bool
IsOptionName(const char* aIn_pName)
{
        if(aIn_pName[ 0 ] < 'A' || aIn_pName[ 0 ] > 'Z')
                return (false);
        for(const char* pCharacter = aIn_pName + 1; *pCharacter != '\0'; pCharacter++)
        {
                char character = *pCharacter;

                if((character < 'A' || character > 'Z') && (character < '0' || character > '9')
                   && character != '_')
                        return (false);
        }
        return (true);
}

/**
 * @brief Reads the type of an option from its type key.
 *
 * @param[in]  aIn_pText  The text of the key.
 * @param[out] aOut_pType The type.
 * @return true when the text names a type.
 */
static bool
ParseType(const char* aIn_pText, OpenKBuild_tOptionType* aOut_pType)
{
        static const struct
        {
                const char*            pName;
                OpenKBuild_tOptionType type;
        } kTypes[] = {
                { "bool", OpenKBuild_kBool },
                { "tristate", OpenKBuild_kTristate },
                { "int", OpenKBuild_kInt },
                { "string", OpenKBuild_kString },
        };

        for(size_t index = 0; index < sizeof(kTypes) / sizeof(kTypes[ 0 ]); index++)
        {
                if(strcmp(aIn_pText, kTypes[ index ].pName) == 0)
                {
                        *aOut_pType = kTypes[ index ].type;
                        return (true);
                }
        }
        return (false);
}

/**
 * @brief Reads a tristate value, "y", "m" or "n".
 *
 * @param[in]  aIn_pText   The text.
 * @param[out] aOut_pState The value.
 * @return true when the text is one of the three.
 */
static bool
ParseState(const char* aIn_pText, OpenKBuild_tState* aOut_pState)
{
        if(strcmp(aIn_pText, "y") == 0)
                *aOut_pState = OpenKBuild_kYes;
        else if(strcmp(aIn_pText, "m") == 0)
                *aOut_pState = OpenKBuild_kModule;
        else if(strcmp(aIn_pText, "n") == 0)
                *aOut_pState = OpenKBuild_kNo;
        else
                return (false);
        return (true);
}

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
                             OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        static const char* const kExpected[] = {
                [OpenKBuild_kBool]     = "true or false",
                [OpenKBuild_kTristate] = "\"y\", \"m\" or \"n\"",
                [OpenKBuild_kInt]      = "an integer",
                [OpenKBuild_kString]   = "a string",
        };
        bool isValid = false;

        memset(aOut_pValue, 0, sizeof(*aOut_pValue));
        switch(aType)
        {
        case OpenKBuild_kBool:
                if(aIn_pDatum->type == TOML_BOOLEAN)
                {
                        aOut_pValue->state = aIn_pDatum->u.boolean ? OpenKBuild_kYes
                                                                   : OpenKBuild_kNo;
                        isValid            = true;
                }
                break;
        case OpenKBuild_kTristate:
                isValid = aIn_pDatum->type == TOML_STRING
                        && ParseState(aIn_pDatum->u.s, &aOut_pValue->state);
                break;
        case OpenKBuild_kInt:
                if(aIn_pDatum->type == TOML_INT64)
                {
                        aOut_pValue->number = aIn_pDatum->u.int64;
                        isValid             = true;
                }
                break;
        case OpenKBuild_kString:
                if(aIn_pDatum->type == TOML_STRING)
                {
                        for(const char* pCharacter = aIn_pDatum->u.s; *pCharacter != '\0';
                            pCharacter++)
                        {
                                unsigned char character = (unsigned char) *pCharacter;

                                if(character < 0x20 || character == 0x7F)
                                {
                                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                                            aIn_pPath,
                                                            aIn_pDatum->lineno,
                                                            aIn_pDatum->colno,
                                                            "%s: a string option holds no control "
                                                            "characters",
                                                            aIn_pName);
                                        return (false);
                                }
                        }
                        aOut_pValue->pText = aIn_pDatum->u.s;
                        isValid            = true;
                }
                break;
        }
        if(!isValid)
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aIn_pPath,
                                    aIn_pDatum->lineno,
                                    aIn_pDatum->colno,
                                    "%s: a %s option takes %s",
                                    aIn_pName,
                                    OpenKBuild_Options_TypeName(aType),
                                    kExpected[ aType ]);
        return (isValid);
}

/**
 * @brief Reads one [options.NAME] table, except its dependencies, which need every name first.
 *
 * @param[in]  aIn_pOptions     The options, for the path.
 * @param[in]  aIn_pName        The name of the option.
 * @param[in]  aIn_pTable       The table.
 * @param[out] aOut_pOption     The option.
 * @param[out] aOut_pDiagnostic The reason, when it fails.
 * @return true when the table is a valid option.
 */
static bool
ReadOption(const OpenKBuild_tOptions* aIn_pOptions,
           const char*                aIn_pName,
           const toml_datum_t*        aIn_pTable,
           OpenKBuild_tOption*        aOut_pOption,
           OpenKBuild_tDiagnostic*    aOut_pDiagnostic)
{
        const char* pPath = aIn_pOptions->pPath;

        aOut_pOption->pName = aIn_pName;
        aOut_pOption->line  = aIn_pTable->lineno;
        if(!IsOptionName(aIn_pName))
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    pPath,
                                    aIn_pTable->lineno,
                                    aIn_pTable->colno,
                                    "%s: an option name is upper case letters, digits and "
                                    "underscores, starting with a letter",
                                    aIn_pName);
                return (false);
        }
        if(aIn_pTable->type != TOML_TABLE)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    pPath,
                                    aIn_pTable->lineno,
                                    aIn_pTable->colno,
                                    "options.%s must be a table",
                                    aIn_pName);
                return (false);
        }
        for(int32_t index = 0; index < aIn_pTable->u.tab.size; index++)
        {
                const char* pKey = aIn_pTable->u.tab.key[ index ];

                if(strcmp(pKey, "type") != 0 && strcmp(pKey, "default") != 0
                   && strcmp(pKey, "help") != 0 && strcmp(pKey, "depends-on") != 0
                   && strcmp(pKey, "range") != 0)
                {
                        const toml_datum_t* pValue = &aIn_pTable->u.tab.value[ index ];

                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            pPath,
                                            pValue->lineno,
                                            pValue->colno,
                                            "%s: unknown key %s; the keys are type, default, help, "
                                            "depends-on and range",
                                            aIn_pName,
                                            pKey);
                        return (false);
                }
        }
        toml_datum_t type = toml_get(*aIn_pTable, "type");

        if(type.type != TOML_STRING || !ParseType(type.u.s, &aOut_pOption->type))
        {
                OpenKBuild_Diagnose(
                        aOut_pDiagnostic,
                        pPath,
                        type.type == TOML_UNKNOWN ? aIn_pTable->lineno : type.lineno,
                        type.type == TOML_UNKNOWN ? aIn_pTable->colno : type.colno,
                        "%s: type must be \"bool\", \"tristate\", \"int\" or \"string\"",
                        aIn_pName);
                return (false);
        }

        toml_datum_t help = toml_get(*aIn_pTable, "help");

        if(help.type == TOML_STRING)
                aOut_pOption->pHelp = help.u.s;
        else if(help.type != TOML_UNKNOWN)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    pPath,
                                    help.lineno,
                                    help.colno,
                                    "%s: help must be a string",
                                    aIn_pName);
                return (false);
        }

        toml_datum_t range = toml_get(*aIn_pTable, "range");

        if(range.type != TOML_UNKNOWN)
        {
                if(aOut_pOption->type != OpenKBuild_kInt || range.type != TOML_ARRAY
                   || range.u.arr.size != 2 || range.u.arr.elem[ 0 ].type != TOML_INT64
                   || range.u.arr.elem[ 1 ].type != TOML_INT64
                   || range.u.arr.elem[ 0 ].u.int64 > range.u.arr.elem[ 1 ].u.int64)
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            pPath,
                                            range.lineno,
                                            range.colno,
                                            "%s: range is [minimum, maximum], for an int option",
                                            aIn_pName);
                        return (false);
                }
                aOut_pOption->hasRange = true;
                aOut_pOption->minimum  = range.u.arr.elem[ 0 ].u.int64;
                aOut_pOption->maximum  = range.u.arr.elem[ 1 ].u.int64;
        }

        toml_datum_t defaultValue = toml_get(*aIn_pTable, "default");

        if(defaultValue.type == TOML_UNKNOWN)
        {
                /* No default: off, zero or empty. */
                memset(&aOut_pOption->defaultValue, 0, sizeof(aOut_pOption->defaultValue));
                aOut_pOption->defaultValue.pText = "";
        } else if(!OpenKBuild_Options_ReadValue(&defaultValue,
                                                aOut_pOption->type,
                                                pPath,
                                                aIn_pName,
                                                &aOut_pOption->defaultValue,
                                                aOut_pDiagnostic))
                return (false);
        if(aOut_pOption->hasRange
           && (aOut_pOption->defaultValue.number < aOut_pOption->minimum
               || aOut_pOption->defaultValue.number > aOut_pOption->maximum))
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    pPath,
                                    aIn_pTable->lineno,
                                    aIn_pTable->colno,
                                    "%s: the default %lld is outside [%lld, %lld]",
                                    aIn_pName,
                                    (long long) aOut_pOption->defaultValue.number,
                                    (long long) aOut_pOption->minimum,
                                    (long long) aOut_pOption->maximum);
                return (false);
        }
        return (true);
}

/**
 * @brief Reads the depends-on list of an option, once every option has its name and type.
 *
 * @param[in,out] aInOut_pOptions  The options.
 * @param[in]     aIndex           The option.
 * @param[in]     aIn_pTable       Its table.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return true when every dependency names a bool or tristate option other than itself.
 */
static bool
ReadDependencies(OpenKBuild_tOptions*    aInOut_pOptions,
                 size_t                  aIndex,
                 const toml_datum_t*     aIn_pTable,
                 OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        OpenKBuild_tOption* pOption   = &aInOut_pOptions->pItems[ aIndex ];
        toml_datum_t        dependsOn = toml_get(*aIn_pTable, "depends-on");

        if(dependsOn.type == TOML_UNKNOWN)
                return (true);
        if(dependsOn.type != TOML_ARRAY)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    dependsOn.lineno,
                                    dependsOn.colno,
                                    "%s: depends-on is a list of option names",
                                    pOption->pName);
                return (false);
        }
        if(dependsOn.u.arr.size == 0)
                return (true);
        pOption->pDependsOn = calloc((size_t) dependsOn.u.arr.size, sizeof(size_t));
        if(pOption->pDependsOn == NULL)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    0,
                                    0,
                                    "out of memory");
                return (false);
        }
        for(int32_t index = 0; index < dependsOn.u.arr.size; index++)
        {
                const toml_datum_t* pName      = &dependsOn.u.arr.elem[ index ];
                size_t              dependency = 0;

                if(pName->type != TOML_STRING
                   || !OpenKBuild_Options_Find(aInOut_pOptions, pName->u.s, &dependency))
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aInOut_pOptions->pPath,
                                            pName->lineno,
                                            pName->colno,
                                            "%s: depends-on names %s, which is no option",
                                            pOption->pName,
                                            pName->type == TOML_STRING ? pName->u.s : "a value");
                        return (false);
                }
                if(dependency == aIndex
                   || (aInOut_pOptions->pItems[ dependency ].type != OpenKBuild_kBool
                       && aInOut_pOptions->pItems[ dependency ].type != OpenKBuild_kTristate))
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aInOut_pOptions->pPath,
                                            pName->lineno,
                                            pName->colno,
                                            "%s: depends-on takes other bool or tristate options",
                                            pOption->pName);
                        return (false);
                }
                pOption->pDependsOn[ pOption->dependsOnCount++ ] = dependency;
        }
        return (true);
}

/**
 * @brief Puts an option and, before it, everything it depends on into the order.
 *
 * @param[in,out] aInOut_pOptions  The options.
 * @param[in]     aIndex           The option.
 * @param[in,out] aInOut_pMarks    The mark of each option.
 * @param[in,out] aInOut_pCount    How many options the order holds.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return false when the dependencies form a cycle.
 */
static bool
Visit(OpenKBuild_tOptions*    aInOut_pOptions,
      size_t                  aIndex,
      unsigned char*          aInOut_pMarks,
      size_t*                 aInOut_pCount,
      OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        const OpenKBuild_tOption* pOption = &aInOut_pOptions->pItems[ aIndex ];

        if(aInOut_pMarks[ aIndex ] == kVisited)
                return (true);
        if(aInOut_pMarks[ aIndex ] == kVisiting)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    pOption->line,
                                    1,
                                    "%s: its dependencies lead back to it",
                                    pOption->pName);
                return (false);
        }
        aInOut_pMarks[ aIndex ] = kVisiting;
        for(size_t index = 0; index < pOption->dependsOnCount; index++)
        {
                if(!Visit(aInOut_pOptions,
                          pOption->pDependsOn[ index ],
                          aInOut_pMarks,
                          aInOut_pCount,
                          aOut_pDiagnostic))
                        return (false);
        }
        aInOut_pMarks[ aIndex ]                       = kVisited;
        aInOut_pOptions->pOrder[ (*aInOut_pCount)++ ] = aIndex;
        return (true);
}

/**
 * @brief Reads every option of a parsed document.
 *
 * @param[in,out] aInOut_pOptions  The options, with the parsed document.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return true when every option is valid.
 */
static bool
ReadDocument(OpenKBuild_tOptions* aInOut_pOptions, OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        const toml_datum_t* pTop = &aInOut_pOptions->document.toptab;

        for(int32_t index = 0; index < pTop->u.tab.size; index++)
        {
                if(strcmp(pTop->u.tab.key[ index ], "options") != 0)
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            aInOut_pOptions->pPath,
                                            pTop->u.tab.value[ index ].lineno,
                                            pTop->u.tab.value[ index ].colno,
                                            "unknown key %s; options are [options.NAME] tables",
                                            pTop->u.tab.key[ index ]);
                        return (false);
                }
        }

        toml_datum_t options = toml_get(*pTop, "options");

        if(options.type == TOML_UNKNOWN)
                return (true);
        if(options.type != TOML_TABLE)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    options.lineno,
                                    options.colno,
                                    "options must be a table of [options.NAME] tables");
                return (false);
        }
        if(options.u.tab.size == 0)
                return (true);
        aInOut_pOptions->pItems = calloc((size_t) options.u.tab.size, sizeof(OpenKBuild_tOption));
        aInOut_pOptions->pOrder = calloc((size_t) options.u.tab.size, sizeof(size_t));
        if(aInOut_pOptions->pItems == NULL || aInOut_pOptions->pOrder == NULL)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    0,
                                    0,
                                    "out of memory");
                return (false);
        }
        for(int32_t index = 0; index < options.u.tab.size; index++)
        {
                if(!ReadOption(aInOut_pOptions,
                               options.u.tab.key[ index ],
                               &options.u.tab.value[ index ],
                               &aInOut_pOptions->pItems[ index ],
                               aOut_pDiagnostic))
                        return (false);
                aInOut_pOptions->count++;
        }
        for(size_t index = 0; index < aInOut_pOptions->count; index++)
        {
                if(!ReadDependencies(aInOut_pOptions,
                                     index,
                                     &options.u.tab.value[ index ],
                                     aOut_pDiagnostic))
                        return (false);
        }

        unsigned char* pMarks = calloc(aInOut_pOptions->count, 1);
        size_t         count  = 0;
        bool           isOk   = pMarks != NULL;

        if(!isOk)
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    0,
                                    0,
                                    "out of memory");
        for(size_t index = 0; isOk && index < aInOut_pOptions->count; index++)
                isOk = Visit(aInOut_pOptions, index, pMarks, &count, aOut_pDiagnostic);
        free(pMarks);
        return (isOk);
}

/**
 * @brief Checks a parsed document and reads its options, or releases it.
 *
 * @param[in,out] aInOut_pOptions  The options, with the result of the parser.
 * @param[out]    aOut_pDiagnostic The reason, when it fails.
 * @return true when the document parsed and every option is valid.
 */
static bool
Finish(OpenKBuild_tOptions* aInOut_pOptions, OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        if(!aInOut_pOptions->document.ok)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    aInOut_pOptions->pPath,
                                    0,
                                    0,
                                    "%s",
                                    aInOut_pOptions->document.errmsg);
                toml_free(aInOut_pOptions->document);
                memset(aInOut_pOptions, 0, sizeof(*aInOut_pOptions));
                return (false);
        }
        if(!ReadDocument(aInOut_pOptions, aOut_pDiagnostic))
        {
                OpenKBuild_Options_Deinit(aInOut_pOptions);
                return (false);
        }
        return (true);
}

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
                        OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        FILE* pFile = fopen(aIn_pPath, "r");

        memset(aOut_pOptions, 0, sizeof(*aOut_pOptions));
        if(pFile == NULL)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic, aIn_pPath, 0, 0, "%s", strerror(errno));
                return (false);
        }
        aOut_pOptions->pPath    = aIn_pPath;
        aOut_pOptions->document = toml_parse_file_named(pFile, aIn_pPath);
        fclose(pFile);
        return (Finish(aOut_pOptions, aOut_pDiagnostic));
}

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
                                OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        memset(aOut_pOptions, 0, sizeof(*aOut_pOptions));
        aOut_pOptions->pPath    = aIn_pName;
        aOut_pOptions->document = toml_parse_named(aIn_pText, (int) strlen(aIn_pText), aIn_pName);
        return (Finish(aOut_pOptions, aOut_pDiagnostic));
}

/**
 * @brief Releases what the options hold, but not the structure itself.
 *
 * @param[in,out] aInOut_pOptions The options.
 */
void
OpenKBuild_Options_Deinit(OpenKBuild_tOptions* aInOut_pOptions)
{
        if(aInOut_pOptions->pItems != NULL)
        {
                for(size_t index = 0; index < aInOut_pOptions->count; index++)
                        free(aInOut_pOptions->pItems[ index ].pDependsOn);
        }
        free(aInOut_pOptions->pItems);
        free(aInOut_pOptions->pOrder);
        if(aInOut_pOptions->document.ok)
                toml_free(aInOut_pOptions->document);
        memset(aInOut_pOptions, 0, sizeof(*aInOut_pOptions));
}

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
                        size_t*                    aOut_pIndex)
{
        for(size_t index = 0; index < aIn_pOptions->count; index++)
        {
                if(strcmp(aIn_pOptions->pItems[ index ].pName, aIn_pName) == 0)
                {
                        *aOut_pIndex = index;
                        return (true);
                }
        }
        return (false);
}
