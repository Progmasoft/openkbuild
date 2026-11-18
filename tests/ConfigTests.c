/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <criterion/criterion.h>

#include "openkbuild/OpenKBuild.Config.h"
#include "openkbuild/OpenKBuild.Options.h"
#include "tests/Tests.h"

static const char* const kDefinitions = "[options.SERIAL]\ntype = \"bool\"\ndefault = true\n"
                                        "[options.BAUD]\ntype = \"int\"\ndefault = 115200\n"
                                        "range = [9600, 115200]\ndepends-on = [\"SERIAL\"]\n"
                                        "[options.NET]\ntype = \"tristate\"\ndefault = \"y\"\n"
                                        "[options.WIFI]\ntype = \"tristate\"\ndefault = \"y\"\n"
                                        "depends-on = [\"NET\"]\n"
                                        "[options.NAME]\ntype = \"string\"\ndefault = \"x\"\n";

/**
 * @brief Resolves a configuration that must be valid, with its notes captured.
 *
 * @param[out] aOut_pOptions The options of kDefinitions.
 * @param[out] aOut_pConfig  The configuration.
 * @param[in]  aIn_pText     The configured values.
 * @param[out] aOut_pNotes   The notes.
 */
static void
Resolve(OpenKBuild_tOptions* aOut_pOptions,
        OpenKBuild_tConfig*  aOut_pConfig,
        const char*          aIn_pText,
        Tests_tCapture*      aOut_pNotes)
{
        OpenKBuild_tDiagnostic diagnostic;
        bool                   isResolved;

        Tests_LoadOptions(aOut_pOptions, kDefinitions);
        Tests_Capture_Open(aOut_pNotes);
        isResolved = OpenKBuild_Config_InitFromText(aOut_pConfig,
                                                    aOut_pOptions,
                                                    aIn_pText,
                                                    "config.toml",
                                                    aOut_pNotes->pFile,
                                                    &diagnostic);
        Tests_Capture_Close(aOut_pNotes);
        cr_assert(isResolved, "%s", diagnostic.text);
}

/**
 * @brief Releases what Resolve() made.
 *
 * @param[in,out] aInOut_pOptions The options.
 * @param[in,out] aInOut_pConfig  The configuration.
 * @param[in,out] aInOut_pNotes   The notes.
 */
static void
Release(OpenKBuild_tOptions* aInOut_pOptions,
        OpenKBuild_tConfig*  aInOut_pConfig,
        Tests_tCapture*      aInOut_pNotes)
{
        OpenKBuild_Config_Deinit(aInOut_pConfig);
        OpenKBuild_Options_Deinit(aInOut_pOptions);
        Tests_Capture_Free(aInOut_pNotes);
}

Test(Config, TakesTheDefaults)
{
        OpenKBuild_tOptions options;
        OpenKBuild_tConfig  config;
        Tests_tCapture      notes;

        Resolve(&options, &config, "", &notes);
        cr_assert(config.pIsSet[ 0 ]);
        cr_assert_eq(config.pValues[ 1 ].number, 115200);
        cr_assert_eq(config.pValues[ 3 ].state, OpenKBuild_kYes);
        cr_assert_str_eq(config.pValues[ 4 ].pText, "x");
        cr_assert_str_eq(notes.pText, "");
        Release(&options, &config, &notes);
}

Test(Config, TakesTheConfiguredValues)
{
        OpenKBuild_tOptions options;
        OpenKBuild_tConfig  config;
        Tests_tCapture      notes;

        Resolve(&options,
                &config,
                "BAUD = 9600\nNET = \"m\"\nWIFI = \"m\"\nNAME = \"y\"\n",
                &notes);
        cr_assert_eq(config.pValues[ 1 ].number, 9600);
        cr_assert_eq(config.pValues[ 2 ].state, OpenKBuild_kModule);
        cr_assert_eq(config.pValues[ 3 ].state, OpenKBuild_kModule);
        cr_assert_str_eq(config.pValues[ 4 ].pText, "y");
        cr_assert_str_eq(notes.pText, "");
        Release(&options, &config, &notes);
}

