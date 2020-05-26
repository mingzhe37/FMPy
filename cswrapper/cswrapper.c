#ifdef _WIN32
#include <Windows.h>
#else
#include <libgen.h>
#include <dlfcn.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cvode/cvode.h>               /* prototypes for CVODE fcts., consts.  */
#include <nvector/nvector_serial.h>    /* access to serial N_Vector            */
#include <sunmatrix/sunmatrix_dense.h> /* access to dense SUNMatrix            */
#include <sunlinsol/sunlinsol_dense.h> /* access to dense SUNLinearSolver      */
#include <sundials/sundials_types.h>   /* defs. of realtype, sunindextype      */

#include "fmi2Functions.h"


#define RTOL  RCONST(1.0e-4)   /* scalar relative tolerance            */

#if defined(_WIN32)
#define SHARED_LIBRARY_EXTENSION ".dll"
#elif defined(__APPLE__)
#define SHARED_LIBRARY_EXTENSION ".dylib"
#else
#define SHARED_LIBRARY_EXTENSION ".so"
#endif

typedef struct {

#if defined(_WIN32)
    HMODULE libraryHandle;
#else
    void *libraryHandle;
#endif

    fmi2Component c;
    
    size_t nx;
    size_t nz;
    
    void *cvode_mem;
    N_Vector x;
    N_Vector abstol;

    /***************************************************
    Common Functions
    ****************************************************/
    fmi2GetTypesPlatformTYPE         *fmi2GetTypesPlatform;
    fmi2GetVersionTYPE               *fmi2GetVersion;
    fmi2SetDebugLoggingTYPE          *fmi2SetDebugLogging;
    fmi2InstantiateTYPE              *fmi2Instantiate;
    fmi2FreeInstanceTYPE             *fmi2FreeInstance;
    fmi2SetupExperimentTYPE          *fmi2SetupExperiment;
    fmi2EnterInitializationModeTYPE  *fmi2EnterInitializationMode;
    fmi2ExitInitializationModeTYPE   *fmi2ExitInitializationMode;
    fmi2TerminateTYPE                *fmi2Terminate;
    fmi2ResetTYPE                    *fmi2Reset;
    fmi2GetRealTYPE                  *fmi2GetReal;
    fmi2GetIntegerTYPE               *fmi2GetInteger;
    fmi2GetBooleanTYPE               *fmi2GetBoolean;
    fmi2GetStringTYPE                *fmi2GetString;
    fmi2SetRealTYPE                  *fmi2SetReal;
    fmi2SetIntegerTYPE               *fmi2SetInteger;
    fmi2SetBooleanTYPE               *fmi2SetBoolean;
    fmi2SetStringTYPE                *fmi2SetString;
    fmi2GetFMUstateTYPE              *fmi2GetFMUstate;
    fmi2SetFMUstateTYPE              *fmi2SetFMUstate;
    fmi2FreeFMUstateTYPE             *fmi2FreeFMUstate;
    fmi2SerializedFMUstateSizeTYPE   *fmi2SerializedFMUstateSize;
    fmi2SerializeFMUstateTYPE        *fmi2SerializeFMUstate;
    fmi2DeSerializeFMUstateTYPE      *fmi2DeSerializeFMUstate;
    fmi2GetDirectionalDerivativeTYPE *fmi2GetDirectionalDerivative;


    /***************************************************
    Functions for FMI2 for Model Exchange
    ****************************************************/
    fmi2EnterEventModeTYPE                *fmi2EnterEventMode;
    fmi2NewDiscreteStatesTYPE             *fmi2NewDiscreteStates;
    fmi2EnterContinuousTimeModeTYPE       *fmi2EnterContinuousTimeMode;
    fmi2CompletedIntegratorStepTYPE       *fmi2CompletedIntegratorStep;
    fmi2SetTimeTYPE                       *fmi2SetTime;
    fmi2SetContinuousStatesTYPE           *fmi2SetContinuousStates;
    fmi2GetDerivativesTYPE                *fmi2GetDerivatives;
    fmi2GetEventIndicatorsTYPE            *fmi2GetEventIndicators;
    fmi2GetContinuousStatesTYPE           *fmi2GetContinuousStates;
    fmi2GetNominalsOfContinuousStatesTYPE *fmi2GetNominalsOfContinuousStates;

} Model;

static int f(realtype t, N_Vector y, N_Vector ydot, void *user_data) {
    
    Model *m = (Model *)user_data;
        
    fmi2Status status = m->fmi2SetTime(m->c, t);
    
    status = m->fmi2GetContinuousStates(m->c, NV_DATA_S(y), NV_LENGTH_S(y));
    
    status = m->fmi2GetDerivatives(m->c, NV_DATA_S(ydot), NV_LENGTH_S(ydot));
    
    return 0;
    
}

