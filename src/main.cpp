/**
 * @file main.cpp
 * @brief Implements the main code for the Device Update Agent.
 *
 * @copyright Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */
#include "aduc/adu_core_export_helpers.h"
#include "aduc/adu_core_interface.h"
#include "aduc/adu_types.h"
#include "aduc/agent_workflow.h"
#include "aduc/c_utils.h"
#include "aduc/client_handle_helper.h"
//#include "aduc/command_helper.h"
#include "aduc/config_utils.h"
#include "aduc/connection_string_utils.h"
#include "aduc/d2c_messaging.h"
#include "aduc/device_info_interface.h"
#include "aduc/extension_manager.h"
//#include "aduc/extension_utils.h"
//#include "aduc/health_management.h"
//#include "aduc/https_proxy_utils.h"
#include "aduc/iothub_communication_manager.h"
#include "aduc/logging.h"
//#include "aduc/permission_utils.h"
#include "aduc/string_c_utils.h"
//#include "aduc/system_utils.h"
#include <azure_c_shared_utility/shared_util_options.h>
#include <azure_c_shared_utility/threadapi.h> // ThreadAPI_Sleep
#include <ctype.h>
//#include <do_config.h>
#include <diagnostics_devicename.h>
#include <diagnostics_interface.h>
//#include <getopt.h>
#include <iothub_client_options.h>
#include <pnp_protocol.h>

#include <limits.h>
//#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // strtol
//#include <sys/stat.h>
//#include <unistd.h>

#include "pnp_protocol.h"

#include "eis_utils.h"

/* Mbed includes */
#include "mbed.h"
#include "NTPClient.h"

#include "configs/aduc_user_config.h"

// Print memory footprint
extern "C" {
#if (MBED_HEAP_STATS_ENABLED)
    void print_heap_stats(void);
#endif
#if (MBED_STACK_STATS_ENABLED)
    void print_stack_stats();
#endif
}

// Name of ADU Agent subcomponent that this device implements.
static const char g_aduPnPComponentName[] = "deviceUpdate";

// Name of DeviceInformation subcomponent that this device implements.
static const char g_deviceInfoPnPComponentName[] = "deviceInformation";

// Name of the Diagnostics subcomponent that this device is using
static const char g_diagnosticsPnPComponentName[] = "diagnosticInformation";

ADUC_LaunchArguments launchArgs;

/**
 * @brief Global IoT Hub client handle.
 */
ADUC_ClientHandle g_iotHubClientHandle = NULL;

/**
 * @brief Determines if we're shutting down.
 *
 * Value indicates the shutdown signal if shutdown requested.
 *
 * Do not shutdown = 0; SIGINT = 2; SIGTERM = 15;
 */
static int g_shutdownSignal = 0;

//
// Components that this agent supports.
//

/**
 * @brief Function signature for PnP Handler create method.
 */
typedef bool (*PnPComponentCreateFunc)(void** componentContext, int argc, char** argv);

/**
 * @brief Called once after connected to IoTHub (device client handler is valid).
 *
 * DigitalTwin handles aren't valid (and as such no calls may be made on them) until this method is called.
 */
typedef void (*PnPComponentConnectedFunc)(void* componentContext);

/**
 * @brief Function signature for PnP component worker method.
 *        Called regularly after the device client is created.
 *
 * This allows an component implementation to do work in a cooperative multitasking environment.
 */
typedef void (*PnPComponentDoWorkFunc)(void* componentContext);

/**
 * @brief Function signature for PnP component uninitialize method.
 */
typedef void (*PnPComponentDestroyFunc)(void** componentContext);

/**
 * @brief Called when a component's property is updated.
 *
 * @param updateState State to report.
 * @param result Result to report (optional, can be NULL).
 */
typedef void (*PnPComponentPropertyUpdateCallback)(
    ADUC_ClientHandle clientHandle,
    const char* propertyName,
    JSON_Value* propertyValue,
    int version,
    ADUC_PnPComponentClient_PropertyUpdate_Context* sourceContext,
    void* userContextCallback);

