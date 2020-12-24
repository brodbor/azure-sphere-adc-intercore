# Azure Sphere inter-core communication: integrating real-time Cortex-M4 with high-level Cortex-A7 app to send telemetry data to Azure IoT Hub  

In this tutorial, we will build real-time Cortex M4 application to capture Ultrasonic telemetry data over AD input, transfer data to high-level app, running on top of Cortex-A7 core and transfer data to Azure IoT Hub.

![](https://borisbrodsky.com/wp-content/uploads/2020/12/MT3620-DATA-FLOW.png)

## Hardware Used
1. Seeed [MT3620](https://www.seeedstudio.com/Azure-Sphere-MT3620-Development-Kit-US-Version-p-3052.html) Azure Sphere Board
2. Sonar Sensor-[LV-MaxSonar-EZ1 Ultrasonic Range Finder](https://www.amazon.com/Maxbotix-MB1010-LV-MaxSonar-EZ1-Ultrasonic-Finder/dp/B00A7YGVJI)
3. Visual Studio Code with number of extensions. Please check this [guide](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-arduino-iot-devkit-az3166-get-started) for additional details
4. Troubleshoot real-time app output -[Adafruit FTDI Friend](https://www.amazon.com/Adafruit-FTDI-Friend-Extras-ADA284)

##  Azure Sphere - Quick Getting Started
1. You'll need Azure Account and a small budget to run IoT Hub. (device provisioning service charges $0.123 per 1000 operations)
2. Install the [Azure Sphere SDK](https://aka.ms/AzureSphereSDKDownload/Windows)
3. Install [USB Serial Converters](https://docs.microsoft.com/en-us/azure-sphere/install/install-sdk?pivots=visual-studio)
4. Visual Studio Code 
   - For VS Code, Install [CMake](https://cmake.org/download/) and [Ninja on Windows](https://github.com/ninja-build/ninja/releases)
   - Add the CMake bin directory and the directory containing ninja.exe to your PATH
   - Add VS Code extension:  Azure Sphere
5. Claim your device. First time user: [click for more details](https://docs.microsoft.com/en-us/azure-sphere/install/claim-device)
   - ```azsphere login --newuser <email-address>```
   - Connect an Azure Sphere device to your computer by USB.
   - Create tenant:  ```azsphere tenant create --name <my-tenant>```
   - ```azsphere device claim```    ***Important:*** device can only be claimed once. Please check this [article](https://docs.microsoft.com/en-us/azure-sphere/install/claim-device) for more details
   
   
## Configure Device
1. Configure Network: ```azsphere device wifi add --ssid <NAME> --psk <PASS>```
2. Test connection: ```azsphere device wifi show-status```

##  Wiring Diagram
![](https://borisbrodsky.com/wp-content/uploads/2020/12/MT3620-WIRING.png)

### Configure Azure IoT Hub
Create an IoT Hub and a device provisioning service. Follow stpes 1-5 in this [tutorial](https://docs.microsoft.com/en-us/learn/modules/develop-secure-iot-solutions-azure-sphere-iot-hub/8-exercise-connect-room-environment-monitor) 

###  Code Configuration
1.	To setup up troubleshooting capabilities for real-time application you’ll need to enable TX-only UART capabilities. Putty or Termite can be used  to establish a serial connection with 115200-8-N-1 terminal settings
2.	In order to load high-level and real-time app to respective cores, we will need to mark application as Partner in launch.json of high-level and real-time (update if you change ComponentId values):
    - Update “AllowedApplicationConnections” to enable communication between cores
3.	Enable Buffer Read/Write (update if you change ComponentId values):
    - Enable real-time application access to buffer. Update offset (0x) with new values, if Component_Id changed from the default value.
      ```
      static const Component_Id A7ID =
          {
              .seg_0   = 0xede667a,
              .seg_1   = 0xce81,
              .seg_2   = 0x448b,
              .seg_3_4 = {0xaa, 0x64, 0x85, 0xd2, 0x80, 0x83, 0x55, 0x9e}
          };
      ```
    - Enable high-level application read from buffer:
      ```
          sockFd = Application_Connect("e4c61359-a32e-4206-8127-2c5735b887e6");
      ```
5.	Update high-level app app_manifest.json CmdArgs with Azure Portal values
    - ScopeID –  navigate to Azure Portal and select DPS, copy ID Scope value
    - HostName – Navigate to Azure Portal and select IoT Hub,  copy hostname value
