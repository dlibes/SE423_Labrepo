#ifndef F28379DSERIAL_H_
#define F28379DSERIAL_H_
#include <buffer.h>



#define PLL_IMULT           0x28        //40
#define OSCCLK_KHZ          10000L  //10 MHz
#define SYSCLKOUT_KHZ   (OSCCLK_KHZ*PLL_IMULT/((ClkCfgRegs.SYSCLKDIVSEL.bit.PLLSYSCLKDIV == 0) ? 1 : (ClkCfgRegs.SYSCLKDIVSEL.bit.PLLSYSCLKDIV*2)))
//LSPCLKDIV shouldn't be changed, so then should still be default /4
//so 200Mhz/4 = 50Mhz
#define LSPCLK_KHZ (SYSCLKOUT_KHZ/((ClkCfgRegs.LOSPCP.bit.LSPCLKDIV == 0) ? 1 : (ClkCfgRegs.LOSPCP.bit.LSPCLKDIV*2)))

#define LSPCLK_HZ       (LSPCLK_KHZ*1000L)

typedef struct serialSCIA_s {
	volatile struct bufferSCIA_s TX;
	volatile struct SCI_REGS *sci;
} serialSCIA_t;

typedef struct serialSCIB_s {
	volatile struct bufferSCIB_s TX;
	volatile struct SCI_REGS *sci;
} serialSCIB_t;

typedef struct serialSCIC_s {
	volatile struct bufferSCIC_s TX;
	volatile struct SCI_REGS *sci;
} serialSCIC_t;

typedef struct serialSCID_s {
	volatile struct bufferSCID_s TX;
	volatile struct SCI_REGS *sci;
} serialSCID_t;


extern serialSCIA_t SerialA;
extern serialSCIB_t SerialB;
extern serialSCIC_t SerialC;
extern serialSCID_t SerialD;

uint16_t init_serialSCIA(serialSCIA_t *s, Uint32 baud);
void uninit_serialSCIA(serialSCIA_t *s);
uint16_t serial_sendSCIA(serialSCIA_t *s, char *data, Uint16 len);
uint16_t serial_printf(serialSCIA_t *s, char *fmt, ...);

// For Text LCD
void LCD_Contrast(int16_t contrast);
void UART_printfLine(unsigned char line, char *format, ...);
void UART_vprintfLine(unsigned char line, char *format, va_list ap);

uint16_t init_serialSCIB(serialSCIB_t *s, Uint32 baud);
void uninit_serialSCIB(serialSCIB_t *s);
uint16_t serial_sendSCIB(serialSCIB_t *s, char *data, Uint16 len);

uint16_t init_serialSCIC(serialSCIC_t *s, Uint32 baud);
void uninit_serialSCIC(serialSCIC_t *s);
uint16_t serial_sendSCIC(serialSCIC_t *s, char *data, Uint16 len);

uint16_t init_serialSCID(serialSCID_t *s, Uint32 baud);
void uninit_serialSCID(serialSCID_t *s);
uint16_t serial_sendSCID(serialSCID_t *s, char *data, Uint16 len);

__interrupt void TXAINT_data_sent(void);
__interrupt void TXBINT_data_sent(void);
__interrupt void TXCINT_data_sent(void);
__interrupt void TXDINT_data_sent(void);
__interrupt void RXAINT_recv_ready(void);
__interrupt void RXBINT_recv_ready(void);
__interrupt void RXCINT_recv_ready(void);
__interrupt void RXDINT_recv_ready(void);

#define CAMNUM_FROM_FLOATS 9
typedef union CAMRecFloats_s {
    uint16_t rawData[CAMNUM_FROM_FLOATS*2];
    float floatData[CAMNUM_FROM_FLOATS];
} CAMRecFloats_t;

#define LVNUM_TOFROM_FLOATS 8
typedef union LVRecFloats_s {
    uint16_t rawData[LVNUM_TOFROM_FLOATS*2];
    float floatData[LVNUM_TOFROM_FLOATS];
} LVRecFloats_t;

typedef union LVSendFloats_s {
    uint16_t rawData[LVNUM_TOFROM_FLOATS*2];
    float floatData[LVNUM_TOFROM_FLOATS];
} LVSendFloats_t;

#define CMDNUM_FROM_FLOATS 11
typedef union CMDRecFloats_s {
    uint16_t rawData[CMDNUM_FROM_FLOATS*2];
    float floatData[CMDNUM_FROM_FLOATS];
} CMDRecFloats_t;

#define LADAR_MAX_READING 5700
typedef struct datapts{
    float angle;
    float distance_ping;
    float distance_pong;
}datapts;

typedef struct {
    float x;        //in feet
    float y;        //in feet
} xy;

typedef struct
{
    float x;        //in feet
    float y;        //in feet
    float theta;    // in radians between -PI and PI. O radian is along the +x axis, PI/2 is the +y axis
} pose;

typedef union optiData_s {
    uint16_t rawData[10];
    float floatData[5];
} optiData_t;

#define OPTITRACKDATASIZE 5

#endif /* F28379DSERIAL_H_ */