static ADUC_PnPComponentClient_PropertyUpdate_Context g_iotHubInitiatedPnPPropertyChangeContext = { false, false };

/**
 * @brief Defines an PnP Component Client that this agent supports.
 */
typedef struct tagPnPComponentEntry
{
    const char* ComponentName;
    ADUC_ClientHandle* clientHandle;
    const PnPComponentCreateFunc Create;
    const PnPComponentConnectedFunc Connected;
    const PnPComponentDoWorkFunc DoWork;
    const PnPComponentDestroyFunc Destroy;
    const PnPComponentPropertyUpdateCallback
        PnPPropertyUpdateCallback; /**< Called when a component's property is updated. (optional) */
    //
    // Following data is dynamic.
    // Must be initialized to NULL in map and remain last entries in this struct.
    //
    void* Context; /**< Opaque data returned from PnPComponentInitFunc(). */
} PnPComponentEntry;

// clang-format off
/**
 * @brief Interfaces to register.
 */
// NOLINTNEXTLINE(cppcoreguidelines-interfaces-global-init)
static PnPComponentEntry componentList[] = {
    {
        g_aduPnPComponentName,
        &g_iotHubClientHandleForADUComponent,
        AzureDeviceUpdateCoreInterface_Create,
        AzureDeviceUpdateCoreInterface_Connected,
        AzureDeviceUpdateCoreInterface_DoWork,
        AzureDeviceUpdateCoreInterface_Destroy,
        AzureDeviceUpdateCoreInterface_PropertyUpdateCallback
    },
    {
        g_deviceInfoPnPComponentName,
        &g_iotHubClientHandleForDeviceInfoComponent,
        DeviceInfoInterface_Create,
        DeviceInfoInterface_Connected,
        NULL /* DoWork method - not used */,
        DeviceInfoInterface_Destroy,
        NULL /* PropertyUpdateCallback - not used */
    },
    {
        g_diagnosticsPnPComponentName,
        &g_iotHubClientHandleForDiagnosticsComponent,
        DiagnosticsInterface_Create,
        DiagnosticsInterface_Connected,
        NULL /* DoWork method - not used */,
        DiagnosticsInterface_Destroy,
        DiagnosticsInterface_PropertyUpdateCallback
    },
};

// clang-format on

/**
 * @brief Parse command-line arguments.
 * @param argc arguments count.
 * @param argv arguments array.
 * @param launchArgs a struct to store the parsed arguments.
 *
 * @return 0 if succeeded without additional non-option args,
 * -1 on failure,
 *  or positive index to argv index where additional args start on success with additional args.
 */
int ParseLaunchArguments(const int argc, char** argv, ADUC_LaunchArguments* launchArgs)
{
    int result = 0;
    memset(launchArgs, 0, sizeof(*launchArgs));

    launchArgs->logLevel = ADUC_LOG_INFO;

    launchArgs->argc = argc;
    launchArgs->argv = argv;

    launchArgs->iotHubTracingEnabled = MBED_CONF_APP_IOTHUB_CLIENT_TRACE;

    if (result == 0 && launchArgs->connectionString)
    {
        ADUC_StringUtils_Trim(launchArgs->connectionString);
    }

    return result;
}

/**
 * @brief Sets the Diagnostic DeviceName for creating the device's diagnostic container
 * @param connectionString connectionString to extract the device-id and module-id from
 * @returns true on success; false on failure
 */
bool ADUC_SetDiagnosticsDeviceNameFromConnectionString(const char* connectionString)
{
    bool succeeded = false;

    char* deviceId = NULL;

    char* moduleId = NULL;

    if (!ConnectionStringUtils_GetDeviceIdFromConnectionString(connectionString, &deviceId))
    {
        goto done;
    }

    // Note: not all connection strings have a module-id
    ConnectionStringUtils_GetModuleIdFromConnectionString(connectionString, &moduleId);

    if (!DiagnosticsComponent_SetDeviceName(deviceId, moduleId))
    {
        goto done;
    }

    succeeded = true;

done:

    free(deviceId);
    free(moduleId);
    return succeeded;
}

