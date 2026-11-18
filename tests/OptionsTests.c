/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <criterion/criterion.h>

#include "openkbuild/OpenKBuild.Options.h"
#include "tests/Tests.h"

static const char* const kDefinitions
        = "[options.SERIAL]\n"
          "type = \"bool\"\n"
          "default = true\n"
          "help = \"Write kernel messages to the first serial port.\"\n"
          "\n"
          "[options.SERIAL_BAUD]\n"
          "type = \"int\"\n"
          "default = 115200\n"
          "range = [9600, 115200]\n"
          "depends-on = [\"SERIAL\"]\n"
          "\n"
          "[options.NET]\n"
          "type = \"tristate\"\n"
          "default = \"m\"\n"
          "\n"
          "[options.LOCALVERSION]\n"
          "type = \"string\"\n"
          "default = \"-dev\"\n";

Test(Options, ReadsEachTypeInFileOrder)
{
        OpenKBuild_tOptions options;

        Tests_LoadOptions(&options, kDefinitions);
        cr_assert_eq(options.count, 4);
        cr_assert_str_eq(options.pItems[ 0 ].pName, "SERIAL");
        cr_assert_eq(options.pItems[ 0 ].type, OpenKBuild_kBool);
        cr_assert_eq(options.pItems[ 0 ].defaultValue.state, OpenKBuild_kYes);
        cr_assert_str_eq(options.pItems[ 0 ].pHelp,
                         "Write kernel messages to the first serial port.");
        cr_assert_eq(options.pItems[ 1 ].type, OpenKBuild_kInt);
        cr_assert_eq(options.pItems[ 1 ].defaultValue.number, 115200);
        cr_assert(options.pItems[ 1 ].hasRange);
        cr_assert_eq(options.pItems[ 1 ].minimum, 9600);
        cr_assert_eq(options.pItems[ 1 ].dependsOnCount, 1);
        cr_assert_eq(options.pItems[ 1 ].pDependsOn[ 0 ], 0);
        cr_assert_eq(options.pItems[ 2 ].defaultValue.state, OpenKBuild_kModule);
        cr_assert_str_eq(options.pItems[ 3 ].defaultValue.pText, "-dev");
        OpenKBuild_Options_Deinit(&options);
}

Test(Options, OrdersEachOptionAfterItsDependencies)
{
        OpenKBuild_tOptions options;

        Tests_LoadOptions(&options,
                          "[options.C]\ntype = \"bool\"\ndepends-on = [\"B\"]\n"
                          "[options.B]\ntype = \"bool\"\ndepends-on = [\"A\"]\n"
                          "[options.A]\ntype = \"bool\"\n");
        cr_assert_eq(options.pOrder[ 0 ], 2);
        cr_assert_eq(options.pOrder[ 1 ], 1);
        cr_assert_eq(options.pOrder[ 2 ], 0);
        OpenKBuild_Options_Deinit(&options);
}

Test(Options, GivesDefaultsWhenLeftOut)
{
        OpenKBuild_tOptions options;

        Tests_LoadOptions(&options,
                          "[options.A]\ntype = \"bool\"\n[options.B]\ntype = \"tristate\"\n"
                          "[options.C]\ntype = \"int\"\n[options.D]\ntype = \"string\"\n");
        cr_assert_eq(options.pItems[ 0 ].defaultValue.state, OpenKBuild_kNo);
        cr_assert_eq(options.pItems[ 1 ].defaultValue.state, OpenKBuild_kNo);
        cr_assert_eq(options.pItems[ 2 ].defaultValue.number, 0);
        cr_assert_str_eq(options.pItems[ 3 ].defaultValue.pText, "");
        OpenKBuild_Options_Deinit(&options);
}

Test(Options, AcceptsAFileWithoutOptions)
{
        OpenKBuild_tOptions options;

        Tests_LoadOptions(&options, "");
        cr_assert_eq(options.count, 0);
        OpenKBuild_Options_Deinit(&options);
}