Test(Config, LeavesOutWhatAnOffDependencyTurnsOff)
{
        OpenKBuild_tOptions options;
        OpenKBuild_tConfig  config;
        Tests_tCapture      notes;

        Resolve(&options, &config, "SERIAL = false\nNET = \"n\"\n", &notes);
        cr_assert_not(config.pIsSet[ 0 ]);
        cr_assert_not(config.pIsSet[ 1 ]);
        cr_assert_not(config.pIsSet[ 2 ]);
        cr_assert_not(config.pIsSet[ 3 ]);
        cr_assert(strstr(notes.pText, "BAUD is not set: it depends on SERIAL, which is n") != NULL,
                  "%s",
                  notes.pText);
        cr_assert(strstr(notes.pText, "WIFI is not set: it depends on NET, which is n") != NULL,
                  "%s",
                  notes.pText);
        Release(&options, &config, &notes);
}

Test(Config, LowersATristateToItsDependency)
{
        OpenKBuild_tOptions options;
        OpenKBuild_tConfig  config;
        Tests_tCapture      notes;

        Resolve(&options, &config, "NET = \"m\"\n", &notes);
        cr_assert_eq(config.pValues[ 3 ].state, OpenKBuild_kModule);
        cr_assert(config.pIsSet[ 3 ]);
        cr_assert(strstr(notes.pText, "WIFI is m, not y: it depends on NET, which is m") != NULL,
                  "%s",
                  notes.pText);
        Release(&options, &config, &notes);
}

/** A configuration that must be refused, and a part of the diagnostic it must give. */
typedef struct Refusal
{
        const char* pText;
        const char* pDiagnostic;
} tRefusal;

static const tRefusal kRefusals[] = {
        { "COLOR = true\n", "config.toml:1:9: COLOR is no option of test.toml" },
        { "SERIAL = \"y\"\n", "takes true or false" },
        { "BAUD = 300\n", "300 is outside [9600, 115200]" },
        { "NET = true\n", "takes \"y\", \"m\" or \"n\"" },
        { "NAME = 1\n", "takes a string" },
        { "SERIAL = \n", "config.toml" },
};

Test(Config, RefusesInvalidValues)
{
        OpenKBuild_tOptions options;

        Tests_LoadOptions(&options, kDefinitions);
        for(size_t index = 0; index < sizeof(kRefusals) / sizeof(kRefusals[ 0 ]); index++)
        {
                OpenKBuild_tConfig     config;
                OpenKBuild_tDiagnostic diagnostic;

                cr_expect_not(OpenKBuild_Config_InitFromText(&config,
                                                             &options,
                                                             kRefusals[ index ].pText,
                                                             "config.toml",
                                                             NULL,
                                                             &diagnostic),
                              "%s was accepted",
                              kRefusals[ index ].pText);
                cr_expect(strstr(diagnostic.text, kRefusals[ index ].pDiagnostic) != NULL,
                          "%s\ngave: %s",
                          kRefusals[ index ].pText,
                          diagnostic.text);
                cr_expect_null(config.pValues);
        }
        OpenKBuild_Options_Deinit(&options);
}

Test(Config, UsesTheDefaultsWithoutAFile)
{
        OpenKBuild_tOptions    options;
        OpenKBuild_tConfig     config;
        OpenKBuild_tDiagnostic diagnostic;

        Tests_LoadOptions(&options, kDefinitions);
        cr_assert(OpenKBuild_Config_Init(&config, &options, NULL, NULL, &diagnostic),
                  "%s",
                  diagnostic.text);
        cr_assert_eq(config.pValues[ 1 ].number, 115200);
        cr_assert_str_eq(config.pPath, "test.toml");
        OpenKBuild_Config_Deinit(&config);
        OpenKBuild_Options_Deinit(&options);
}