//
// IotHub methods.
//

/**
 * @brief Uninitialize all PnP components' handler.
 */
static void ADUC_PnP_Components_Destroy()
{
    for (unsigned index = 0; index < ARRAY_SIZE(componentList); ++index)
    {
        PnPComponentEntry* entry = componentList + index;

        if (entry->Destroy != NULL)
        {
            entry->Destroy(&(entry->Context));
        }
    }
}

/**
 * @brief Refreshes the client handle associated with each of the components in the componentList
 *
 * @param clientHandle new handle to be set on each of the components
 */
static void ADUC_PnP_Components_HandleRefresh(ADUC_ClientHandle clientHandle)
{
    Log_Info("Refreshing the handle for the PnP channels.");

    const size_t componentCount = ARRAY_SIZE(componentList);

    for (size_t index = 0; index < componentCount; ++index)
    {
        PnPComponentEntry* entry = componentList + index;

        *(entry->clientHandle) = clientHandle;
    }
}

/**
 * @brief Initialize PnP component client that this agent supports.
 *
 * @param clientHandle the ClientHandle for the IotHub connection
 * @param argc Command-line arguments specific to upper-level handlers.
 * @param argv Size of argc.
 * @return bool True on success.
 */
static bool ADUC_PnP_Components_Create(ADUC_ClientHandle clientHandle, int argc, char** argv)
{
    Log_Info("Initializing PnP components.");
    bool succeeded = false;
    const unsigned componentCount = ARRAY_SIZE(componentList);

    for (unsigned index = 0; index < componentCount; ++index)
    {
        PnPComponentEntry* entry = componentList + index;

        if (!entry->Create(&entry->Context, argc, argv))
        {
            Log_Error("Failed to initialize PnP component '%s'.", entry->ComponentName);
            goto done;
        }

        *(entry->clientHandle) = clientHandle;
    }
    succeeded = true;

done:
    if (!succeeded)
    {
        ADUC_PnP_Components_Destroy();
    }

    return succeeded;
}

//
// ADUC_PnP_ComponentClient_PropertyUpdate_Callback is the callback function that the PnP helper layer invokes per property update.
//
static void ADUC_PnP_ComponentClient_PropertyUpdate_Callback(
    const char* componentName,
    const char* propertyName,
    JSON_Value* propertyValue,
    int version,
    void* userContextCallback)
{
    ADUC_PnPComponentClient_PropertyUpdate_Context* sourceContext =
        (ADUC_PnPComponentClient_PropertyUpdate_Context*)userContextCallback;

    Log_Debug("ComponentName:%s, propertyName:%s", componentName, propertyName);

    if (componentName == NULL)
    {
        // We only support named-components.
        goto done;
    }

    bool supported; supported = false;
    for (unsigned index = 0; index < ARRAY_SIZE(componentList); ++index)
    {
        PnPComponentEntry* entry = componentList + index;

        if (strcmp(componentName, entry->ComponentName) == 0)
        {
            supported = true;
            if (entry->PnPPropertyUpdateCallback != NULL)
            {
                entry->PnPPropertyUpdateCallback(
                    *(entry->clientHandle), propertyName, propertyValue, version, sourceContext, entry->Context);
            }
            else
            {
                Log_Info(
                    "Component name (%s) is recognized but PnPPropertyUpdateCallback is not specfied. Ignoring the property '%s' change event.",
                    componentName,
                    propertyName);
            }
        }
    }

    if (!supported)
    {
        Log_Info("Component name (%s) is not supported by this agent. Ignoring...", componentName);
    }

done:
    return;
}

// Note: This is an array of weak references to componentList[i].componentName
// as such the size of componentList must be equal to the size of g_modeledComponents
static const char* g_modeledComponents[ARRAY_SIZE(componentList)];

static const size_t g_numModeledComponents = ARRAY_SIZE(g_modeledComponents);

static bool g_firstDeviceTwinDataProcessed = false;

