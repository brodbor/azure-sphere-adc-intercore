
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <applibs/log.h>



#include <getopt.h>
#include <unistd.h>
#include  <stdio.h>

#include <applibs/eventloop.h>
#include <applibs/networking.h>
#include <applibs/application.h>


// Azure IoT SDK
#include <iothub_client_core_common.h>
#include <iothub_device_client_ll.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>
#include <iothub.h>
#include <azure_sphere_provisioning.h>
#include <iothub_security_factory.h>
#include <shared_util_options.h>

#include <sys/time.h>
#include <sys/socket.h>


#include <hw/template_appliance.h>
#include "eventloop_timer_utilities.h"


/////// Azure IoT definitions.
static char *scopeId = NULL;  // ScopeId for DPS.
static char *hostName = NULL; // Azure IoT Hub or IoT Edge Hostname.

static const char networkInterface[] = "wlan0";
static volatile sig_atomic_t terminationSignal = false;

static EventLoop *eventLoop = NULL;
static EventLoopTimer *azureTimer = NULL;
static EventLoopTimer *deviceEventTimer = NULL;

static IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle = NULL;
static int iotHubClientAuthenticated = 0;



//m4 integrations
static const char rtAppComponentId[] = "e4c61359-a32e-4206-8127-2c5735b887e6";
static int sockFd = -1;
static EventRegistration *socketEventReg = NULL;
static int m4bufferOutput =0;





static void PrepareAndSentTelemetryTimerEventHandler(EventLoopTimer *timer);
static void AzureTimerEventHandler(EventLoopTimer *timer);
static void SendTelemetry(const char *jsonMessage);
static void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context);
static bool SetUpAzureIoTHubClientWithDps(void);
static void ParseCommandLineArguments(int argc, char *argv[]);
static void SocketEventHandler(EventLoop *el, int fd, EventLoop_IoEvents events, void *context);

/// <summary>
///    Parse command  line args defined in app_manifest.json
/// </summary>
static void ParseCommandLineArguments(int argc, char *argv[])
{
    int option = 0;
    static const struct option cmdLineOptions[] = {
    
        {.name = "ScopeID", .has_arg = required_argument, .flag = NULL, .val = 's'},
        {.name = "Hostname", .has_arg = required_argument, .flag = NULL, .val = 'h'},
        {.name = NULL, .has_arg = 0, .flag = NULL, .val = 0}};

    // Loop over all of the options.
    while ((option = getopt_long(argc, argv, "s:h:", cmdLineOptions, NULL)) != -1) {
        // Check if arguments are missing. Every option requires an argument.
        if (optarg != NULL && optarg[0] == '-') {
            Log_Debug("WARNING: Option %c requires an argument\n", option);
            continue;
        }
        switch (option) {

        case 's':
            Log_Debug("ScopeID: %s\n", optarg);
            scopeId = optarg;
            break;
        case 'h':
            Log_Debug("Hostname: %s\n", optarg);
            hostName = optarg;
            break;
        default:
            // Unknown options are ignored.
            break;
        }
    }
}


/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    DisposeEventLoopTimer(deviceEventTimer);
    DisposeEventLoopTimer(azureTimer);
    EventLoop_Close(eventLoop);

    Log_Debug("INFO: Closing peripherals\n");

}

//---------------------------------------- AZURE FUNCTIONS ----------------------------------------------------------

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     with DPS
/// </summary>
static bool SetUpAzureIoTHubClientWithDps(void)
{
    AZURE_SPHERE_PROV_RETURN_VALUE provResult =
        IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning(scopeId, 10000,
                                                                          &iothubClientHandle);
    Log_Debug("IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning returned '%s'.\n",
             provResult.result);

    if (provResult.result != AZURE_SPHERE_PROV_RESULT_OK) {
        return false;
    }

    return true;
}


/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     When the SAS Token for a device expires the connection needs to be recreated
///     which is why this is not simply a one time call.
/// </summary>
static void SetUpAzureIoTHubClient(void)
{
    bool isClientSetupSuccessful = false;

    if (iothubClientHandle != NULL) {
        IoTHubDeviceClient_LL_Destroy(iothubClientHandle);
    }

     //FOR DPS
    isClientSetupSuccessful = SetUpAzureIoTHubClientWithDps();


    if (!isClientSetupSuccessful) {

        Log_Debug("ERROR: Failed to create IoTHub Handle - attempting again.\n");
        return;
    }

    //set authantication flag to 1 (SUCCESS)
    iotHubClientAuthenticated =1;

                                                 
}
/// <summary>
///     Check the network status.
/// </summary>
static bool IsConnectionReadyToSendTelemetry(void)
{
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) != 0) {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
         
            return false;
        }
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the networking stack isn't ready "
            "yet.\n");
        return false;
    }

    if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the device is not connected to "
            "the internet.\n");
        return false;
    }

    return true;
}


