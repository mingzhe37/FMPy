/**************************************************************
 *  Copyright (c) Modelica Association Project "FMI".         *
 *  All rights reserved.                                      *
 *  This file is part of the Reference FMUs. See LICENSE.txt  *
 *  in the project root for license information.              *
 **************************************************************/

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#else
#include <stdarg.h>
#include <dlfcn.h>
#endif

#include "FMI.h"

#define INITIAL_MESSAGE_BUFFER_SIZE 1024


FMIInstance *FMICreateInstance(const char *instanceName, const char *libraryPath, FMILogMessage *logMessage, FMILogFunctionCall *logFunctionCall) {

# ifdef _WIN32
    TCHAR Buffer[1024];
    GetCurrentDirectory(1024, Buffer);

    WCHAR dllDirectory[MAX_PATH];

    // convert path to unicode
    mbstowcs(dllDirectory, libraryPath, MAX_PATH);

    // add the binaries directory temporarily to the DLL path to allow discovery of dependencies
    DLL_DIRECTORY_COOKIE dllDirectoryCookie = AddDllDirectory(dllDirectory);

    // TODO: log getLastSystemError()

    HMODULE libraryHandle = LoadLibraryExA(libraryPath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    // remove the binaries directory from the DLL path
    if (dllDirectoryCookie) {
        RemoveDllDirectory(dllDirectoryCookie);
    }

    // TODO: log error

# else
    void *libraryHandle = dlopen(libraryPath, RTLD_LAZY);
# endif

    if (!libraryHandle) {
        return NULL;
    }

    FMIInstance* instance = (FMIInstance*)calloc(1, sizeof(FMIInstance));

    instance->libraryHandle = libraryHandle;

    instance->logMessage = logMessage;
    instance->logFunctionCall = logFunctionCall;

    instance->bufsize1 = INITIAL_MESSAGE_BUFFER_SIZE;
    instance->bufsize2 = INITIAL_MESSAGE_BUFFER_SIZE;

    instance->buf1 = (char *)calloc(instance->bufsize1, sizeof(char));
    instance->buf2 = (char *)calloc(instance->bufsize1, sizeof(char));

    instance->name = strdup(instanceName);

    instance->status = FMIOK;

    return instance;
}

void FMIFreeInstance(FMIInstance *instance) {

    // unload the shared library
    if (instance->libraryHandle) {
# ifdef _WIN32
        FreeLibrary(instance->libraryHandle);
# else
        dlclose(instance->libraryHandle);
# endif
        instance->libraryHandle = NULL;
    }

    free(instance);
}

const char* FMIValueReferencesToString(FMIInstance *instance, const FMIValueReference vr[], size_t nvr) {

    size_t pos = 0;

    do {
        pos += snprintf(&instance->buf1[pos], instance->bufsize1 - pos, "{");

        for (size_t i = 0; i < nvr; i++) {

            pos += snprintf(&instance->buf1[pos], instance->bufsize1 - pos, i < nvr - 1 ? "%u, " : "%u", vr[i]);

            if (pos > instance->bufsize1 - 2) {
                pos = 0;
                instance->bufsize1 *= 2;
                instance->buf1 = (char*)realloc(instance->buf1, instance->bufsize1);
                break;
            }
        }
    } while (pos == 0);

    pos += snprintf(&instance->buf1[pos], instance->bufsize1 - pos, "}");

    return instance->buf1;
}

const char* FMIValuesToString(FMIInstance *instance, size_t nvr, const void *value, FMIVariableType variableType) {

    size_t pos = 0;

    do {
        pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, "{");

        for (size_t i = 0; i < nvr; i++) {

            switch (variableType) {
            case FMIFloat64Type:
                pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "%.16g, " : "%.16g", ((fmi2Real *)value)[i]);
                break;
            case FMIInt32Type:
                pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "%d, " : "%d", ((fmi2Integer *)value)[i]);
                break;
            case FMIBooleanType:
                if (instance->fmiVersion == FMIVersion1) {
                    //pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "%d, " : "%d", ((fmi1Boolean *)value)[i]);
                } else {
                    pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "%d, " : "%d", ((fmi2Boolean *)value)[i]);
                }
                break;
            case FMIStringType:
                pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "\"%s\", " : "\"%s\"", ((fmi2String *)value)[i]);
                break;
            case FMIClockType:
                pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, i < nvr - 1 ? "%d, " : "%d", ((fmi3Clock *)value)[i]);
                break;
            }

            if (pos > instance->bufsize2 - 2) {
                pos = 0;
                instance->bufsize2 *= 2;
                instance->buf2 = (char*)realloc(instance->buf2, instance->bufsize2);
                break;
            }
        }
    } while (pos == 0);

    pos += snprintf(&instance->buf2[pos], instance->bufsize2 - pos, "}");

    return instance->buf2;
}