static void InitializeModeledComponents()
{
    const size_t numModeledComponents = ARRAY_SIZE(g_modeledComponents);

    STATIC_ASSERT(ARRAY_SIZE(componentList) == ARRAY_SIZE(g_modeledComponents));

    for (int i = 0; i < numModeledComponents; ++i)
    {
        g_modeledComponents[i] = componentList[i].ComponentName;
    }
}

//
// ADUC_PnP_DeviceTwin_Callback is invoked by IoT SDK when a twin - either full twin or a PATCH update - arrives.
//
static void ADUC_PnPDeviceTwin_Callback(
    DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payload, size_t size, void* userContextCallback)
{
    // Invoke PnP_ProcessTwinData to actually process the data.  PnP_ProcessTwinData uses a visitor pattern to parse
    // the JSON and then visit each property, invoking PnP_TempControlComponent_ApplicationPropertyCallback on each element.
    if (PnP_ProcessTwinData(
            updateState,
            payload,
            size,
            g_modeledComponents,
            g_numModeledComponents,
            ADUC_PnP_ComponentClient_PropertyUpdate_Callback,
            userContextCallback)
        == false)
    {
        // If we're unable to parse the JSON for any reason (typically because the JSON is malformed or we ran out of memory)
        // there is no action we can take beyond logging.
        Log_Error("Unable to process twin JSON.  Ignoring any desired property update requests.");
    }

    if (!g_firstDeviceTwinDataProcessed)
    {
        g_firstDeviceTwinDataProcessed = true;

        Log_Info("Processing existing Device Twin data after agent started.");

        const unsigned componentCount = ARRAY_SIZE(componentList);
        Log_Debug("Notifies components that all callback are subscribed.");
        for (unsigned index = 0; index < componentCount; ++index)
        {
            PnPComponentEntry* entry = componentList + index;
            if (entry->Connected != NULL)
            {
                entry->Connected(entry->Context);
            }
        }
    }
}

/**
 * @brief Scans the connection string and returns the connection type related to the string
 * @details The connection string must use the valid, correct format for the DeviceId and/or the ModuleId
 * e.g.
 * "DeviceId=some-device-id;ModuleId=some-module-id;"
 * If the connection string contains the DeviceId it is an ADUC_ConnType_Device
 * If the connection string contains the DeviceId AND the ModuleId it is an ADUC_ConnType_Module
 * @param connectionString the connection string to scan
 * @returns the connection type for @p connectionString
 */
ADUC_ConnType GetConnTypeFromConnectionString(const char* connectionString)
{
    ADUC_ConnType result = ADUC_ConnType_NotSet;

    if (connectionString == NULL)
    {
        Log_Debug("Connection string passed to GetConnTypeFromConnectionString is NULL");
        return ADUC_ConnType_NotSet;
    }

    if (ConnectionStringUtils_DoesKeyExist(connectionString, "DeviceId"))
    {
        if (ConnectionStringUtils_DoesKeyExist(connectionString, "ModuleId"))
        {
            result = ADUC_ConnType_Module;
        }
        else
        {
            result = ADUC_ConnType_Device;
        }
    }
    else
    {
        Log_Debug("DeviceId not present in connection string.");
    }

    return result;
}

/**
 * @brief Get the Connection Info from connection string, if a connection string is provided in configuration file
 *
 * @return true if connection info can be obtained
 */
