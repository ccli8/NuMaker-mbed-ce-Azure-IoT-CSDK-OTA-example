/**
 * @file aduc_config.h
 * @brief Azure Update Client configuration
 *
 * @copyright Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 */

#ifndef ADUC_CONFIG_H_
#define ADUC_CONFIG_H_

/**
 * @brief Manufacturer of the device (DeviceInfo configuration)
 */
#define ADUC_DEVICEINFO_MANUFACTURER        "Nuvoton"

/**
 * @brief Name of the device (DeviceInfo configuration)
 */
#define ADUC_DEVICEINFO_MODEL               "NuMaker-ADU-Sample-Device"

/**
 * @brief Software version (DeviceInfo configuration)
 */
#define ADUC_DEVICEINFO_SW_VERSION          "1.0.0"

/**
 * @brief ADU agent name
 */
#define ADUC_AGENT_NAME                     "NuMaker-ADU-Sample-Agent"

/**
 * @brief These properties are used to check for compatibility of the device
 *        to target the update deployment
 */
#define ADUC_COMPAT_PROPERTY_NAMES          "manufacturer,model"

/**
 * @brief Manufacturer of the device (AzureDeviceUpdateInterface DeviceProperties configuration)
 */
#define ADUC_DEVICEPROPERTIES_MANUFACTURER  "Nuvoton"

/**
 * @brief Name of the device (AzureDeviceUpdateInterface DeviceProperties configuration)
 */
#define ADUC_DEVICEPROPERTIES_MODEL         "NuMaker-ADU-Sample-Device"

/**
 * @brief Device or module connection string
 */
#define ADUC_DEVICE_CONNECTION_STRING       "HostName..."

/**
 * @brief The Device Twin Model Identifier.
 * This model must contain 'azureDeviceUpdateAgent' and 'deviceInformation' sub-components.
 *
 * Customers should change this ID to match their device model ID.
 */
#define ADUC_DEVICE_MODEL_ID                "dtmi:azure:iot:deviceUpdateModel;2"

#endif /* ifndef DEMO_CONFIG_H */
