/* Copyright (c) Codethink Ltd. All rights reserved.
   Licensed under the MIT License. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#include "lib/CPUFreq.h"
#include "lib/VectorTable.h"
#include "lib/NVIC.h"
#include "lib/GPIO.h"
#include "lib/GPT.h"
#include "lib/UART.h"
#include "lib/Print.h"
#include "lib/ADC.h"

#include "lib/mt3620/gpt.h"

#include "Socket.h"


static UART   *debug   = NULL;
static Socket *socket  = NULL;
static const int saveDelay = 10000;

static __attribute__((section(".sysram"))) uint32_t rawData[8];
static ADC_Data data[8];





/// <summary>
///     placeholder funcion to handle ADC connectivity errors
/// </summary>
static void callback(int32_t status)
{
}

/// <summary>
///     placeholder function to handle socket connectivity errors///
/// </summary>
static void handleRecvMsgWrapper(Socket *handle)
{
}


/// <summary>
///     write to innercore buffer
/// </summary>
static void HandleTimer(void)
{
    static const Component_Id A7ID =
    {
        .seg_0   = 0xed4e667a,
        .seg_1   = 0xce81,
        .seg_2   = 0x448b,
        .seg_3_4 = {0xaa, 0x64, 0x85, 0xd2, 0x80, 0x83, 0x55, 0x9e}
    };


        ///convert int to  char array
        int mV = (data[0].value);
        
        int n =  mV;
        int count = 0;
        while (n != 0) {
                n /= 10;  
                ++count;
            }
        char msg[count];


        for (int i = (count-1); i >= 0; i--)
        {
            msg[i] = '0' + (mV % 10);
            mV /= 10;
        }

        int32_t error = Socket_Write(socket, &A7ID, msg, sizeof(msg));        

    if (error != ERROR_NONE) {
        UART_Printf(debug, "ERROR: sending msg %s - %ld\r\n", msg, error);
    }     

}


////------------------------------------------------ MAIN-------------------------------------------------------


_Noreturn void RTCoreMain(void)
{
    VectorTableInit();
    CPUFreq_Set(26000000);

    debug = UART_Open(MT3620_UNIT_UART_DEBUG, 115200, UART_PARITY_NONE, 1, NULL);

    UART_Print(debug, "----------------------------------------\r\n");
    UART_Print(debug, "App built on: " __DATE__ ", " __TIME__ "\r\n");



 // Setup socket
    socket = Socket_Open(handleRecvMsgWrapper);
    if (!socket) {
        UART_Printf(debug, "ERROR: socket initialisation failed\r\n");
    }

    //Initialise ADC driver, and then configure it to use channel 0
    AdcContext *handle = ADC_Open(MT3620_UNIT_ADC0);
    if (ADC_ReadPeriodicAsync(handle, &callback, 8, data, rawData,
        0xF, 1000, 2500) != ERROR_NONE) {
        UART_Print(debug, "Error: Failed to initialise ADC.\r\n");
    }


 
    GPT  *tt = NULL;
    if (!( tt = GPT_Open(MT3620_UNIT_GPT1, 1000, GPT_MODE_REPEAT))) {
        UART_Print(debug, "ERROR: Opening timer\r\n");
    }

    GPT_StartTimeout(tt, saveDelay, GPT_UNITS_MILLISEC, &HandleTimer) ;

    for (;;) {
        __asm__("wfi");
    }

}