bool GetConnectionInfoFromConnectionString(ADUC_ConnectionInfo* info, const char* connectionString)
{
    bool succeeded = false;

    ADUC_ConfigInfo config;
    memset(&config, 0, sizeof(config));

    if (info == NULL)
    {
        goto done;
    }
    if (connectionString == NULL)
    {
        goto done;
    }

    memset(info, 0, sizeof(*info));

    // NUVOTON: Unnecessary for no Edge Gateway connection support.
    //          Besides, stack overflow.
#if 0
    char certificateString[8192];
#endif

    if (mallocAndStrcpy_s(&info->connectionString, connectionString) != 0)
    {
        goto done;
    }

    info->connType = GetConnTypeFromConnectionString(info->connectionString);

    if (info->connType == ADUC_ConnType_NotSet)
    {
        Log_Error("Connection string is invalid");
        goto done;
    }

    info->authType = ADUC_AuthType_SASToken;

    // NUVOTON: Unnecessary for no Edge Gateway connection support.
#if 0
    // Optional: The certificate string is needed for Edge Gateway connection.
    if (ADUC_ConfigInfo_Init(&config, ADUC_CONF_FILE_PATH) && config.edgegatewayCertPath != NULL)
    {
        if (!LoadBufferWithFileContents(config.edgegatewayCertPath, certificateString, ARRAY_SIZE(certificateString)))
        {
            Log_Error("Failed to read the certificate from path: %s", config.edgegatewayCertPath);
            goto done;
        }

        if (mallocAndStrcpy_s(&info->certificateString, certificateString) != 0)
        {
            Log_Error("Failed to copy certificate string.");
            goto done;
        }

        info->authType = ADUC_AuthType_NestedEdgeCert;
    }
#endif

    succeeded = true;

done:
    ADUC_ConfigInfo_UnInit(&config);
    return succeeded;
}

/**
 * @brief Get the Connection Info from Identity Service
 *
 * @return true if connection info can be obtained
 */
bool GetConnectionInfoFromIdentityService(ADUC_ConnectionInfo* info)
{
    // NUVOTON: For no AIS support
#if 0
    bool succeeded = false;
    if (info == NULL)
    {
        goto done;
    }
    memset(info, 0, sizeof(*info));

    time_t expirySecsSinceEpoch = time(NULL) + EIS_TOKEN_EXPIRY_TIME_IN_SECONDS;

    EISUtilityResult eisProvisionResult =
        RequestConnectionStringFromEISWithExpiry(expirySecsSinceEpoch, EIS_PROVISIONING_TIMEOUT, info);

    if (eisProvisionResult.err != EISErr_Ok && eisProvisionResult.service != EISService_Utils)
    {
        Log_Info(
            "Failed to provision a connection string from eis, Failed with error %s on service %s",
            EISErr_ErrToString(eisProvisionResult.err),
            EISService_ServiceToString(eisProvisionResult.service));
        goto done;
    }

    succeeded = true;
done:

    return succeeded;
#else
    return false;
#endif
}

/**
 * @brief Gets the agent configuration information and loads it according to the provisioning scenario
 *
 * @param info the connection information that will be configured
 * @return true on success; false on failure
 */
bool GetAgentConfigInfo(ADUC_ConnectionInfo* info)
{
    bool success = false;
    ADUC_ConfigInfo config = {};
    if (info == NULL)
    {
        return false;
    }

    if (!ADUC_ConfigInfo_Init(&config, ADUC_CONF_FILE_PATH))
    {
        Log_Error("No connection string set from launch arguments or configuration file");
        goto done;
    }

    const ADUC_AgentInfo* agent; agent = ADUC_ConfigInfo_GetAgent(&config, 0);
    if (agent == NULL)
    {
        Log_Error("ADUC_ConfigInfo_GetAgent failed to get the agent information.");
        goto done;
    }

    if (strcmp(agent->connectionType, "AIS") == 0)
    {
        if (!GetConnectionInfoFromIdentityService(info))
        {
            Log_Error("Failed to get connection information from AIS.");
            goto done;
        }
    }
    else if (strcmp(agent->connectionType, "string") == 0)
    {
        if (!GetConnectionInfoFromConnectionString(info, agent->connectionData))
        {
            goto done;
        }
    }
    else
    {
        Log_Error("The connection type %s is not supported", agent->connectionType);
        goto done;
    }

    success = true;

done:
    if (!success)
    {
        ADUC_ConnectionInfo_DeAlloc(info);
    }

    ADUC_ConfigInfo_UnInit(&config);

    return success;
}

