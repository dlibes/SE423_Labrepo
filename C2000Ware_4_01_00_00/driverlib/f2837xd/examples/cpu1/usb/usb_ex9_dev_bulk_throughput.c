//##############################################################################
//
// FILE:   usb_ex9_dev_bulk_throughput.c
//
// TITLE:  Main routines for the generic bulk device example.
//
//! \addtogroup driver_example_list
//! <h1>USB Throughput Bulk Device Example (usb_ex9_throughput_dev_bulk)</h1>
//!
//! This example provides a throughput numbers of bulk data transfer to and
//! from the host. The device uses a vendor-specific class ID and supports
//! a single bulk IN Endpoint and a single bulk OUT Endpoint.
//!
//! SCIA, connected to the FTDI virtual COM port and running at 115200,
//! 8-N-1, is used to display messages from this application.
//!
//! A Windows INF file for the device is provided under the windows drivers
//! directory. This INF contains information required to install the WinUSB
//! subsystem on WindowsXP, Windows 7 and Windows 10. This is present in 
//! utilities/windows_drivers.
//!
//! A sample Windows command-line application, usb_throughput_bulk_example,
//! illustrating how to connect to and communicate with the bulk device is
//! also provided. Project files are included to allow the examples to be
//! built using Microsoft VisualStudio. Source code for this application can
//! be found in directory
//! ~/C2000Ware/utilities/tools/usb_throughput_bulk_example/Release.
//!
//! After running the example in CCS Connect the USB Micro to the PC. Then the
//! example will wait to receive data from the application. Run the
//! usb_throughput_bulk example, the throughput and Data Packets Transferred.
//!
//
//##############################################################################
//
// $Release Date: $
// $Copyright:
// Copyright (C) 2013-2022 Texas Instruments Incorporated - http://www.ti.com/
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//##############################################################################

//
// Included Files
//
#include "driverlib.h"
#include "device.h"
#include "usb_hal.h"
#include "usblib.h"
#include "usb_ids.h"
#include "device/usbdevice.h"
#include "device/usbdbulk.h"
#include "scistdio.h"
#include "usb_ex9_bulk_structs.h"

//
// Defines
//

//
// The system tick rate expressed both as ticks per second and a millisecond
// period.
//
#define TICKS_PER_SECOND     1000

#define COMMAND_PACKET_RECEIVED 0x00000001
#define COMMAND_STATUS_UPDATE   0x00000002

//
// Debug-related definitions and declarations.
//
// Debug output is available via SCIA if DEBUG is defined during build.
//
#ifdef DEBUG
#define DEBUG_PRINT SCIprintf   // Map all debug print calls to SCIprintf in
                                // debug builds.
#else
#define DEBUG_PRINT while(0) ((int32_t (*)(char *, ...))0)  // Compile out all
                                                            // debug print calls
                                                            // in release builds
#endif

//******************************************************************************
//
// Globals
//
//******************************************************************************
volatile uint32_t g_ui32TickCount = 0; // The global system tick counter.
volatile uint32_t g_ui32TxCount = 0;
volatile uint32_t g_ui32RxCount = 0;
tUSBMode g_eCurrentUSBMode; // The current USB operating mode - Host, Device
                            // or unknown.
volatile uint32_t g_ui32Flags = 0;
char *g_pcStatus;
static volatile bool g_bUSBConfigured = false; // Global flag indicating that a
                                               // USB configuration has been
                                               // set.

uint8_t g_bConnected = 0;
uint8_t buffer[256];
volatile uint32_t globalCounter = 0UL;
volatile uint32_t idleCycles = 0UL;

