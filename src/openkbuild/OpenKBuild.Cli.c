/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "openkbuild/OpenKBuild.Cli.h"
#include "openkbuild/OpenKBuild.Options.h"

/** The flags, each a bit, so a command can name those it takes. */
enum
{
        kFlagOptions   = 1 << 0,
        kFlagConfig    = 1 << 1,
        kFlagOutput    = 1 << 2,
        kFlagOutputDir = 1 << 3,
};

/** A command: its name and the flags it takes. */
typedef struct Command
{
        const char*         pName;
        OpenKBuild_tCommand command;
        unsigned            flags;
} tCommand;

static const tCommand kCommands[] = {
        { "help", OpenKBuild_kHelp, 0 },
        { "--help", OpenKBuild_kHelp, 0 },
        { "-h", OpenKBuild_kHelp, 0 },
        { "version", OpenKBuild_kVersion, 0 },
        { "--version", OpenKBuild_kVersion, 0 },
        { "check", OpenKBuild_kCheck, kFlagOptions },
        { "defconfig", OpenKBuild_kDefconfig, kFlagOptions | kFlagOutput },
        { "generate", OpenKBuild_kGenerate, kFlagOptions | kFlagConfig | kFlagOutputDir },
};

/** A flag: its name, its bit, and the place of its value in the command line. */
typedef struct Flag
{
        const char* pName;
        unsigned    bit;
} tFlag;

static const tFlag kFlags[] = {
        { "--options", kFlagOptions },
        { "--config", kFlagConfig },
        { "--output", kFlagOutput },
        { "--output-dir", kFlagOutputDir },
};

/**
 * @brief Finds the field of the command line that a flag sets.
 *
 * @param[in,out] aInOut_pCommandLine The command line.
 * @param[in]     aBit                The bit of the flag.
 * @return The field.
 */
static const char**
FlagField(OpenKBuild_tCommandLine* aInOut_pCommandLine, unsigned aBit)
{
        switch(aBit)
        {
        case kFlagOptions:
                return (&aInOut_pCommandLine->pOptionsPath);
        case kFlagConfig:
                return (&aInOut_pCommandLine->pConfigPath);
        case kFlagOutput:
                return (&aInOut_pCommandLine->pOutputPath);
        default:
                return (&aInOut_pCommandLine->pOutputDirectory);
        }
}

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
                     OpenKBuild_tDiagnostic*  aOut_pDiagnostic)
{
        const tCommand* pCommand = NULL;
        unsigned        given    = 0;

        memset(aOut_pCommandLine, 0, sizeof(*aOut_pCommandLine));
        aOut_pCommandLine->pOptionsPath     = "OpenKBuild.conf.toml";
        aOut_pCommandLine->pConfigPath      = ".config.toml";
        aOut_pCommandLine->pOutputPath      = ".config.toml";
        aOut_pCommandLine->pOutputDirectory = "build";
        if(aArgumentCount < 2)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic, "openkbuild", 0, 0, "a command is missing");
                return (false);
        }
        for(size_t index = 0; index < sizeof(kCommands) / sizeof(kCommands[ 0 ]); index++)
        {
                if(strcmp(aIn_ppArguments[ 1 ], kCommands[ index ].pName) == 0)
                        pCommand = &kCommands[ index ];
        }
        if(pCommand == NULL)
        {
                OpenKBuild_Diagnose(aOut_pDiagnostic,
                                    "openkbuild",
                                    0,
                                    0,
                                    "unknown command %s",
                                    aIn_ppArguments[ 1 ]);
                return (false);
        }
        aOut_pCommandLine->command = pCommand->command;
        for(int argument = 2; argument < aArgumentCount; argument++)
        {
                const char*  pArgument = aIn_ppArguments[ argument ];
                const tFlag* pFlag     = NULL;
                const char*  pValue    = NULL;

                for(size_t index = 0; index < sizeof(kFlags) / sizeof(kFlags[ 0 ]); index++)
                {
                        size_t length = strlen(kFlags[ index ].pName);

                        if(strncmp(pArgument, kFlags[ index ].pName, length) == 0
                           && (pArgument[ length ] == '\0' || pArgument[ length ] == '='))
                        {
                                pFlag  = &kFlags[ index ];
                                pValue = pArgument[ length ] == '=' ? pArgument + length + 1 : NULL;
                        }
                }
                if(pFlag == NULL || (pCommand->flags & pFlag->bit) == 0)
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            "openkbuild",
                                            0,
                                            0,
                                            "%s does not take %s",
                                            pCommand->pName,
                                            pArgument);
                        return (false);
                }
                if((given & pFlag->bit) != 0)
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            "openkbuild",
                                            0,
                                            0,
                                            "%s is given twice",
                                            pFlag->pName);
                        return (false);
                }
                if(pValue == NULL && argument + 1 < aArgumentCount)
                        pValue = aIn_ppArguments[ ++argument ];
                if(pValue == NULL || pValue[ 0 ] == '\0')
                {
                        OpenKBuild_Diagnose(aOut_pDiagnostic,
                                            "openkbuild",
                                            0,
                                            0,
                                            "%s needs a value",
                                            pFlag->pName);
                        return (false);
                }
                given                                    |= pFlag->bit;
                *FlagField(aOut_pCommandLine, pFlag->bit) = pValue;
        }
        aOut_pCommandLine->isConfigGiven = (given & kFlagConfig) != 0;
        return (true);
}

/**
 * @brief Writes how to call openkbuild.
 *
 * @param[in,out] aInOut_pFile The stream.
 */
void
OpenKBuild_Cli_WriteUsage(FILE* aInOut_pFile)
{
        fputs("Usage: openkbuild <command> [flags]\n"
              "\n"
              "Commands:\n"
              "  check      check the option definitions\n"
              "  defconfig  write a configuration with every default\n"
              "  generate   resolve the configuration and write config.h and config.mk\n"
              "  version    print the version\n"
              "  help       print this text\n"
              "\n"
              "Flags:\n"
              "  --options FILE     option definitions (check, defconfig, generate);\n"
              "                     default OpenKBuild.conf.toml\n"
              "  --output FILE      configuration to write (defconfig); default .config.toml\n"
              "  --config FILE      configuration to read (generate); default .config.toml,\n"
              "                     the defaults alone when that file does not exist\n"
              "  --output-dir DIR   where to write config.h and config.mk (generate); default "
              "build\n",
              aInOut_pFile);
}