/**
 * @brief Handles the startup of the agent
 * @details Provisions the connection string with the CLI or either
 * the Edge Identity Service or the configuration file
 * @param launchArgs CLI arguments passed to the client
 * @returns bool true on success.
 */
bool StartupAgent(const ADUC_LaunchArguments* launchArgs)
{
    bool succeeded = false;

    ADUC_ConnectionInfo info = {};

    if (!ADUC_D2C_Messaging_Init())
    {
        goto done;
    }

    if (launchArgs->connectionString != NULL)
    {
        ADUC_ConnType connType = GetConnTypeFromConnectionString(launchArgs->connectionString);

        if (connType == ADUC_ConnType_NotSet)
        {
            Log_Error("Connection string is invalid");
            goto done;
        }

        ADUC_ConnectionInfo connInfo = {
            ADUC_AuthType_NotSet, connType, launchArgs->connectionString, NULL, NULL, NULL
        };

        if (!ADUC_SetDiagnosticsDeviceNameFromConnectionString(connInfo.connectionString))
        {
            Log_Error("Setting DiagnosticsDeviceName failed");
            goto done;
        }

        if (!IoTHub_CommunicationManager_Init(
                &g_iotHubClientHandle,
                ADUC_PnPDeviceTwin_Callback,
                ADUC_PnP_Components_HandleRefresh,
                &g_iotHubInitiatedPnPPropertyChangeContext))
        {
            Log_Error("IoTHub_CommunicationManager_Init failed");
            goto done;
        }
    }
    else
    {
        if (!GetAgentConfigInfo(&info))
        {
            goto done;
        }

        if (!ADUC_SetDiagnosticsDeviceNameFromConnectionString(info.connectionString))
        {
            Log_Error("Setting DiagnosticsDeviceName failed");
            goto done;
        }

        if (!IoTHub_CommunicationManager_Init(
                &g_iotHubClientHandle,
                ADUC_PnPDeviceTwin_Callback,
                ADUC_PnP_Components_HandleRefresh,
                &g_iotHubInitiatedPnPPropertyChangeContext))
        {
            Log_Error("IoTHub_CommunicationManager_Init failed");
            goto done;
        }
    }

    if (!ADUC_PnP_Components_Create(g_iotHubClientHandle, launchArgs->argc, launchArgs->argv))
    {
        Log_Error("ADUC_PnP_Components_Create failed");
        goto done;
    }

    // NUVOTON: For no DO SDK
#if 0
    ADUC_Result result;

    // The connection string is valid (IoT hub connection successful) and we are ready for further processing.
    // Send connection string to DO SDK for it to discover the Edge gateway if present.
    if (ConnectionStringUtils_IsNestedEdge(info.connectionString))
    {
        result = ExtensionManager_InitializeContentDownloader(info.connectionString);
    }
    else
    {
        result = ExtensionManager_InitializeContentDownloader(NULL /*initializeData*/);
    }

    if (IsAducResultCodeFailure(result.ResultCode))
    {
        // Since it is nested edge and if DO fails to accept the connection string, then we go ahead and
        // fail the startup.
        Log_Error("Failed to set DO connection string in Nested Edge scenario, result: 0x%08x", result.ResultCode);
        goto done;
    }
#endif

    succeeded = true;

done:

    ADUC_ConnectionInfo_DeAlloc(&info);
    return succeeded;
}

/**
 * @brief Called at agent shutdown.
 */
void ShutdownAgent()
{
    Log_Info("Agent is shutting down with signal %d.", g_shutdownSignal);
    ADUC_D2C_Messaging_Uninit();
    ADUC_PnP_Components_Destroy();
    IoTHub_CommunicationManager_Deinit();
    DiagnosticsComponent_DestroyDeviceName();
    ADUC_Logging_Uninit();
    ExtensionManager_Uninit();
}

// Global symbol referenced by the Azure SDK's port for Mbed OS, via "extern"
NetworkInterface *_defaultSystemNetwork;

//
// Main.
//

/**
 * @brief Main method.
 *
 * @return int Return value. 0 for succeeded.
 */