//******************************************************************************
//
// CPUTimerIntHandler - Interrupt handler for the CPU Timer.
//
//******************************************************************************
__interrupt void
CPUTimerIntHandler(void)
{
    idleCycles = globalCounter * 100U;

    globalCounter = 0UL;

    //
    // Update our system tick counter.
    //
    g_ui32TickCount++;
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

//******************************************************************************
//
// EchoNewDataToHost - Receive new data and echo it back to the host.
//
// \param psDevice points to the instance data for the device whose data is to
// be processed.
// \param pi8Data points to the newly received data in the USB receive buffer.
// \param ui32NumBytes is the number of bytes of data available to be processed.
//
// This function is called whenever we receive a notification that data is
// available from the host. We read the data, byte-by-byte and swap the case
// of any alphabetical characters found then write it back out to be
// transmitted back to the host.
//
// \return Returns the number of bytes of data processed.
//
//******************************************************************************
static uint32_t
EchoNewDataToHost(tUSBDBulkDevice *psDevice, uint8_t *pi8Data,
                  uint_fast32_t ui32NumBytes)
{
    uint_fast32_t ui32Loop, ui32Space, ui32Count;
    uint_fast32_t ui32ReadIndex;
    uint_fast32_t ui32WriteIndex;
    tUSBRingBufObject sTxRing;

    //
    // Get the current buffer information to allow us to write directly to
    // the transmit buffer (we already have enough information from the
    // parameters to access the receive buffer directly).
    //
    USBBufferInfoGet(&g_sTxBuffer, &sTxRing);

    //
    // How much space is there in the transmit buffer?
    //
    ui32Space = USBBufferSpaceAvailable(&g_sTxBuffer);

    //
    // How many characters can we process this time round?
    //
    ui32Loop = (ui32Space < ui32NumBytes) ? ui32Space : ui32NumBytes;
    ui32Count = ui32Loop;

    //
    // Update our receive counter.
    //
    g_ui32RxCount += ui32NumBytes;

    //
    // Dump a debug message.
    //
    DEBUG_PRINT("Received %d bytes\n", ui32NumBytes);

    //
    // Set up to process the characters by directly accessing the USB buffers.
    //
    ui32ReadIndex = (uint32_t)(pi8Data - g_pui8USBRxBuffer);
    ui32WriteIndex = sTxRing.ui32WriteIndex;

    while(ui32Loop)
    {
        //
        // Copy from the receive buffer to the transmit buffer converting
        // character case on the way.
        //

        //
        // Is this a lower case character?
        //
        if((g_pui8USBRxBuffer[ui32ReadIndex] >= 'a') &&
           (g_pui8USBRxBuffer[ui32ReadIndex] <= 'z'))
        {
            //
            // Convert to upper case and write to the transmit buffer.
            //
            g_pui8USBTxBuffer[ui32WriteIndex] =
                (g_pui8USBRxBuffer[ui32ReadIndex] - 'a') + 'A';
        }
        else
        {
            //
            // Is this an upper case character?
            //
            if((g_pui8USBRxBuffer[ui32ReadIndex] >= 'A') &&
               (g_pui8USBRxBuffer[ui32ReadIndex] <= 'Z'))
            {
                //
                // Convert to lower case and write to the transmit buffer.
                //
                g_pui8USBTxBuffer[ui32WriteIndex] =
                    (g_pui8USBRxBuffer[ui32ReadIndex] - 'Z') + 'z';
            }
            else
            {
                //
                // Copy the received character to the transmit buffer.
                //
                g_pui8USBTxBuffer[ui32WriteIndex] =
                    g_pui8USBRxBuffer[ui32ReadIndex];
            }
        }

        //
        // Move to the next character taking care to adjust the pointer for
        // the buffer wrap if necessary.
        //
        ui32WriteIndex++;
        ui32WriteIndex =
            (ui32WriteIndex == BULK_BUFFER_SIZE) ? 0 : ui32WriteIndex;

        ui32ReadIndex++;
        ui32ReadIndex = (ui32ReadIndex == BULK_BUFFER_SIZE) ? 0 : ui32ReadIndex;

        ui32Loop--;
    }

    //
    // We've processed the data in place so now send the processed data
    // back to the host.
    //
    USBBufferDataWritten(&g_sTxBuffer, ui32Count);

    //
    // We processed as much data as we can directly from the receive buffer so
    // we need to return the number of bytes to allow the lower layer to
    // update its read pointer appropriately.
    //
    return(ui32Count);
}

//******************************************************************************
//
// TxHandler - Handles bulk driver notifications related to the transmit
//             channel (data to the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ui32Event identifies the event we are being notified about.
// \param ui32MsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
// related to operation of the transmit data channel (the IN channel carrying
// data to the USB host).
//
// \return The return value is event-specific.
//
//******************************************************************************
uint32_t
TxHandler(void *pvCBData, uint32_t ui32Event, uint32_t ui32MsgValue,
          void *pvMsgData)
{
    //
    // Update the transmit counter.
	// Write the USB Bulk Data.
    //
    if(ui32Event == USB_EVENT_TX_COMPLETE)
    {
        g_ui32TxCount += ui32MsgValue;

        uint32_t ui32Space = USBDBulkTxPacketAvailable(&g_sBulkDevice);
        if(ui32Space)
        {
            USBDBulkPacketWrite(&g_sBulkDevice, buffer, ui32Space, true);
        }
    }

    return(0);
}

//******************************************************************************
//
// Handles bulk driver notifications related to the receive channel (data from
// the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ui32Event identifies the event we are being notified about.
// \param ui32MsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
// related to operation of the receive data channel (the OUT channel carrying
// data from the USB host).
//
// \return The return value is event-specific.
//
//******************************************************************************
uint32_t
RxHandler(void *pvCBData, uint32_t ui32Event, uint32_t ui32MsgValue,
          void *pvMsgData)
{
    //
    // Which event are we being sent?
    //
    switch(ui32Event)
    {
        //
        // We are connected to a host and communication is now possible.
        //
        case USB_EVENT_CONNECTED:
        {
            g_bUSBConfigured = true;
            g_pcStatus = "Host Connected.";
            g_ui32Flags |= COMMAND_STATUS_UPDATE;

            //
            // Flush our buffers.
            //
            USBBufferFlush(&g_sTxBuffer);
            USBBufferFlush(&g_sRxBuffer);

            g_bConnected = true;

            break;
        }

        //
        // The host has disconnected.
        //
        case USB_EVENT_DISCONNECTED:
        {
            g_bUSBConfigured = false;
            g_pcStatus = "Host Disconnected.";
            g_ui32Flags |= COMMAND_STATUS_UPDATE;
            g_bConnected = false;
            break;
        }

        //
        // A new packet has been received.
        //
        case USB_EVENT_RX_AVAILABLE:
        {
            tUSBDBulkDevice *psDevice;

            //
            // Get a pointer to our instance data from the callback data
            // parameter.
            //
            psDevice = (tUSBDBulkDevice *)pvCBData;

            //
            // Read the new packet and echo it back to the host.
            //
            return(EchoNewDataToHost(psDevice, pvMsgData, ui32MsgValue));
        }

        //
        // Ignore SUSPEND and RESUME for now.
        //
        case USB_EVENT_SUSPEND:
        case USB_EVENT_RESUME:
            break;

        //
        // Ignore all other events and return 0.
        //
        default:
            break;
    }

    return(0);
}

#ifdef DEBUG
//******************************************************************************
//
// ConfigureSCI - Configure the SCI and its pins. This must be called before
// DEBUG_PRINT(). It configures the SCI GPIOs and the Console I/O.
//
//******************************************************************************
void
ConfigureSCI(void)
{
    //
    // GPIO28 is the SCI RX pin.
    //
    GPIO_setMasterCore(28, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);

    //
    // GPIO29 is the SCI TX pin.
    //
    GPIO_setMasterCore(29, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

    //
    // Initialize the SCI for console I/O.
    //
    SCIStdioConfig(SCIA_BASE, 115200,
                   SysCtl_getLowSpeedClock(DEVICE_OSCSRC_FREQ));
}
#endif

//******************************************************************************
//
// ModeCallback - USB Mode callback
//
// \param ui32Index is the zero-based index of the USB controller making the
//        callback.
// \param eMode indicates the new operating mode.
//
// This function is called by the USB library whenever an OTG mode change
// occurs and, if a connection has been made, informs us of whether we are to
// operate as a host or device.
//
// \return None.
//
//******************************************************************************
void
ModeCallback(uint32_t ui32Index, tUSBMode eMode)
{
    //
    // Save the new mode.
    //
    g_eCurrentUSBMode = eMode;
}

//******************************************************************************
//
// Main
//
//******************************************************************************
int
main(void)
{
    uint8_t index;
    volatile uint16_t firstPacket;
    unsigned long ui32Space;
    firstPacket = 1;

    for(index = 0; index < 256; index++)
    {
        buffer[index] = index;
    }

    //
    // Initialize device clock and peripherals
    //
    Device_init();

    //
    // Initialize GPIO and configure GPIO pins for USB.
    //
    Device_initGPIO();

    //
    // Set the clocking to run from the PLL at 60MHz
    //
    SysCtl_setAuxClock(DEVICE_AUXSETCLOCK_CFG_USB);

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    //
    Interrupt_initVectorTable();

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;

#ifdef DEBUG
    //
    // Configure the SCI for debug output.
    //
    ConfigureSCI();
#endif

    //
    // Not configured initially.
    //
    g_bUSBConfigured = false;

    //
    // Enable the GPIO peripheral used for USB, and configure the USB
    // pins.
    //
    USBGPIOEnable();
    Interrupt_register(INT_USBA, f28x_USB0DeviceIntHandler);

    //
    // Register the CPU Timer interrupt handler.
	// This is used to calculate the CPU Utilization.
    //
    Interrupt_register(INT_TIMER0, &CPUTimerIntHandler);

    CPUTimerInit();

    CPUTimer_setPeriod(CPUTIMER0_BASE,
                      (SysCtl_getClock(DEVICE_OSCSRC_FREQ) / TICKS_PER_SECOND));

    //
    // Enable the CPU Timer interrupt.
    //
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_enable(INT_TIMER0);

    CPUTimer_startTimer(CPUTIMER0_BASE);

    //
    // Show the application name on the display and SCI output.
    //
    DEBUG_PRINT("\nC2000 F2837xD Series USB Throughput bulk device example\n");
    DEBUG_PRINT("---------------------------------\n\n");

    //
    // Initialize the transmit and receive buffers.
    //
    USBBufferInit(&g_sTxBuffer);
    USBBufferInit(&g_sRxBuffer);

    USBStackModeSet(0, eUSBModeForceDevice, ModeCallback);

    //
    // Pass our device information to the USB library and place the device
    // on the bus.
    //
    USBDBulkInit(0, &g_sBulkDevice);

    //
    // Enable global interrupts
    //
    Interrupt_enableMaster();

    //
    // Wait for the first packet to be received.
    //
    while(firstPacket)
    {
        if(g_bConnected)
        {
            //
            // How much space is there in the transmit buffer?
            //
            ui32Space = USBDBulkTxPacketAvailable(&g_sBulkDevice);

            if(ui32Space)
            {
                USBDBulkPacketWrite(&g_sBulkDevice, buffer, ui32Space,
                                    true);
            }
			
            firstPacket = 0;
        }
    }

    //
    // This is where USB interrupts will be taken.
    // Set up CPUTimer for 1ms with interrupt.
    // Count Number of cycles per loop iteration.
    // In CPUTimer interrupt multiply cycle count by counter to find Number
    // cycles idle in 1ms.
    //
    while(1)
    {
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");

        //
        // Loop iteration should be 100 cycles
        //
        globalCounter++;
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
        __asm(" RPT #10 || NOP");
    }
}

//
// End of file
//
