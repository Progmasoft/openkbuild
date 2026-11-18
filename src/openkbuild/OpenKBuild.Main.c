/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "openkbuild/OpenKBuild.Cli.h"
#include "openkbuild/OpenKBuild.Config.h"
#include "openkbuild/OpenKBuild.Options.h"
#include "openkbuild/OpenKBuild.Output.h"

/** The exit status of a run whose input was wrong or whose files could not be written. */
#define OPENKBUILD_EXIT_FAILED 1
/** The exit status of a command line that openkbuild does not accept. */
#define OPENKBUILD_EXIT_USAGE 2
/** The size of a path openkbuild builds, its final null character included. */
#define OPENKBUILD_PATH_SIZE 4096U

/** A function that writes one generated file. */
typedef bool (*tFn_Write)(const void* aIn_pSource, FILE* aInOut_pFile);

/**
 * @brief Writes config.h, in the form tFn_Write takes.
 *
 * @param[in]     aIn_pSource  The configuration.
 * @param[in,out] aInOut_pFile The stream.
 * @return false when writing fails.
 */
static bool
WriteHeader(const void* aIn_pSource, FILE* aInOut_pFile)
{
        return (OpenKBuild_Output_WriteHeader(aIn_pSource, aInOut_pFile));
}

/**
 * @brief Writes config.mk, in the form tFn_Write takes.
 *
 * @param[in]     aIn_pSource  The configuration.
 * @param[in,out] aInOut_pFile The stream.
 * @return false when writing fails.
 */
static bool
WriteMake(const void* aIn_pSource, FILE* aInOut_pFile)
{
        return (OpenKBuild_Output_WriteMake(aIn_pSource, aInOut_pFile));
}

/**
 * @brief Writes the defaults, in the form tFn_Write takes.
 *
 * @param[in]     aIn_pSource  The options.
 * @param[in,out] aInOut_pFile The stream.
 * @return false when writing fails.
 */
static bool
WriteDefaults(const void* aIn_pSource, FILE* aInOut_pFile)
{
        return (OpenKBuild_Output_WriteDefaults(aIn_pSource, aInOut_pFile));
}

/**
 * @brief Creates a directory unless it exists.
 *
 * @param[in] aIn_pPath The directory.
 * @return true when the directory exists afterwards.
 */
static bool
MakeDirectory(const char* aIn_pPath)
{
        struct stat status;

        if(mkdir(aIn_pPath, 0777) == 0)
                return (true);
        if(errno == EEXIST && stat(aIn_pPath, &status) == 0 && S_ISDIR(status.st_mode))
                return (true);
        fprintf(stderr, "openkbuild: %s: %s\n", aIn_pPath, strerror(errno));
        return (false);
}

/**
 * @brief Writes a file whole or not at all: into a temporary file beside it, then renamed over it.
 *
 * The directory of the file is created when it does not exist; its parents must exist.
 *
 * @param[in] aIn_pPath   The file.
 * @param[in] aFn_Write   What writes its contents.
 * @param[in] aIn_pSource What aFn_Write reads.
 * @return true when the file was written.
 */
static bool
WriteFile(const char* aIn_pPath, tFn_Write aFn_Write, const void* aIn_pSource)
{
        char        temporary[ OPENKBUILD_PATH_SIZE ];
        char        directory[ OPENKBUILD_PATH_SIZE ];
        const char* pSlash = strrchr(aIn_pPath, '/');
        FILE*       pFile;
        bool        isWritten;

        if(snprintf(temporary, sizeof(temporary), "%s.tmp", aIn_pPath) >= (int) sizeof(temporary))
        {
                fprintf(stderr, "openkbuild: %s: the path is too long\n", aIn_pPath);
                return (false);
        }
        if(pSlash != NULL && pSlash != aIn_pPath)
        {
                memcpy(directory, aIn_pPath, (size_t) (pSlash - aIn_pPath));
                directory[ pSlash - aIn_pPath ] = '\0';
                if(!MakeDirectory(directory))
                        return (false);
        }
        pFile = fopen(temporary, "w");
        if(pFile == NULL)
        {
                fprintf(stderr, "openkbuild: %s: %s\n", temporary, strerror(errno));
                return (false);
        }
        isWritten = aFn_Write(aIn_pSource, pFile);
        if(fclose(pFile) != 0)
                isWritten = false;
        if(!isWritten || rename(temporary, aIn_pPath) != 0)
        {
                fprintf(stderr, "openkbuild: %s: %s\n", aIn_pPath, strerror(errno));
                remove(temporary);
                return (false);
        }
        return (true);
}