/// <summary>
///     Sends telemetry to Azure IoT Hub
/// </summary>
static void SendTelemetry(const char *jsonMessage)
{
    if (iotHubClientAuthenticated != 1) {
        // AzureIoT client is not authenticated. Log a warning and return.
        Log_Debug("WARNING: Azure IoT Hub is not authenticated. Not sending telemetry.\n");
        return;
    }

    Log_Debug("Sending Azure IoT Hub telemetry: %s.\n", jsonMessage);

    // Check whether the device is connected to the internet.
    if (IsConnectionReadyToSendTelemetry() == false) {
        return;
    }

    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(jsonMessage);

    if (messageHandle == 0) {
        Log_Debug("ERROR: unable to create a new IoTHubMessage.\n");
        return;
    }

    if (IoTHubDeviceClient_LL_SendEventAsync(iothubClientHandle, messageHandle, SendEventCallback, NULL) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: failure requesting IoTHubClient to send telemetry event.\n");
    } else {
        Log_Debug("INFO: IoTHubClient accepted the telemetry event for delivery.\n");
    }

    IoTHubMessage_Destroy(messageHandle);
}



/// <summary>
///     Handle socket event by reading incoming data from real-time capable application.
/// </summary>
static void SocketEventHandler(EventLoop *el, int fd, EventLoop_IoEvents events, void *context)
{
    // Read response from real-time capable application.
    // If the RTApp has sent more than 32 bytes, then truncate.
    char rxBuf[32];
    int bytesReceived = recv(fd, rxBuf, sizeof(rxBuf), 0);
    Log_Debug("SocketEventHandler\n");

    if (bytesReceived == -1) {
        Log_Debug("ERROR: Unable to receive message: %d (%s)\n", errno, strerror(errno));

    }

    Log_Debug("Received %d bytes: ", bytesReceived);
    for (int i = 0; i < bytesReceived; ++i) {
       // Log_Debug("%c", isprint(rxBuf[i]) ? rxBuf[i] : '.');
    }

    m4bufferOutput = atoi(rxBuf);
     Log_Debug(" Buffer: %d", m4bufferOutput);

    Log_Debug("\n");


}



/// <summary>
///     Callback invoked when the Azure IoT Hub send event request is processed.
/// </summary>
static void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context)
{
    Log_Debug("INFO: Azure IoT Hub send telemetry event callback: status code %d.\n", result);
}

////---------------------------------------- TIMER FUNCTIONS---------------------------------------------------------


/// <summary>
///     timer event: check ADC input voltage
/// </summary>
static void PrepareAndSentTelemetryTimerEventHandler(EventLoopTimer *timer)
{
 Log_Debug("\tINFO: Sonar timer loop\n");
         
  
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        return;
    }

          char tempAsString[50];
		  sprintf(tempAsString, "{\"Distance\": \"%d\"}", m4bufferOutput);

           //send telemetry to Azure IoT Device
	       SendTelemetry(tempAsString);
		
}

/// <summary>
///      timer event:  Check connection status and send telemetry
/// </summary>
static void AzureTimerEventHandler(EventLoopTimer *timer)
{
    Log_Debug("\tINFO: Azure timer loop\n");


    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        return;
    }

   Networking_InterfaceConnectionStatus status;

   if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) == 0) {
        if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) &&  (iotHubClientAuthenticated == 0) ) { 
            
            SetUpAzureIoTHubClient();
            
        }
    } else {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
           
            return;
        }
    }

    


    if (iothubClientHandle != NULL) {
         Log_Debug("IoTHubDeviceClient_LL_DoWork: OK \n");
        IoTHubDeviceClient_LL_DoWork(iothubClientHandle);
    }



    char tempAsString[50];
		  sprintf(tempAsString, "{\"Distance\": \"%d\"}", m4bufferOutput);

           //send telemetry to Azure IoT Device
	       SendTelemetry(tempAsString);


}


////------------------------------------------------ MAIN-------------------------------------------------------


int  main(int argc, char *argv[])
{
   ParseCommandLineArguments(argc, argv);


 // Open a connection to the RTApp.
    sockFd = Application_Connect(rtAppComponentId);
    if (sockFd == -1) {
        Log_Debug("ERROR: Unable to create socket: %d (%s)\n", errno, strerror(errno));
        terminationSignal = true;
    }


           
    eventLoop = EventLoop_Create();
    if (eventLoop == NULL) {
        Log_Debug("Could not create event loop.\n");
           terminationSignal = true;
    }


    // Register handler for incoming messages from real-time capable application.
    socketEventReg = EventLoop_RegisterIo(eventLoop, sockFd, EventLoop_Input, SocketEventHandler,
                                          /* context */ NULL);

    if (socketEventReg == NULL) {
        Log_Debug("ERROR: Unable to register socket event: %d (%s)\n", errno, strerror(errno));
           terminationSignal = true;
    }      

    struct timespec azureTelemetryPeriod = {.tv_sec = 30, .tv_nsec = 0};
    azureTimer = CreateEventLoopPeriodicTimer(eventLoop, &AzureTimerEventHandler, &azureTelemetryPeriod);
    if (azureTimer == NULL) {
            Log_Debug("ERROR: Could not run event loop.\n");
    }



   // static const struct timespec sonarCheckPeriod = {.tv_sec = 10, .tv_nsec = 0};
   // deviceEventTimer = CreateEventLoopPeriodicTimer(eventLoop, &PrepareAndSentTelemetryTimerEventHandler,
  //                                                 &sonarCheckPeriod);
  //  if (deviceEventTimer == NULL) {
  //    Log_Debug("ERROR: Could not run event loop.\n");
  //  }
    



     while (!terminationSignal) {
        EventLoop_Run_Result result = EventLoop_Run(eventLoop, -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
            terminationSignal = true;
        }
      

        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
             Log_Debug("ERROR: Loop failed\n");
              terminationSignal = true;

        }


    }

        ClosePeripheralsAndHandlers();

}




