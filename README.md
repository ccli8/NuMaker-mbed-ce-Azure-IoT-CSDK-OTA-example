# Example for Azure IoT Device Update on Nuvoton's Mbed CE enabled boards

This is an example to show [Azure IoT Device Update](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/) on Nuvoton's Mbed CE enabled boards.
It relies on the following modules:

-   [Mbed OS Community Edition](https://github.com/mbed-ce/mbed-os)
-   [Azure IoT Device SDK port for Mbed OS](https://github.com/ARMmbed/mbed-client-for-azure):
    -   [Azure IoT C SDKs and Libraries](https://github.com/Azure/azure-iot-sdk-c)
    -   [Adapters for Mbed OS](https://github.com/ARMmbed/mbed-client-for-azure/tree/master/mbed/adapters)
    -   [Azure IoT Device Update SDK](https://github.com/Azure/iot-hub-device-update)
    -   Custom step handler (a.k.a. update content handler) `mcubupdate`, integrating with MCUboot for platform layer firmware upgrade
    -   Other dependency libraries
-   [NTP client library](https://github.com/ARMmbed/ntp-client)

This example is:

-   Port of [Azure IoT Device Update Model sample](https://github.com/Azure/iot-hub-device-update/tree/main/src/agent/src),
    which implements the model [Device Update Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatemodel-2.json)
    to enable Device Update.

    **NOTE**: See [Azure IoT Device Update and Plug and Play](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-plug-and-play) for details.

-   Integrating with MCUboot for platform layer firmware upgrade.

    **NOTE**: See [MCUboot for Nuvoton's Mbed CE enabled boards](https://github.com/OpenNuvoton/mbed-mcuboot-demo/blob/nuvoton_port/NUVOTON_QUICK_START.md) for details.

## Support targets

Platform                                                                    |  Connectivity     | Bootloader        | Firmware candidate storage
----------------------------------------------------------------------------|-------------------|-------------------|--------------------------------
[NuMaker-IoT-M467](https://www.nuvoton.com/board/numaker-iot-m467/)         | Wi-Fi ESP8266     | MCUboot           | SPI flash

## Support development tools

Use cmake-based build system.
Check out [hello world example](https://github.com/mbed-ce/mbed-ce-hello-world) for getting started.

**_NOTE:_** Legacy development tools below are not supported anymore.
-   [Arm's Mbed Studio](https://os.mbed.com/docs/mbed-os/v6.15/build-tools/mbed-studio.html)
-   [Arm's Mbed CLI 2](https://os.mbed.com/docs/mbed-os/v6.15/build-tools/mbed-cli-2.html)
-   [Arm's Mbed CLI 1](https://os.mbed.com/docs/mbed-os/v6.15/tools/developing-mbed-cli.html)

For [VS Code development](https://github.com/mbed-ce/mbed-os/wiki/Project-Setup:-VS-Code)
or [OpenOCD as upload method](https://github.com/mbed-ce/mbed-os/wiki/Upload-Methods#openocd),
install below additionally:

-   [NuEclipse](https://github.com/OpenNuvoton/Nuvoton_Tools#numicro-software-development-tools): Nuvoton's fork of Eclipse
-   Nuvoton forked OpenOCD: Shipped with NuEclipse installation package above.
    Checking openocd version `openocd --version`, it should fix to `0.10.022`.

## Developer guide

This chapter is intended for developers to get started, import the example application, build, and get it running as Azure IoT Device Update enabled device.

**NOTE**: The following command lines are verified on Windows git-bash environment.
For other shell environments, check [Inline JSON help](https://github.com/Azure/azure-iot-cli-extension/wiki/Inline-JSON-help)
on how different shells use line continuation, quotation marks, and escapes characters differently.

### Hardware requirements

-   [NuMaker-IoT-M467 board](https://os.mbed.com/platforms/NUMAKER-IOT-M467/)

### Software requirements

-   [Image tool](https://github.com/mcu-tools/mcuboot/blob/main/docs/imgtool.md)

    **NOTE**: Used for signing application binary for MCUboot

-   [SRecord](http://srecord.sourceforge.net/)

    **NOTE**: Used for concatenating MCUboot bootloader binary and application binary

-   [Azure CLI](https://learn.microsoft.com/en-us/cli/azure/)

    **NOTE**: You need to also install `azure-iot` extension:
    ```
    $ az extension add --name azure-iot
    ```

    **NOTE**: Used for creating
    [Device Update import manifest](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/create-update#create-a-basic-device-update-import-manifest).

### Hardware setup

-   Firmware candidate storage
    -   NuMaker-IoT-M467: On-board SPI flash
-   Switch target board
    -   NuMaker-IoT-M467's Nu-Link2: TX/RX/VCOM to ON, MSG to non-ON
-   Connect target board to host through USB
    -   NuMaker-IoT-M467: Mbed USB drive shows up in File Browser

### Azure portal setup

-   Sign in to [Azure portal](https://portal.azure.com/).

-   Create one [IoT Hub and Device using symmetric key authentication](https://learn.microsoft.com/en-us/azure/iot-hub/iot-hub-create-through-portal).

-   Create one [Device Update account and instance](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/create-device-update-account?tabs=portal).

-   Create one [Storage account](https://learn.microsoft.com/en-us/azure/storage/common/storage-account-create?toc=%2Fazure%2Fstorage%2Fblobs%2Ftoc.json&bc=%2Fazure%2Fstorage%2Fblobs%2Fbreadcrumb%2Ftoc.json&tabs=azure-portal)
    and [container](https://learn.microsoft.com/en-us/azure/storage/blobs/blob-containers-portal).
    
**NOTE**: Make sure you have completed [configuring access control roles](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/configure-access-control-device-update?tabs=portal).

### Build the example

In the following, we take NuMaker-IoT-M467 as example board to show this example.

1.  Clone the example and navigate into it
    ```
    $ git clone https://github.com/mbed-nuvoton/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example
    $ cd NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example
    $ git checkout -f master
    ```

1.  Deploy necessary libraries
    ```
    $ git submodule update --init
    ```
    Or for fast install:
    ```
    $ git submodule update --init --filter=blob:none
    ```

    Deploy further for `mbed-ce-client-for-azure` library:
    ```
    $ cd mbed-ce-client-for-azure; \
    git submodule update --init; \
    cd ..
    ```
    Or for fast install:
    ```
    $ cd mbed-ce-client-for-azure; \
    git submodule update --init --filter=blob:none; \
    cd ..
    ```

1.  Configure network interface
    -   Ethernet: Need no further configuration.

        **mbed_app.json5**:
        ```json5
        "target.network-default-interface-type" : "Ethernet",
        ```

    -   WiFi: Configure WiFi `SSID`/`PASSWORD`.

        **mbed_app.json5**:
        ```json5
        "target.network-default-interface-type" : "WIFI",
        "nsapi.default-wifi-security"           : "WPA_WPA2",
        "nsapi.default-wifi-ssid"               : "\"SSID\"",
        "nsapi.default-wifi-password"           : "\"PASSWORD\"",
        ```

1.  In `configs/aduc_user_config.h`, provide relevant information.

    **NOTE**: To meet the following build command lines, just provide your device connection string acquired above to `ADUC_DEVICE_CONNECTION_STRING`,
    and make no other modifications.

1.  Build application of first version `1.0.0` as base.

    **NOTE**: We will see application related versions appear in several places.
    Even though they are managed separately and can be different for the same application binary,
    make them the same for consistency.
    -   Software version in [Device information interface](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-agent-overview#device-information-interface)
    -   Application version in [imgtool sign](https://docs.mcuboot.com/imgtool.html#signing-images) for MCUboot
    -   [Update ID version in import manifest](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/import-schema#updateid-object)
    -   Installed criteria for the custom step handler `mcubupdate`

    1.  In `configs/aduc_user_config.h`, set `ADUC_DEVICEINFO_SW_VERSION` to `1.0.0`.

    1.  Compile with cmake/ninja
        ```
        $ mkdir build; cd build
        $ cmake .. -GNinja -DCMAKE_BUILD_TYPE=Develop -DMBED_TARGET=NUMAKER_IOT_M467 -DMCUBOOT_SIGNING_KEY="bootloader/MCUboot/signing-keys.pem" -DMBED_USE_SHALLOW_SUBMODULES=FALSE
        $ ninja
        $ cd ..
        ```

    1.  Append version suffix `_V1.0.0` to the built image file name for distinct from below.
        ```
        $ mv \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example.bin \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.0.bin
        ```

    1.  Sign application binary of first version `1.0.0`
        ```
        $ imgtool sign \
        -k bootloader/MCUboot/signing-keys.pem \
        --align 4 \
        -v 1.0.0+0 \
        --header-size 4096 \
        --pad-header \
        -S 0xE6000 \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.0.bin \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.0_signed.bin
        ```

        **NOTE**: This file will be combined with MCUboot bootloader later.

        **NOTE**: For consistency, set application version for MCUboot to `1.0.0+0`.

        **NOTE**: `-S 0xE6000` is to specify MCUboot primary/secondary slot size.

    1.  Combine MCUboot bootloader binary and signed application binary of first version `1.0.0`
        ```
        $ srec_cat \
        bootloader/MCUboot/mbed-ce-mcuboot-bootloader_m467-iot_spif.bin -Binary -offset 0x0 \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.0_signed.bin -Binary -offset 0x10000 \
        -o build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_merged.hex -Intel
        ```

        **NOTE**: The combined file `NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_merged.hex` is for flash later.

        **NOTE**: `-offset 0x0` is to specify start address of MCUboot bootloader.

        **NOTE**: `-offset 0x10000` is to specify start address of primary slot where active application binary is located.

1.  Rebuild application of second version `1.0.1` for update.

    1.  In `configs/aduc_user_config.h`, set `ADUC_DEVICEINFO_SW_VERSION` to `1.0.1`.

    1.  Re-compile with cmake/ninja
        ```
        $ cd build
        $ ninja
        $ cd ..
        ```

    1.  Append version suffix `_V1.0.1` to the built image file name for distinct from above.
        ```
        $ mv \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example.bin \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1.bin
        ```

    1.  Sign application binary of second version `1.0.1`
        ```
        $ imgtool sign \
        -k bootloader/MCUboot/signing-keys.pem \
        --align 4 \
        -v 1.0.1+0 \
        --header-size 4096 \
        --pad-header \
        -S 0xE6000 \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1.bin \
        build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin
        ```

        **NOTE**: For consistency, set application version for MCUboot to `1.0.1+0`.

        **NOTE**: This file is for upload to Azure Device Update later.

    1.  Generate Azure Device Update import manifest
        ```
        $ az iot du update init v5 \
        --update-provider Nuvoton \
        --update-name NuMaker-ADU-Sample-Update \
        --update-version 1.0.1 \
        --compat manufacturer=Nuvoton model=NuMaker-ADU-Sample-Device \
        --step handler=nuvoton/mcubupdate:1 properties='{"installedCriteria": "1.0.1"}' \
        --file path="./build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin" \
        > Nuvoton_NuMaker-ADU-Sample-Update_1.0.1.importmanifest.json
        ```

        **NOTE**: For consistency, set `--update-version` to `1.0.1`.

        **NOTE**: If changed, `--compat` must match `ADUC_COMPAT_PROPERTY_NAMES`, `ADUC_DEVICEPROPERTIES_MANUFACTURER`, and `ADUC_DEVICEPROPERTIES_MODEL`
        in above `configs/aduc_user_config.h`.

        **NOTE**: Continuing above, `manufacturer`/`model` are the common
        [compatibility properties](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/import-concepts#compatibility).

        **NOTE**: `nuvoton/mcubupdate:1` is the custom step handler,
        integrating with MCUboot for platform layer firmware upgrade.

        **NOTE**: For consistency, set `installedCriteria` to `1.0.1`.

        **NOTE**: An import manifest JSON file name must end with `.importmanifest.json` when imported through Azure portal.
        See [Create an import manifest](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/import-concepts#create-an-import-manifest).

        **NOTE**: This file is for upload to Azure Device Update later.

### Flashing the image

Flash the first version `1.0.0` by drag-n-drop'ing the built image file generated above onto **NuMaker-IoT-M467** board

`build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_merged.hex`

### Operations on Azure portal

1.  [Import an update to Device Update](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/import-update?tabs=portal).

    The files generated above are for import:
    -   `Nuvoton_NuMaker-ADU-Sample-Update_1.0.1.importmanifest.json`
    -   `build/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin`

    **NOTE**: The import process can take a few minutes to complete.

1.  [Deploy the update](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/deploy-update?tabs=portal#deploy-the-update)

    **NOTE**: The device must have had connected with IoT Hub so that
    default or created [device group](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/create-update-group?tabs=portal) can automatically generate.

1.  [Monitor the deployment](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/deploy-update?tabs=portal#monitor-an-update-deployment)

### Monitoring the application through host console

Configure host terminal program with **115200/8-N-1**, and you should see log similar to below:

On first boot, MCUboot does no firmware upgrade before update deployment:
```
[INFO][BL]: Starting MCUboot
[INFO][MCUb]: Primary image: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
[INFO][MCUb]: Scratch: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
[INFO][MCUb]: Boot source: primary slot
[INFO][MCUb]: Swap type: none
[INFO][BL]: Booting firmware image at 0x11000
```

Application version before update deployment should be `1.0.0`:
```
Info: Application version (deviceInformation.swVersion): 1.0.0
```

Connect to Azure IoT Hub:
```
Info: Attempting to create connection to IotHub using type: ADUC_ConnType_Device
Info: IotHub Protocol: MQTT
Info: IoTHub Device Twin callback registered.
Info: Refreshing the handle for the PnP channels.
Info: Successfully re-authenticated the IoT Hub connection.
-> 1:36:11 CONNECT | VER: 4 | KEEPALIVE: 240 | FLAGS: 192 | USERNAME: hub-002.azure-devices.net/device-symm-002/?api-version=2020-09-30&DeviceClientType=iothubclient%2f1.9.1%20(native%3b%20mbedOS5%3b%20undefined)&model-id=dtmi%3aazure%3aiot%3adeviceUpdateModel%3b2 | PWD: XXXX | CLEAN: 0
Info: The connection is currently broken. Will try to authenticate in 17 seconds.
<- 1:36:12 CONNACK | SESSION_PRESENT: true | RETURN_CODE: 0x0
Info: IotHub connection status: 0, reason: 6
```

To print heap usage, press **h**:
```
** MBED HEAP STATS **
**** current_size   : 44019
**** max_size       : 55584
**** reserved_size  : 472880
```

To print stack usage, press **s**:
```
** MBED THREAD STACK STATS **
Thread: 0x20008fd4, Stack size: 16384, Max stack: 10240
Thread: 0x20008f4c, Stack size: 512, Max stack: 72
Thread: 0x20008f90, Stack size: 768, Max stack: 96
Thread: 0x20003ce8, Stack size: 2048, Max stack: 608
*****************************
```

Device will receive update action `applyDeployment` after you start update deployment on Azure portal:
```
Info: Update Action info string ({"workflow":{"action":3.00000000000000000,"id":"9b410934-b4e2-49d9-aa71-b40bf2265557"},"updateManifest":"{\"manifestVersion\":\"5\",\"updateId\":{\"provider\":\"Nuvoton\",\"name\":\"NuMaker-ADU-Sample-Update\",\"version\":\"1.0.1\"},\"compatibility\":[{\"manufacturer\":\"Nuvoton\",\"model\":\"NuMaker-ADU-Sample-Device\"}],\"instructions\":{\"steps\":[{\"handler\":\"nuvoton\/mcubupdate:1\",\"files\":[\"f7c99108ce676353f\"],\"handlerProperties\":{\"installedCriteria\":\"1.0.1\"}}]},\"files\":{\"f7c99108ce676353f\":{\"fileName\":\"NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin\",\"sizeInBytes\":629688,\"hashes\":{\"sha256\":\"CWPiC5Z8plEOecGF+tASNnqOXWD71A+oJJIBhSt8j8I=\"}}},\"createdDateTime\":\"2023-03-15T09:55:47Z\"}","updateManifestSignature":null,"fileUrls":null}), property version (21)
......
Info: Processing current step:
{
    "handler": "nuvoton\/mcubupdate:1",
    "files": [
        "f7c99108ce676353f"
    ],
    "handlerProperties": {
        "installedCriteria": "1.0.1"
    }
}
```

Download started:
```
Info: workflow is not completed. AutoTransition to step: Download
Info: Processing 'Download' step
......
Info: Loading handler for step #0 (handler: 'nuvoton/mcubupdate:1')
Info: Loading Update Content Handler for 'nuvoton/mcubupdate:1'.
Info: Determining contract version for 'nuvoton/mcubupdate:1'.
Info: Caching new content handler for 'nuvoton/mcubupdate:1'.
Info: No installed criteria settled down. Maybe it is the first time for ADU.
Info: Upgrade firmware: FileId f7c99108ce676353f
Info: Upgrade firmware: DownloadUri http://hub-002--hub-002.b.nlu.dl.adu.microsoft.com/eastus/hub-002--hub-002/0b8713a8590744e09b74e74d479828ef/NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin
Info: Upgrade firmware: TargetFilename NuMaker-mbed-ce-Azure-IoT-CSDK-OTA-example_V1.0.1_signed.bin
Info: Upgrade firmware: SizeInBytes 629688
Info: Secondary BlockDevice size: 942080 (bytes)
......
Info: Active image version: 1.0.0+0
Info: HTTP download: 0/629688
Info: Image header: padded header size=4096, image size=625256, protected TLV size=0
Info: Stage image version: 1.0.1+0
Info: HTTP download: 1209/629688
Info: HTTP download: 3257/629688
Info: HTTP download: 5305/629688
```

Download completed:
```
Info: HTTP download: 624981/629688
Info: HTTP download: 627029/629688
Info: HTTP download: 629077/629688
Info: HTTP download: Completed 629688/629688 bytes
Info: Steps_Handler Download end (level 0).
Info: Action 'Download' complete. Result: 500 (succeeded), 0 (0x0)
```

After Apply completed, device will reboot:
```
Info: Action 'Apply' complete. Result: 700 (succeeded), 0 (0x0)
Info: Apply indicated success with RebootRequired - rebooting system now
Info: Calling ADUC_RebootSystem
Info: ADUC_RebootSystem called. Rebooting system.
```

On second boot, MCUboot does firmware upgrade after update deployment:
```
[INFO][BL]: Starting MCUboot
[INFO][MCUb]: Primary image: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
[INFO][MCUb]: Scratch: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
[INFO][MCUb]: Boot source: primary slot
[INFO][MCUb]: Swap type: test
[INFO][MCUb]: Starting swap using scratch algorithm.
[INFO][BL]: Booting firmware image at 0x11000
```

Application version after update deployment should be `1.0.1`:
```
Info: Application version (deviceInformation.swVersion): 1.0.1
```

Re-connect to Azure IoT Hub:
```
Info: Attempting to create connection to IotHub using type: ADUC_ConnType_Device
Info: IotHub Protocol: MQTT
Info: IoTHub Device Twin callback registered.
Info: Refreshing the handle for the PnP channels.
Info: Successfully re-authenticated the IoT Hub connection.
-> 1:43:28 CONNECT | VER: 4 | KEEPALIVE: 240 | FLAGS: 192 | USERNAME: hub-002.azure-devices.net/device-symm-002/?api-version=2020-09-30&DeviceClientType=iothubclient%2f1.9.1%20(native%3b%20mbedOS5%3b%20undefined)&model-id=dtmi%3aazure%3aiot%3adeviceUpdateModel%3b2 | PWD: XXXX | CLEAN: 0
Info: The connection is currently broken. Will try to authenticate in 17 seconds.
<- 1:43:28 CONNACK | SESSION_PRESENT: true | RETURN_CODE: 0x0
Info: IotHub connection status: 0, reason: 6
```

## Customizing the application

The chapter lists advanced topics on customizing the application.

### Adjusting the bootloader

This example uses MCUboot as bootloader.
You can make the following adjustments:

-   Recreate the prebuilt bootloader binary `mbed-mcuboot-bootloader_m467-iot_spif.bin`.

    Follow [MCUboot for Nuvoton's Mbed CE enabled boards](https://github.com/OpenNuvoton/mbed-mcuboot-demo/blob/nuvoton_port/NUVOTON_QUICK_START.md),
    with target name being `NUMAKER_IOT_M467_SPIF`

-   Change attached signing keys `signing-keys.pem` and `signing_keys.c` for production.

    1.  Follow [change signing keys](https://github.com/OpenNuvoton/mbed-mcuboot-demo/blob/nuvoton_port/NUVOTON_QUICK_START.md#changing-signing-keys).
        In `mbed-mcuboot-demo`, recreate signing keys and then recreate bootloader binary.

        **NOTE**: Ignore the `mbed-mcuboot-blinky` part.

    1.  In this example, replace the prebuilt bootloader binary `mbed-mcuboot-bootloader_m467-iot_spif.bin`
        with the recreated one above.

    1.  In this example. synchronize updated signing keys in `mbed-mcuboot-demo`.

-   Change firmware candidate storage.

    1.  Follow [support new target](https://github.com/OpenNuvoton/mbed-mcuboot-demo/blob/nuvoton_port/NUVOTON_QUICK_START.md#support-new-target).
        In `mbed-mcuboot-demo`, re-implement `get_secondary_bd()` and then recreate bootloader binary.

        **NOTE**: Ignore the `mbed-mcuboot-blinky` part.

    1.  In this example, replace the prebuilt bootloader binary `mbed-mcuboot-bootloader_m467-iot_spif.bin`
        with the recreated one above.

    1.  In this example, remove default `get_secondary_bd()` following below and
        replace with the re-implemented `get_secondary_bd()` in `mbed-mcuboot-demo`.
        The re-implemented `get_secondary_bd()` can be placed anywhere, for example, in `main.cpp`.

        **mbed_app.json5**:
        ```json5
        "azure-client-ota-mcuboot.provide-default-secondary-blockdevice"    : null,
        ```

### Providing your own device model

To provide your own device model enabling Device Update,
according to [ability to support custom model Id](https://github.com/Azure/iot-hub-device-update/issues/230),
you must follow the rules:

-   Extend [Device Update Base Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatecontractmodel-2.json)
    or
    [Device Update Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatemodel-2.json)
    to define your device model.

-   [Publish](https://learn.microsoft.com/en-us/azure/iot-develop/concepts-modeling-guide#publish) this device model.
    Private device model is not supported.

    **NOTE**: Published [Temperature Controller](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/com/example/temperaturecontroller-4.json),
    which extends [Device Update Base Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatecontractmodel-2.json),
    can be used to quickly evaluate the implementation of custom device model.

Based on this example code, to implement the custom device model, you need to:

1.  In `configs/aduc_user_config.h`, override `ADUC_DEVICE_MODEL_ID` with the custom device model ID.

1.  In the supported component list below,
    1.  Keep `deviceUpdate`.
    1.  Keep `deviceInformation` if the custom device model supports
        [Device Information](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/devicemanagement/deviceinformation-1.json)
        on its own, even though it just extends
        [Device Update Base Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatecontractmodel-2.json).
    1.  Keep `deviceInformation` and `diagnosticInformation` if the custom device model extends
        [Device Update Model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/deviceupdatemodel-2.json).
    1.  Add other components of your own.

    ```
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
    ```

1.  To implement other components of your own, reference
    [deviceInformation](https://github.com/Azure/iot-hub-device-update/tree/1.0.2/src/agent/device_info_interface)
    as starting point.

    **NOTE**: For messaging, this intrinsic component invokes inbuilt [ADUC_D2C_Message API](https://github.com/Azure/iot-hub-device-update/blob/1.0.2/src/utils/d2c_messaging/inc/aduc/d2c_messaging.h),
    which doesn't support extrinsic components.
    Use
    [ClientHandle API](https://github.com/Azure/iot-hub-device-update/blob/1.0.2/src/communication_abstraction/inc/aduc/client_handle_helper.h)
    for extrinsic components instead.

## Limitations

-   Support no [Device Provisioning Service](https://learn.microsoft.com/en-us/azure/iot-dps/).

    Currently, support only connecting straight with IoT Hub.

-   Support no [Diagnostics](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-log-collection?tabs=portal).

    Dummy implementation of
    [Diagnostic Information Interface](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/azure/iot/diagnosticinformation-1.json)
    is provided to meet resource constrained device.

-   Support no [reference steps](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-multi-step-updates#parent-updates-and-child-updates)
    and no [detached update manifests](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-multi-step-updates#detached-update-manifests).

    The above need file system to place extra downloaded update manifests.
    Support only inline step and not large update manifest.

-   Support no other step handlers

    Support only custom step handler `mcubupdate` to meet target device capability.
    Others like `apt`, `script`, and `swupdate` are not supported.

## Trouble-shooting

-   In Device Update,
    [device group](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-groups)
    or registered device doesn't show up.

    Make sure the device has connected with IoT Hub at least one time for reporting its Device Update capability,
    and then
    [initiate a device sync operation](https://learn.microsoft.com/en-us/azure/iot-hub-device-update/device-update-agent-check?tabs=portal#initiate-a-device-sync-operation).

## Known issues

-   On Azure portal, device is not listed during deployment.

    The issue is being tracked via the [ticket](https://github.com/Azure/iot-hub-device-update/issues/421).

-   Continuing above, after reboot, device will halt at the point below on reconnecting to IoT Hub.
    ```
    Info: Attempting to create connection to IotHub using type: ADUC_ConnType_Device
    Info: IotHub Protocol: MQTT
    Info: IoTHub Device Twin callback registered.
    Info: Refreshing the handle for the PnP channels.
    Info: Successfully re-authenticated the IoT Hub connection.
    -> 1:43:28 CONNECT | VER: 4 | KEEPALIVE: 240 | FLAGS: 192 | USERNAME: hub-002.azure-devices.net/device-symm-002/?api-version=2020-09-30&DeviceClientType=iothubclient%2f1.9.1%20(native%3b%20mbedOS5%3b%20undefined)&model-id=dtmi%3aazure%3aiot%3adeviceUpdateModel%3b2 | PWD: XXXX | CLEAN: 0
    Info: The connection is currently broken. Will try to authenticate in 17 seconds.
    <- 1:43:28 CONNACK | SESSION_PRESENT: true | RETURN_CODE: 0x0
    Info: IotHub connection status: 0, reason: 6
    -> 1:43:28 SUBSCRIBE | PACKET_ID: 2 | TOPIC_NAME: $iothub/twin/res/# | QOS: 0
    <- 1:43:28 SUBACK | PACKET_ID: 2 | RETURN_CODE: 0
    -> 1:43:28 PUBLISH | IS_DUP: false | RETAIN: 0 | QOS: DELIVER_AT_MOST_ONCE | TOPIC_NAME: $iothub/twin/GET/?$rid=3
    ```

    This issue is unclear with Device Update service.
    Canceling and/or removing the active deployment can recover to normal situation.