//#############################################################################
// FILE:   LABstarter_main.c
//
// TITLE:  Lab Starter
//#############################################################################

// Included Files
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "F28x_Project.h"
#include "driverlib.h"
#include "device.h"
#include "F28379dSerial.h"
#include "song.h"
#include "dsp.h"
#include "fpu32/fpu_rfft.h"

#define PI          3.1415926535897932384626433832795
#define TWOPI       6.283185307179586476925286766559
#define HALFPI      1.5707963267948966192313216916398
// The Launchpad's CPU Frequency set to 200 you should not change this value
#define LAUNCHPAD_CPU_FREQUENCY 200


// Interrupt Service Routines predefinition
__interrupt void cpu_timer0_isr(void);
__interrupt void cpu_timer1_isr(void);
__interrupt void cpu_timer2_isr(void);
__interrupt void SWI_isr(void);

// Count variables
uint32_t numTimer0calls = 0;
uint32_t numSWIcalls = 0;
extern uint32_t numRXA;
uint16_t UARTPrint = 0;
uint16_t power = 0;
float controleffort = -10.0;
float controls = 0.0;
float Left = 0.0;
float Right = 0.0;
int32_t raw = 0;
float encoder = 0;
float distL = 0.0;
float distR = 0.0;
float uLeft = 0.0;
float uRight = 0.0;
float v1 = 0.0;
float p_current1 = 0.0;
float p_old1 = 0.0;
float v2 = 0.0;
float p_current2 = 0.0;
float p_old2 = 0.0;
float prd20=0.0;
float prd2=0.0;
uint32_t QEP_maxvalue = 0xFFFFFFFFU;
float Cpos = 1.866; // 1.48;
float Vpos = 2.55;//2.561;
float Cneg = -1.666;//-1.517;

float Vneg = 2.55;//2.584;

float calc_v(float p_current, float p_old){
    float v = (p_current-p_old)/0.001;
    return v;
}