static int g(realtype t, N_Vector y, realtype *gout, void *user_data) {
    return 0;
}


/***************************************************
Types for Common Functions
****************************************************/

/* Inquire version numbers of header files and setting logging status */
const char* fmi2GetTypesPlatform(void) { return fmi2TypesPlatform; }

const char* fmi2GetVersion(void) { return fmi2Version; }

fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn, size_t nCategories, const fmi2String categories[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetDebugLogging(c, loggingOn, nCategories, categories);
}


/* Creation and destruction of FMU instances and setting debug status */
#define GET(f) m->f = dlsym(m->libraryHandle, #f);

/* Creation and destruction of FMU instances and setting debug status */
fmi2Component fmi2Instantiate(fmi2String instanceName,
                              fmi2Type fmuType,
                              fmi2String fmuGUID,
                              fmi2String fmuResourceLocation,
                              const fmi2CallbackFunctions* functions,
                              fmi2Boolean visible,
                              fmi2Boolean loggingOn) {

    if (fmuType != fmi2CoSimulation) {
        if (functions && functions->logger) {
            functions->logger(NULL, instanceName, fmi2Error, "logError", "Argument fmuType must be fmi2CoSimulation.");
        }
        return NULL;
    }

    Model *m = calloc(1, sizeof(Model));
    
    Dl_info info;
    
    if (!dladdr(fmi2Instantiate, &info)) {
        if (functions && functions->logger) {
            functions->logger(NULL, instanceName, fmi2Error, "logError", "Failed to get shared library info.");
        }
        return NULL;
    }
    
//    char *name = strdup(info.dli_fname);
    
    char *name = strdup("/Users/tors10/Development/Reference-FMUs/build/temp/VanDerPol/binaries/darwin64/VanDerPol_2_0.dylib");
    
    size_t len = strlen(name);
    
    // remove the file extension
    char *sle = SHARED_LIBRARY_EXTENSION;
    size_t slelen = strlen(sle);
    
    name[len-slelen] = '\0';
    
    // number of event indicators as a string
    char *n = strrchr(name, '_');
    
    m->nz = atoi(n+1);
    
    len = strlen(name);
    name[len-strlen(n)] = '\0';

    // number of continuous states as a string
    n = strrchr(name, '_');
    
    m->nx = atoi(n+1);

    len = strlen(name);
    name[len-strlen(n)] = '\0';
    
    // re-append the file extension
    strcat(name, SHARED_LIBRARY_EXTENSION);

    m->libraryHandle = dlopen(name, RTLD_LAZY);

    GET(fmi2GetTypesPlatform)
    GET(fmi2GetVersion)
    GET(fmi2SetDebugLogging)
    GET(fmi2Instantiate)
    GET(fmi2FreeInstance)
    GET(fmi2SetupExperiment)
    GET(fmi2EnterInitializationMode)
    GET(fmi2ExitInitializationMode)
    GET(fmi2Terminate)
    GET(fmi2Reset)
    GET(fmi2GetReal)
    GET(fmi2GetInteger)
    GET(fmi2GetBoolean)
    GET(fmi2GetString)
    GET(fmi2SetReal)
    GET(fmi2SetInteger)
    GET(fmi2SetBoolean)
    GET(fmi2SetString)
    GET(fmi2GetFMUstate)
    GET(fmi2SetFMUstate)
    GET(fmi2FreeFMUstate)
    GET(fmi2SerializedFMUstateSize)
    GET(fmi2SerializeFMUstate)
    GET(fmi2DeSerializeFMUstate)
    GET(fmi2GetDirectionalDerivative)

    GET(fmi2EnterEventMode)
    GET(fmi2NewDiscreteStates)
    GET(fmi2EnterContinuousTimeMode)
    GET(fmi2CompletedIntegratorStep)
    GET(fmi2SetTime)
    GET(fmi2SetContinuousStates)
    GET(fmi2GetDerivatives)
    GET(fmi2GetEventIndicators)
    GET(fmi2GetContinuousStates)
    GET(fmi2GetNominalsOfContinuousStates)

    m->c = m->fmi2Instantiate(instanceName, fmi2ModelExchange, fmuGUID, fmuResourceLocation, functions, visible, loggingOn);
            
    m->x = N_VNew_Serial(m->nx);
    m->abstol = N_VNew_Serial(m->nx);
    
    for (size_t i = 0; i < m->nx; i++) {
        NV_DATA_S(m->abstol)[i] = RTOL;
    }
    
    m->cvode_mem = CVodeCreate(CV_BDF);
    
    int cstatus = CVodeInit(m->cvode_mem, f, 0, m->x);

    cstatus = CVodeSVtolerances(m->cvode_mem, RTOL, m->abstol);

    cstatus = CVodeRootInit(m->cvode_mem, m->nz, g);

    SUNMatrix A = SUNDenseMatrix(m->nx, m->nx);

    SUNLinearSolver LS = SUNLinSol_Dense(m->x, A);

    cstatus = CVodeSetLinearSolver(m->cvode_mem, LS, A);
    
    cstatus = CVodeSetUserData(m->cvode_mem, m);

    return m;
}