Test(Options, FindsByName)
{
        OpenKBuild_tOptions options;
        size_t              index = 99;

        Tests_LoadOptions(&options, kDefinitions);
        cr_assert(OpenKBuild_Options_Find(&options, "NET", &index));
        cr_assert_eq(index, 2);
        cr_assert_not(OpenKBuild_Options_Find(&options, "NONE", &index));
        OpenKBuild_Options_Deinit(&options);
}

/** A definition that must be refused, and a part of the diagnostic it must give. */
typedef struct Refusal
{
        const char* pText;
        const char* pDiagnostic;
} tRefusal;

static const tRefusal kRefusals[] = {
        { "[options.a]\ntype = \"bool\"\n", "upper case letters" },
        { "[options.A]\ntype = \"float\"\n", "type must be" },
        { "[options.A]\ndefault = true\n", "type must be" },
        { "[options.A]\ntype = \"bool\"\nmenu = 1\n", "unknown key menu" },
        { "[options.A]\ntype = \"bool\"\ndefault = 1\n", "takes true or false" },
        { "[options.A]\ntype = \"tristate\"\ndefault = \"yes\"\n", "takes \"y\", \"m\" or \"n\"" },
        { "[options.A]\ntype = \"int\"\ndefault = \"1\"\n", "takes an integer" },
        { "[options.A]\ntype = \"string\"\ndefault = \"a\\tb\"\n", "no control characters" },
        { "[options.A]\ntype = \"int\"\ndefault = 5\nrange = [10, 20]\n", "outside [10, 20]" },
        { "[options.A]\ntype = \"int\"\nrange = [20, 10]\n", "range is [minimum, maximum]" },
        { "[options.A]\ntype = \"bool\"\nrange = [0, 1]\n", "range is [minimum, maximum]" },
        { "[options.A]\ntype = \"bool\"\ndepends-on = [\"B\"]\n", "names B, which is no option" },
        { "[options.A]\ntype = \"bool\"\ndepends-on = \"B\"\n", "a list of option names" },
        { "[options.A]\ntype = \"bool\"\ndepends-on = [\"A\"]\n", "other bool or tristate" },
        { "[options.N]\ntype = \"int\"\n[options.A]\ntype = \"bool\"\ndepends-on = [\"N\"]\n",
          "other bool or tristate" },
        { "[options.A]\ntype = \"bool\"\ndepends-on = [\"B\"]\n"
          "[options.B]\ntype = \"bool\"\ndepends-on = [\"A\"]\n",
          "lead back to it" },
        { "[options.A]\ntype = \"bool\"\nhelp = 1\n", "help must be a string" },
        { "name = 1\n", "unknown key name" },
        { "options = 1\n", "options must be a table" },
        { "[options.A]\ntype = \n", "test.toml" },
};

Test(Options, RefusesInvalidDefinitions)
{
        for(size_t index = 0; index < sizeof(kRefusals) / sizeof(kRefusals[ 0 ]); index++)
        {
                OpenKBuild_tDiagnostic diagnostic;

                Tests_RefuseOptions(kRefusals[ index ].pText, &diagnostic);
                cr_expect(strstr(diagnostic.text, kRefusals[ index ].pDiagnostic) != NULL,
                          "%s\ngave: %s",
                          kRefusals[ index ].pText,
                          diagnostic.text);
        }
}

Test(Options, PlacesTheDiagnostic)
{
        OpenKBuild_tDiagnostic diagnostic;

        Tests_RefuseOptions("[options.A]\ntype = \"bool\"\ndefault = 1\n", &diagnostic);
        cr_assert_str_eq(diagnostic.text, "test.toml:3:11: A: a bool option takes true or false");
}

Test(Options, ReportsAMissingFile)
{
        OpenKBuild_tOptions    options;
        OpenKBuild_tDiagnostic diagnostic;

        cr_assert_not(OpenKBuild_Options_Init(&options, "does-not-exist.toml", &diagnostic));
        cr_assert(strstr(diagnostic.text, "does-not-exist.toml: ") == diagnostic.text);
}
