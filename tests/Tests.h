/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>

#include <criterion/criterion.h>

#include "openkbuild/OpenKBuild.Options.h"

/**
 * @brief Reads option definitions from text and fails the test when they are not valid.
 *
 * @param[out] aOut_pOptions The options.
 * @param[in]  aIn_pText     The TOML text.
 */
static inline void
Tests_LoadOptions(OpenKBuild_tOptions* aOut_pOptions, const char* aIn_pText)
{
        OpenKBuild_tDiagnostic diagnostic;
        bool                   isLoaded = OpenKBuild_Options_InitFromText(aOut_pOptions,
                                                                          aIn_pText,
                                                                          "test.toml",
                                                                          &diagnostic);

        cr_assert(isLoaded, "%s", diagnostic.text);
}

/**
 * @brief Reads option definitions that must be refused, and returns the diagnostic.
 *
 * @param[in]  aIn_pText        The TOML text.
 * @param[out] aOut_pDiagnostic The diagnostic.
 */
static inline void
Tests_RefuseOptions(const char* aIn_pText, OpenKBuild_tDiagnostic* aOut_pDiagnostic)
{
        OpenKBuild_tOptions options;

        cr_assert_not(
                OpenKBuild_Options_InitFromText(&options, aIn_pText, "test.toml", aOut_pDiagnostic),
                "the definitions were accepted");
        cr_assert_eq(options.count, 0);
        cr_assert_null(options.pItems);
}

/**
 * @brief A stream in memory, for what a function writes.
 */
typedef struct Tests_Capture
{
        char*  pText;
        size_t size;
        FILE*  pFile;
} Tests_tCapture;

/**
 * @brief Opens a stream in memory.
 *
 * @param[out] aOut_pCapture The stream.
 */
static inline void
Tests_Capture_Open(Tests_tCapture* aOut_pCapture)
{
        aOut_pCapture->pText = NULL;
        aOut_pCapture->size  = 0;
        aOut_pCapture->pFile = open_memstream(&aOut_pCapture->pText, &aOut_pCapture->size);
        cr_assert_not_null(aOut_pCapture->pFile);
}

/**
 * @brief Closes the stream; its text stays in pText until Tests_Capture_Free().
 *
 * @param[in,out] aInOut_pCapture The stream.
 */
static inline void
Tests_Capture_Close(Tests_tCapture* aInOut_pCapture)
{
        fclose(aInOut_pCapture->pFile);
        aInOut_pCapture->pFile = NULL;
}

/**
 * @brief Releases the text of a closed stream.
 *
 * @param[in,out] aInOut_pCapture The stream.
 */
static inline void
Tests_Capture_Free(Tests_tCapture* aInOut_pCapture)
{
        free(aInOut_pCapture->pText);
        aInOut_pCapture->pText = NULL;
}