/**
 * @brief Runs generate: resolves the configuration and writes config.h and config.mk.
 *
 * @param[in] aIn_pCommandLine The command line.
 * @param[in] aIn_pOptions     The options.
 * @return The exit status.
 */
static int
Generate(const OpenKBuild_tCommandLine* aIn_pCommandLine, const OpenKBuild_tOptions* aIn_pOptions)
{
        OpenKBuild_tConfig     config;
        OpenKBuild_tDiagnostic diagnostic;
        const char*            pConfigPath = aIn_pCommandLine->pConfigPath;
        char                   header[ OPENKBUILD_PATH_SIZE ];
        char                   make[ OPENKBUILD_PATH_SIZE ];
        struct stat            status;
        bool                   isWritten;

        if(!aIn_pCommandLine->isConfigGiven && stat(pConfigPath, &status) != 0 && errno == ENOENT)
        {
                fprintf(stderr, "openkbuild: %s does not exist; using the defaults\n", pConfigPath);
                pConfigPath = NULL;
        }
        if(!OpenKBuild_Config_Init(&config, aIn_pOptions, pConfigPath, stderr, &diagnostic))
        {
                fprintf(stderr, "%s\n", diagnostic.text);
                return (OPENKBUILD_EXIT_FAILED);
        }
        if(snprintf(header, sizeof(header), "%s/config.h", aIn_pCommandLine->pOutputDirectory)
                   >= (int) sizeof(header)
           || snprintf(make, sizeof(make), "%s/config.mk", aIn_pCommandLine->pOutputDirectory)
                   >= (int) sizeof(make))
        {
                fprintf(stderr, "openkbuild: the output directory is too long\n");
                OpenKBuild_Config_Deinit(&config);
                return (OPENKBUILD_EXIT_FAILED);
        }
        isWritten = MakeDirectory(aIn_pCommandLine->pOutputDirectory)
                && WriteFile(header, WriteHeader, &config) && WriteFile(make, WriteMake, &config);
        OpenKBuild_Config_Deinit(&config);
        return (isWritten ? EXIT_SUCCESS : OPENKBUILD_EXIT_FAILED);
}

/**
 * @brief Runs openkbuild.
 *
 * @param[in] aArgumentCount  The number of arguments.
 * @param[in] aIn_ppArguments The arguments.
 * @return 0 on success, 1 when the input is wrong or a file cannot be written, 2 for a command
 *         line it does not accept.
 */
int
main(int aArgumentCount, char** aIn_ppArguments)
{
        OpenKBuild_tCommandLine commandLine;
        OpenKBuild_tDiagnostic  diagnostic;
        OpenKBuild_tOptions     options;
        int                     status = EXIT_SUCCESS;

        if(!OpenKBuild_Cli_Parse(aArgumentCount, aIn_ppArguments, &commandLine, &diagnostic))
        {
                fprintf(stderr, "%s\n", diagnostic.text);
                OpenKBuild_Cli_WriteUsage(stderr);
                return (OPENKBUILD_EXIT_USAGE);
        }
        switch(commandLine.command)
        {
        case OpenKBuild_kHelp:
                OpenKBuild_Cli_WriteUsage(stdout);
                return (EXIT_SUCCESS);
        case OpenKBuild_kVersion:
                puts("openkbuild " OPENKBUILD_VERSION);
                return (EXIT_SUCCESS);
        default:
                break;
        }
        if(!OpenKBuild_Options_Init(&options, commandLine.pOptionsPath, &diagnostic))
        {
                fprintf(stderr, "%s\n", diagnostic.text);
                return (OPENKBUILD_EXIT_FAILED);
        }
        if(commandLine.command == OpenKBuild_kDefconfig)
                status = WriteFile(commandLine.pOutputPath, WriteDefaults, &options)
                        ? EXIT_SUCCESS
                        : OPENKBUILD_EXIT_FAILED;
        else if(commandLine.command == OpenKBuild_kGenerate)
                status = Generate(&commandLine, &options);
        OpenKBuild_Options_Deinit(&options);
        return (status);
}