void setEPWM1A(float controleffort){
    if (controleffort > 10){
        controleffort = 10.0;
    }
    if (controleffort < -10){
        controleffort = -10.0;
    }
    prd20=EPwm1Regs.TBPRD*1/20;
    prd2=EPwm1Regs.TBPRD*1/2;
    controls = controleffort*prd20+prd2;
    EPwm1Regs.CMPA.bit.CMPA = controls;
}
void setEPWM2A(float controleffort){
    if (controleffort > 10){
        controleffort = 10.0;
    }
    if (controleffort < -10){
        controleffort = -10.0;
    }
    float controls = controleffort*EPwm2Regs.TBPRD/20+EPwm2Regs.TBPRD/2;
    EPwm2Regs.CMPA.bit.CMPA = controls;

}
void init_eQEPs(void) {
    // setup eQEP1 pins for input
    EALLOW;
    //Disable internal pull-up for the selected output pins for reduced power consumption
    GpioCtrlRegs.GPAPUD.bit.GPIO20 = 1; // Disable pull-up on GPIO20 (EQEP1A)
    GpioCtrlRegs.GPAPUD.bit.GPIO21 = 1; // Disable pull-up on GPIO21 (EQEP1B)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO20 = 2; // Qual every 6 samples
    GpioCtrlRegs.GPAQSEL2.bit.GPIO21 = 2; // Qual every 6 samples
    EDIS;
    // This specifies which of the possible GPIO pins will be EQEP1 functional pins.
    // Comment out other unwanted lines.
    GPIO_SetupPinMux(20, GPIO_MUX_CPU1, 1);
    GPIO_SetupPinMux(21, GPIO_MUX_CPU1, 1);
    EQep1Regs.QEPCTL.bit.QPEN = 0; // make sure eqep in reset
    EQep1Regs.QDECCTL.bit.QSRC = 0; // Quadrature count mode
    EQep1Regs.QPOSCTL.all = 0x0; // Disable eQep Position Compare
    EQep1Regs.QCAPCTL.all = 0x0; // Disable eQep Capture
    EQep1Regs.QEINT.all = 0x0; // Disable all eQep interrupts
    EQep1Regs.QPOSMAX = 0xFFFFFFFF; // use full range of the 32 bit count
    EQep1Regs.QEPCTL.bit.FREE_SOFT = 2; // EQep uneffected by emulation suspend in Code Composer
    EQep1Regs.QPOSCNT = 0;
    EQep1Regs.QEPCTL.bit.QPEN = 1; // Enable EQep
    EALLOW;
    // setup QEP2 pins for input
    //Disable internal pull-up for the selected output pinsfor reduced power consumption
    GpioCtrlRegs.GPBPUD.bit.GPIO54 = 1; // Disable pull-up on GPIO54 (EQEP2A)
    GpioCtrlRegs.GPBPUD.bit.GPIO55 = 1; // Disable pull-up on GPIO55 (EQEP2B)
    GpioCtrlRegs.GPBQSEL2.bit.GPIO54 = 2; // Qual every 6 samples
    GpioCtrlRegs.GPBQSEL2.bit.GPIO55 = 2; // Qual every 6 samples
    EDIS;
    GPIO_SetupPinMux(54, GPIO_MUX_CPU1, 5); // set GPIO54 and eQep2A
    GPIO_SetupPinMux(55, GPIO_MUX_CPU1, 5); // set GPIO55 and eQep2B
    EQep2Regs.QEPCTL.bit.QPEN = 0; // make sure qep reset
    EQep2Regs.QDECCTL.bit.QSRC = 0; // Quadrature count mode
    EQep2Regs.QPOSCTL.all = 0x0; // Disable eQep Position Compare
    EQep2Regs.QCAPCTL.all = 0x0; // Disable eQep Capture
    EQep2Regs.QEINT.all = 0x0; // Disable all eQep interrupts
    EQep2Regs.QPOSMAX = 0xFFFFFFFF; // use full range of the 32 bit count.
    EQep2Regs.QEPCTL.bit.FREE_SOFT = 2; // EQep uneffected by emulation suspend
    EQep2Regs.QPOSCNT = 0;
    EQep2Regs.QEPCTL.bit.QPEN = 1; // Enable EQep
    EALLOW;
    // setup QEP3 pins for input
    //Disable internal pull-up for the selected output pins for reduced power consumption
    GpioCtrlRegs.GPAPUD.bit.GPIO6 = 1; // Disable pull-up on GPIO54 (EQEP3A)
    GpioCtrlRegs.GPAPUD.bit.GPIO7 = 1; // Disable pull-up on GPIO55 (EQEP3B)
    GpioCtrlRegs.GPAQSEL1.bit.GPIO6 = 2; // Qual every 6 samples
    GpioCtrlRegs.GPAQSEL1.bit.GPIO7 = 2; // Qual every 6 samples
    EDIS;
    GPIO_SetupPinMux(6, GPIO_MUX_CPU1, 5); // set GPIO6 and eQep2A
    GPIO_SetupPinMux(7, GPIO_MUX_CPU1, 5); // set GPIO7 and eQep2B
    EQep3Regs.QEPCTL.bit.QPEN = 0; // make sure qep reset
    EQep3Regs.QDECCTL.bit.QSRC = 0; // Quadrature count mode
    EQep3Regs.QPOSCTL.all = 0x0; // Disable eQep Position Compare
    EQep3Regs.QCAPCTL.all = 0x0; // Disable eQep Capture
    EQep3Regs.QEINT.all = 0x0; // Disable all eQep interrupts
    EQep3Regs.QPOSMAX = 0xFFFFFFFF; // use full range of the 32 bit count.
    EQep3Regs.QEPCTL.bit.FREE_SOFT = 2; // EQep uneffected by emulation suspend
    EQep3Regs.QPOSCNT = 0;
    EQep3Regs.QEPCTL.bit.QPEN = 1; // Enable EQep
}
float readEncLeft(void) {
    int32_t raw = 0;
    uint32_t QEP_maxvalue = 0xFFFFFFFFU; //4294967295U
    raw = EQep1Regs.QPOSCNT;
    if (raw >= QEP_maxvalue/2) raw -= QEP_maxvalue; // I don't think this is needed and never true
    return (raw*(2*PI/40000));
}
float readEncRight(void) {
    int32_t raw = 0;
    uint32_t QEP_maxvalue = 0xFFFFFFFFU; //4294967295U -1 32bit signed int
    raw = EQep2Regs.QPOSCNT;
    if (raw >= QEP_maxvalue/2) raw -= QEP_maxvalue; // I don't think this is needed and never true
    return (raw*(2*PI/40000));
}
float readEncWheel(void) {
    int32_t raw = 0;
    uint32_t QEP_maxvalue = 0xFFFFFFFFU; //4294967295U -1 32bit signed int
    raw = EQep3Regs.QPOSCNT;
    if (raw >= QEP_maxvalue/2) raw -= QEP_maxvalue; // I don't think this is needed and never true
    return (raw*(2*PI/4000));
}
void main(void)
{
    // PLL, WatchDog, enable Peripheral Clocks
    // This example function is found in the F2837xD_SysCtrl.c file.
    InitSysCtrl();

    InitGpio();

    // Blue LED on LaunchPad
    GPIO_SetupPinMux(31, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(31, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPASET.bit.GPIO31 = 1;

    // Red LED on LaunchPad
    GPIO_SetupPinMux(34, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(34, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPBSET.bit.GPIO34 = 1;

    // LED1 and PWM Pin
    GPIO_SetupPinMux(22, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(22, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPACLEAR.bit.GPIO22 = 1;

    // LED2
    GPIO_SetupPinMux(94, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(94, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPCCLEAR.bit.GPIO94 = 1;

    // LED3
    GPIO_SetupPinMux(95, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(95, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPCCLEAR.bit.GPIO95 = 1;

    // LED4
    GPIO_SetupPinMux(97, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(97, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPDCLEAR.bit.GPIO97 = 1;

    // LED5
    GPIO_SetupPinMux(111, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(111, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPDCLEAR.bit.GPIO111 = 1;

    // LS7366#1 CS
    GPIO_SetupPinMux(130, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(130, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPECLEAR.bit.GPIO130 = 1;

    // LS7366#2 CS
    GPIO_SetupPinMux(131, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(131, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPECLEAR.bit.GPIO131 = 1;

    // LS7366#3 CS
    GPIO_SetupPinMux(25, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(25, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPACLEAR.bit.GPIO25 = 1;

    // LS7366#4 CS
    GPIO_SetupPinMux(26, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(26, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPACLEAR.bit.GPIO26 = 1;

    // WIZNET RST
    GPIO_SetupPinMux(27, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(27, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPACLEAR.bit.GPIO27 = 1;

    //PushButton 1
    GPIO_SetupPinMux(157, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(157, GPIO_INPUT, GPIO_PULLUP);

    //PushButton 2
    GPIO_SetupPinMux(158, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(158, GPIO_INPUT, GPIO_PULLUP);

    //PushButton 3
    GPIO_SetupPinMux(159, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(159, GPIO_INPUT, GPIO_PULLUP);

    //PushButton 4
    GPIO_SetupPinMux(160, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(160, GPIO_INPUT, GPIO_PULLUP);

    //SPIRAM  CS  Chip Select
    GPIO_SetupPinMux(19, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(19, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPASET.bit.GPIO19 = 1;

    //F28027 CS
    GPIO_SetupPinMux(29, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(29, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPASET.bit.GPIO29 = 1;

    //MPU9250  CS  Chip Select
    GPIO_SetupPinMux(66, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(66, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPCSET.bit.GPIO66 = 1;

    //WIZNET  CS  Chip Select
    GPIO_SetupPinMux(125, GPIO_MUX_CPU1, 0);
    GPIO_SetupPinOptions(125, GPIO_OUTPUT, GPIO_PUSHPULL);
    GpioDataRegs.GPDSET.bit.GPIO125 = 1;

    // Clear all interrupts and initialize PIE vector table:
    // Disable CPU interrupts
    DINT;

    // Initialize the PIE control registers to their default state.
    // The default state is all PIE interrupts disabled and flags
    // are cleared.
    // This function is found in the F2837xD_PieCtrl.c file.
    InitPieCtrl();

    // Disable CPU interrupts and clear all CPU interrupt flags:
    IER = 0x0000;
    IFR = 0x0000;

    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    // This will populate the entire table, even if the interrupt
    // is not used in this example.  This is useful for debug purposes.
    // The shell ISR routines are found in F2837xD_DefaultIsr.c.
    // This function is found in F2837xD_PieVect.c.
    InitPieVectTable();

    // Interrupts that are used in this example are re-mapped to
    // ISR functions found within this project
    EALLOW;  // This is needed to write to EALLOW protected registers
    PieVectTable.TIMER0_INT = &cpu_timer0_isr;
    PieVectTable.TIMER1_INT = &cpu_timer1_isr;
    PieVectTable.TIMER2_INT = &cpu_timer2_isr;
    PieVectTable.SCIA_RX_INT = &RXAINT_recv_ready;
    PieVectTable.SCIB_RX_INT = &RXBINT_recv_ready;
    PieVectTable.SCIC_RX_INT = &RXCINT_recv_ready;
    PieVectTable.SCID_RX_INT = &RXDINT_recv_ready;
    PieVectTable.SCIA_TX_INT = &TXAINT_data_sent;
    PieVectTable.SCIB_TX_INT = &TXBINT_data_sent;
    PieVectTable.SCIC_TX_INT = &TXCINT_data_sent;
    PieVectTable.SCID_TX_INT = &TXDINT_data_sent;

    PieVectTable.EMIF_ERROR_INT = &SWI_isr;
    EDIS;    // This is needed to disable write to EALLOW protected registers


    // Initialize the CpuTimers Device Peripheral. This function can be
    // found in F2837xD_CpuTimers.c
    InitCpuTimers();

    // Configure CPU-Timer 0, 1, and 2 to interrupt every given period:
    // 200MHz CPU Freq,                       Period (in uSeconds)
    ConfigCpuTimer(&CpuTimer0, LAUNCHPAD_CPU_FREQUENCY, 1000);
    ConfigCpuTimer(&CpuTimer1, LAUNCHPAD_CPU_FREQUENCY, 20000);
    ConfigCpuTimer(&CpuTimer2, LAUNCHPAD_CPU_FREQUENCY, 1000);

    // Enable CpuTimer Interrupt bit TIE
    CpuTimer0Regs.TCR.all = 0x4000;
    CpuTimer1Regs.TCR.all = 0x4000;
    CpuTimer2Regs.TCR.all = 0x4000;

    init_serialSCIA(&SerialA,115200);
    init_serialSCIB(&SerialB,19200);
    GPIO_SetupPinMux(22, GPIO_MUX_CPU1, 5); //GPIO Pinname, CPU, Mux Index
    GPIO_SetupPinMux(0, GPIO_MUX_CPU1, 1);
    GPIO_SetupPinMux(2, GPIO_MUX_CPU1, 1);
    EPwm12Regs.TBCTL.bit.FREE_SOFT = 2;
    EPwm12Regs.TBCTL.bit.CTRMODE = 0;
    EPwm12Regs.TBCTL.bit.CLKDIV = 0;
    EPwm12Regs.TBCTR = 0;
    EPwm12Regs.TBPRD = 2500;
    EPwm12Regs.CMPA.bit.CMPA = 0;
    EPwm12Regs.AQCTLA.bit.CAU = 1;
    EPwm12Regs.AQCTLA.bit.ZRO = 2;
    EPwm12Regs.TBPHS.bit.TBPHS = 0;
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 2;
    EPwm1Regs.TBCTL.bit.CTRMODE = 0;
    EPwm1Regs.TBCTL.bit.CLKDIV = 0;
    EPwm1Regs.TBCTR = 0;
    EPwm1Regs.TBPRD = 2500;
    EPwm1Regs.CMPA.bit.CMPA = 0;
    EPwm1Regs.AQCTLA.bit.CAU = 1;
    EPwm1Regs.AQCTLA.bit.ZRO = 2;
    EPwm1Regs.TBPHS.bit.TBPHS = 0;
    EPwm2Regs.TBCTL.bit.FREE_SOFT = 2;
    EPwm2Regs.TBCTL.bit.CTRMODE = 0;
    EPwm2Regs.TBCTL.bit.CLKDIV = 0;
    EPwm2Regs.TBCTR = 0;
    EPwm2Regs.TBPRD = 2500;
    EPwm2Regs.CMPA.bit.CMPA = 0;
    EPwm2Regs.AQCTLA.bit.CAU = 1;
    EPwm2Regs.AQCTLA.bit.ZRO = 2;
    EPwm2Regs.TBPHS.bit.TBPHS = 0;
    init_eQEPs();
    EALLOW; // Below are protected registers
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 1; // For EPWM1A
    GpioCtrlRegs.GPAPUD.bit.GPIO2 = 1; // For EPWMA2
    GpioCtrlRegs.GPAPUD.bit.GPIO22 = 1; // For EPWM12A
    EDIS;
    //    init_serialSCIC(&SerialC,115200);
    //    init_serialSCID(&SerialD,115200);

    // Enable CPU int1 which is connected to CPU-Timer 0, CPU int13
    // which is connected to CPU-Timer 1, and CPU int 14, which is connected
    // to CPU-Timer 2:  int 12 is for the SWI.  
    IER |= M_INT1;
    IER |= M_INT8;  // SCIC SCID
    IER |= M_INT9;  // SCIA
    IER |= M_INT12;
    IER |= M_INT13;
    IER |= M_INT14;

    // Enable TINT0 in the PIE: Group 1 interrupt 7
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    // Enable SWI in the PIE: Group 12 interrupt 9
    PieCtrlRegs.PIEIER12.bit.INTx9 = 1;

    // Enable global Interrupts and higher priority real-time debug events
    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM


    // IDLE loop. Just sit and loop forever (optional):
    while(1)
    {
        if (UARTPrint == 1 ) {
            //serial_printf(&SerialA,"Left:%ld Num SerialRX: %ld\r\n",CpuTimer2.InterruptCount,numRXA);
            UART_printfLine(1,"v1 %.2f v2 %.2f",v1,v2);
            UART_printfLine(2,"Encoder: %.2f", -encoder);
            UARTPrint = 0;
        }
    }
}


// SWI_isr,  Using this interrupt as a Software started interrupt
__interrupt void SWI_isr(void) {

    // These three lines of code allow SWI_isr, to be interrupted by other interrupt functions
    // making it lower priority than all other Hardware interrupts.
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP12;
    asm("       NOP");                    // Wait one cycle
    EINT;                                 // Clear INTM to enable interrupts



    // Insert SWI ISR Code here.......


    numSWIcalls++;

    DINT;

}

// cpu_timer0_isr - CPU Timer0 ISR
__interrupt void cpu_timer0_isr(void)
{
    CpuTimer0.InterruptCount++;
    Right = readEncLeft();
    Left = readEncRight();
    //encoder = readEncWheel();
    distR = Right*3/29.66;
    distL = Left*3/29.44;
    p_current1 = distL;
    p_current2 = distR;

    if (encoder > 10){
        encoder = 10.0;
    }
    if (encoder < -10){
        encoder = -10.0;
    }
    //uLeft = encoder;
    //uRight = encoder;
    v1 = calc_v(p_current1,p_old1);
    v2 = calc_v(p_current2,p_old2);
    uLeft=0.0;
    uRight=0.0;
    if (v1>0.0){
        uLeft = uLeft + Vpos*v1 + Cpos;
    }
    else {
        uLeft = uLeft + Vneg*v1 + Cneg;
    }
    if (v2>0.0){
        uRight = uRight + Vpos*v2 + Cpos;
    }
    else {

        uRight = uRight + Vneg*v2 + Cneg;

    }
    if (uRight > 10){
        uRight = 10.0;
    }
    if (uRight < -10){
        uRight = -10.0;
    }
    if (uLeft > 10){
        uLeft = 10.0;
    }
    if (uLeft < -10){
        uLeft = -10.0;
    }

    setEPWM1A(uLeft);
    setEPWM2A(uRight);
    //numTimer0calls++;


    //    if ((numTimer0calls%50) == 0) {
    //        PieCtrlRegs.PIEIFR12.bit.INTx9 = 1;  // Manually cause the interrupt for the SWI
    //    }

    //if ((numTimer0calls%5) == 0) {
    // Blink LaunchPad Red LED
    // GpioDataRegs.GPBTOGGLE.bit.GPIO34 = 1;
    p_old1 = distL;
    p_old2 = distR;


    // Acknowledge this interrupt to receive more interrupts from group 1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

// cpu_timer1_isr - CPU Timer1 ISR
__interrupt void cpu_timer1_isr(void)
{

    CpuTimer1.InterruptCount++;
}

// cpu_timer2_isr CPU Timer2 ISR
__interrupt void cpu_timer2_isr(void)
{
    // Blink LaunchPad Blue LED
    GpioDataRegs.GPATOGGLE.bit.GPIO31 = 1;

    if (power == 0) {
        controleffort = controleffort + 0.005;
    }
    else {
        controleffort = controleffort - 0.005;
    }
    //setEPWM1A(controleffort);
    //setEPWM2A(-controleffort);
    if (controleffort > 10){
        power = 1;
    }

    if (controleffort < -10){
        power = 0;
    }
    CpuTimer2.InterruptCount++;

    if ((CpuTimer2.InterruptCount % 50) == 0) {
        UARTPrint = 1;
    }
}