void fmi2FreeInstance(fmi2Component c) {
    if (!c) return;
    Model *m = (Model *)c;
    dlclose(m->libraryHandle);
    free(m);
}

/* Enter and exit initialization mode, terminate and reset */
fmi2Status fmi2SetupExperiment(fmi2Component c,
                               fmi2Boolean toleranceDefined,
                               fmi2Real tolerance,
                               fmi2Real startTime,
                               fmi2Boolean stopTimeDefined,
                               fmi2Real stopTime) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetupExperiment(m->c, toleranceDefined, tolerance, startTime, stopTimeDefined, stopTime);
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2EnterInitializationMode(m->c);
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    fmi2Status status = m->fmi2ExitInitializationMode(m->c);
    return m->fmi2EnterContinuousTimeMode(m->c);
}

fmi2Status fmi2Terminate(fmi2Component c) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2Terminate(m->c);
}

fmi2Status fmi2Reset(fmi2Component c) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2Reset(m->c);
}

/* Getting and setting variable values */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real    value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2GetReal(m->c, vr, nvr, value);
}

fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2GetInteger(m->c, vr, nvr, value);
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2GetBoolean(m->c, vr, nvr, value);
}

fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String  value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2GetString(m->c, vr, nvr, value);
}

fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real    value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetReal(m->c, vr, nvr, value);
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetInteger(m->c, vr, nvr, value);
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetBoolean(m->c, vr, nvr, value);
}

fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String  value[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2SetString(m->c, vr, nvr, value);
}

/* Getting and setting the internal FMU state */
fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    return fmi2Error;
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate  FMUstate) {
    return fmi2Error;
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    return fmi2Error;
}

fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate  FMUstate, size_t* size) {
    return fmi2Error;
}

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate  FMUstate, fmi2Byte serializedState[], size_t size) {
    return fmi2Error;
}

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate) {
    return fmi2Error;
}

/* Getting partial derivatives */
fmi2Status fmi2GetDirectionalDerivative(fmi2Component c,
                                        const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
                                        const fmi2ValueReference vKnown_ref[],   size_t nKnown,
                                        const fmi2Real dvKnown[],
                                        fmi2Real dvUnknown[]) {
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    return m->fmi2GetDirectionalDerivative(m->c, vUnknown_ref, nUnknown, vKnown_ref, nKnown, dvKnown, dvUnknown);
}

/***************************************************
Types for Functions for FMI2 for Co-Simulation
****************************************************/

/* Simulating the slave */
fmi2Status fmi2SetRealInputDerivatives(fmi2Component c,
                                       const fmi2ValueReference vr[], size_t nvr,
                                       const fmi2Integer order[],
                                       const fmi2Real value[]) {
    return fmi2Error;
}
fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c,
                                        const fmi2ValueReference vr[], size_t nvr,
                                        const fmi2Integer order[],
                                        fmi2Real value[]) {
    return fmi2Error;
}



fmi2Status fmi2DoStep(fmi2Component c,
                      fmi2Real      currentCommunicationPoint,
                      fmi2Real      communicationStepSize,
                      fmi2Boolean   noSetFMUStatePriorToCurrentPoint) {
    
    if (!c) return fmi2Error;
    Model *m = (Model *)c;
    
    fmi2Status status;
    
    realtype tret;
    
    status = m->fmi2GetContinuousStates(m->c, NV_DATA_S(m->x), NV_LENGTH_S(m->x));
    
    status = CVode(m->cvode_mem, currentCommunicationPoint + communicationStepSize, m->x, &tret, CV_NORMAL);
    
    status = m->fmi2SetContinuousStates(m->c, NV_DATA_S(m->x), NV_LENGTH_S(m->x));
    
    fmi2Boolean enterEventMode, terminateSimulation;
    
    status = m->fmi2CompletedIntegratorStep(m->c, fmi2False, &enterEventMode, &terminateSimulation);
    
    return fmi2OK;
}

fmi2Status fmi2CancelStep(fmi2Component c) {
    return fmi2Error;
}

/* Inquire slave status */
fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status*  value) {
    return fmi2Error;
}

fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real*    value) {
    return fmi2Error;
}

fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value) {
    return fmi2Error;
}

fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value) {
    return fmi2Error;
}

fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String*  value) {
    return fmi2Error;
}
