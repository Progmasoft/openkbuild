/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <criterion/criterion.h>

#include "openkbuild/OpenKBuild.Cli.h"
#include "openkbuild/OpenKBuild.Options.h"

/**
 * @brief Parses a command line that must be accepted.
 *
 * @param[in]  aArgumentCount    The number of arguments.
 * @param[in]  aIn_ppArguments   The arguments.
 * @param[out] aOut_pCommandLine The command line.
 */
static void
Accept(int aArgumentCount, char* const* aIn_ppArguments, OpenKBuild_tCommandLine* aOut_pCommandLine)
{
        OpenKBuild_tDiagnostic diagnostic;

        cr_assert(OpenKBuild_Cli_Parse(aArgumentCount,
                                       aIn_ppArguments,
                                       aOut_pCommandLine,
                                       &diagnostic),
                  "%s",
                  diagnostic.text);
}

/**
 * @brief Parses a command line that must be refused, and checks the diagnostic.
 *
 * @param[in] aArgumentCount  The number of arguments.
 * @param[in] aIn_ppArguments The arguments.
 * @param[in] aIn_pExpected   The diagnostic.
 */
static void
Refuse(int aArgumentCount, char* const* aIn_ppArguments, const char* aIn_pExpected)
{
        OpenKBuild_tCommandLine commandLine;
        OpenKBuild_tDiagnostic  diagnostic;

        cr_assert_not(
                OpenKBuild_Cli_Parse(aArgumentCount, aIn_ppArguments, &commandLine, &diagnostic));
        cr_assert_str_eq(diagnostic.text, aIn_pExpected);
}

Test(Cli, GivesTheDefaults)
{
        char* const             arguments[] = { "openkbuild", "generate" };
        OpenKBuild_tCommandLine commandLine;

        Accept(2, arguments, &commandLine);
        cr_assert_eq(commandLine.command, OpenKBuild_kGenerate);
        cr_assert_str_eq(commandLine.pOptionsPath, "OpenKBuild.conf.toml");
        cr_assert_str_eq(commandLine.pConfigPath, ".config.toml");
        cr_assert_not(commandLine.isConfigGiven);
        cr_assert_str_eq(commandLine.pOutputDirectory, "build");
}

Test(Cli, ReadsFlagsInBothForms)
{
        char* const arguments[] = { "openkbuild",      "generate",     "--options", "a.toml",
                                    "--config=b.toml", "--output-dir", "out" };
        OpenKBuild_tCommandLine commandLine;

        Accept(7, arguments, &commandLine);
        cr_assert_str_eq(commandLine.pOptionsPath, "a.toml");
        cr_assert_str_eq(commandLine.pConfigPath, "b.toml");
        cr_assert(commandLine.isConfigGiven);
        cr_assert_str_eq(commandLine.pOutputDirectory, "out");
}

Test(Cli, ReadsEachCommand)
{
        static const struct
        {
                char*               pName;
                OpenKBuild_tCommand command;
        } kCases[] = {
                { "help", OpenKBuild_kHelp },
                { "--help", OpenKBuild_kHelp },
                { "-h", OpenKBuild_kHelp },
                { "version", OpenKBuild_kVersion },
                { "--version", OpenKBuild_kVersion },
                { "check", OpenKBuild_kCheck },
                { "defconfig", OpenKBuild_kDefconfig },
                { "generate", OpenKBuild_kGenerate },
        };

        for(size_t index = 0; index < sizeof(kCases) / sizeof(kCases[ 0 ]); index++)
        {
                char* const             arguments[] = { "openkbuild", kCases[ index ].pName };
                OpenKBuild_tCommandLine commandLine;

                Accept(2, arguments, &commandLine);
                cr_expect_eq(commandLine.command,
                             kCases[ index ].command,
                             "%s",
                             kCases[ index ].pName);
        }
}

Test(Cli, RefusesWhatItDoesNotTake)
{
        char* const none[]      = { "openkbuild" };
        char* const unknown[]   = { "openkbuild", "build" };
        char* const wrongFlag[] = { "openkbuild", "check", "--output", "x" };
        char* const extra[]     = { "openkbuild", "check", "file" };
        char* const twice[]     = { "openkbuild", "check", "--options", "a", "--options", "b" };
        char* const noValue[]   = { "openkbuild", "check", "--options" };
        char* const empty[]     = { "openkbuild", "check", "--options=" };
        char* const prefix[]    = { "openkbuild", "check", "--optionsx" };

        Refuse(1, none, "openkbuild: a command is missing");
        Refuse(2, unknown, "openkbuild: unknown command build");
        Refuse(4, wrongFlag, "openkbuild: check does not take --output");
        Refuse(3, extra, "openkbuild: check does not take file");
        Refuse(6, twice, "openkbuild: --options is given twice");
        Refuse(3, noValue, "openkbuild: --options needs a value");
        Refuse(3, empty, "openkbuild: --options needs a value");
        Refuse(3, prefix, "openkbuild: check does not take --optionsx");
}