int main()
{
    int argc = 0;
    char** argv = NULL;

    InitializeModeledComponents();

    int ret = ParseLaunchArguments(argc, argv, &launchArgs);
    if (ret < 0)
    {
        return ret;
    }

    // Need to set ret and goto done after this to ensure proper shutdown and deinitialization.
    ADUC_Logging_Init(launchArgs.logLevel, "du-agent");

    //
    Log_Info("Application version (deviceInformation.swVersion): %s", ADUC_DEVICEINFO_SW_VERSION);

    // default to failure
    ret = 1;

    Log_Info("Agent (%s; %s) starting.", ADUC_PLATFORM_LAYER, ADUC_VERSION);
    Log_Info(
        "Supported Update Manifest version: min: %d, max: %d",
        SUPPORTED_UPDATE_MANIFEST_VERSION_MIN,
        SUPPORTED_UPDATE_MANIFEST_VERSION_MAX);

    LogInfo("Connecting to the network");

    _defaultSystemNetwork = NetworkInterface::get_default_instance();
    if (_defaultSystemNetwork == nullptr) {
        LogError("No network interface found");
        return -1;
    }

    ret = _defaultSystemNetwork->connect();
    if (ret != 0) {
        LogError("Connection error: %d", ret);
        return -1;
    }
    LogInfo("Connection success, MAC: %s", _defaultSystemNetwork->get_mac_address());

    LogInfo("Getting time from the NTP server");

    NTPClient ntp(_defaultSystemNetwork);
    ntp.set_server("time.google.com", 123);
    time_t timestamp = ntp.get_timestamp();
    if (timestamp < 0) {
        LogError("Failed to get the current time, error: %ld", (long)timestamp);
        return -1;
    }

    rtc_init();
    rtc_write(timestamp);
    time_t rtc_timestamp = rtc_read(); // verify it's been successfully updated

    LogInfo("Time: %s", ctime(&timestamp));
    LogInfo("RTC reports %s", ctime(&rtc_timestamp));

    if (!StartupAgent(&launchArgs))
    {
        goto done;
    }

    //
    // Main Loop
    //

    Log_Info("Agent running.");
    while (g_shutdownSignal == 0)
    {
        // If any components have requested a DoWork callback, regularly call it.
        for (unsigned index = 0; index < ARRAY_SIZE(componentList); ++index)
        {
            PnPComponentEntry* entry = componentList + index;

            if (entry->DoWork != NULL)
            {
                entry->DoWork(entry->Context);
            }
        }

        IoTHub_CommunicationManager_DoWork(&g_iotHubClientHandle);
        ADUC_D2C_Messaging_DoWork();

        // NOTE: When using low level samples (iothub_ll_*), the IoTHubDeviceClient_LL_DoWork
        // function must be called regularly (eg. every 100 milliseconds) for the IoT device client to work properly.
        // See: https://github.com/Azure/azure-iot-sdk-c/tree/master/iothub_client/samples
        // NOTE: For this example the above has been wrapped to support module and device client methods using
        // the client_handle_helper.h function ClientHandle_DoWork()

        ThreadAPI_Sleep(100);

#if MBED_HEAP_STATS_ENABLED || MBED_STACK_STATS_ENABLED
        // Pump host command
        {
            struct pollfd fds[1];

            fds[0].fd = STDIN_FILENO;
            fds[0].events = POLLIN;    
            int rc = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
            if ((rc > 0) && (fds[0].revents & POLLIN)) {
                int c = getchar();
                
                switch (c) {
#if (MBED_HEAP_STATS_ENABLED)
                case 'h':
                    print_heap_stats();
                    break;
#endif

#if (MBED_STACK_STATS_ENABLED)
                case 's':
                    print_stack_stats();
                    break;
#endif

                default:
                    break;
                }
            } 
        }
#endif  // MBED_HEAP_STATS_ENABLED || MBED_STACK_STATS_ENABLED
    };

    ret = 0; // Success.

done:
    Log_Info("Agent exited with code %d", ret);

    ShutdownAgent();

    return ret;
}
