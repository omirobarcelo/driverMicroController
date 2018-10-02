/******************************************************************************/
/*                                                                            */
/*  Description:                                                              */
/*                                                                            */
/*  Author: Oriol Miró Barceló y Lorenzo Seguí Capllonch                                                                  */
/*                                                                            */
/******************************************************************************/

#include <p30f4011.h>
#include <stdio.h>
#include <salvo.h>
#include "uart.h"

#include <string.h>
#include <stdlib.h>



//#include "libLCD.h"
#include "libTIMER.h"
#include "constants.h"



/******************************************************************************/
/* Configuration words                                                        */
/******************************************************************************/
_FOSC(CSW_FSCM_OFF & EC_PLL16);
_FWDT(WDT_OFF);
_FBORPOR(MCLR_EN & PBOR_OFF & PWRT_OFF);
_FGS(CODE_PROT_OFF);

/******************************************************************************/
/* Hardware                                                                   */
/******************************************************************************/

/******************************************************************************/
/* Salvo elements declarations                                                */
/******************************************************************************/
// Tasks TCBs
//#define TASK_PANEL_P		OSTCBP(1)

// Tasks priorities
#define PRIO_PANEL			10

// OS events control blocks (number of OS EVENT)
// Recall that the number of OS event must range from 1 to OSEVENTS (defined in salvocfg.h)
/*#define EFLAG_FOR_SPEED				OSECBP(1)
#define EFLAG_FOR_SPEED_EFCB		OSEFCBP(1)*/

/******************************************************************************/
/* Global Variable and macros declaration                                     */
/******************************************************************************/
#define ON	1
#define OFF	0

#define FXT 7372800 // Oscillator frequency
#define PLL 16
#define FCY (FXT * PLL) / 4 // Real system frequency

//#define TCY 1000000.0 / FCY //33.90842014 // microseconds
#define TCY 33.90842014 // nsecs

// UART configuration
#define BAUD_RATE 115200
#define BRG       (FCY / (16L * BAUD_RATE)) - 1L
/******************************************************************************/
/* Procedures declaration                                                     */
/******************************************************************************/
void UARTConfig();

/******************************************************************************/
/* TASKS declaration and implementation                                       */
/******************************************************************************/

 /* Task states

	DESTROYED / uninitalized
	STOPPED
	WAITING
	DELAYED
	ELIGIBLE /READY
	RUNNING

*/

void TaskPanel (void)
{
	OStypeMsgP msgP;
	struct StructMessage *mensaje;
	
	while (1) {
			OS_WaitMsg(MSG_FOR_PANEL, &msgP, OSNO_TIMEOUT);
			mensaje = (struct StructMessage *)msgP;
			
			
			float velActKMH, velObjKMH;
			velActKMH = (*mensaje).velAct*3.6;
			velObjKMH = (*mensaje).velObj*3.6;
			
			char buffer[40];
			if (velActKMH > 120) {
				sprintf(buffer, "Velocidad excesiva (> 120): WARNING\r\n");		
			} else {
				sprintf(buffer, "Velocidad excesiva (> 120): OK\r\n");	
			}
			putsUART1((unsigned int *)buffer);
			while (BusyUART1()) {} ;
			
			sprintf(buffer, "Velocidad actual: %f\r\n", velActKMH);
			putsUART1((unsigned int *)buffer);
			while (BusyUART1()) {} ;
			
			sprintf(buffer, "Velocidad objetivo: %f\r\n", velObjKMH);
			putsUART1((unsigned int *)buffer);
			while (BusyUART1()) {} ;
			
			int cruceroEncendido = (*mensaje).cruceroEncendido;
			int cruceroActivado = (*mensaje).cruceroActivado;
			
			if (cruceroEncendido == ON) {
				sprintf(buffer, "Crucero encendido: SI\r\n");
			} else {
				sprintf(buffer, "Crucero encendido: NO\r\n");
			}
			putsUART1((unsigned int *)buffer);
			while (BusyUART1()) {} ;
			
			if (cruceroActivado == ON) {
				sprintf(buffer, "Crucero activado: SI\r\n");
			} else {
				sprintf(buffer, "Crucero activado: NO\r\n");
			}
			putsUART1((unsigned int *)buffer);
			while (BusyUART1()) {} ;
			
			OS_Delay(1);
	}	
}

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/

void _ISR _T1Interrupt(void) {
	//static struct StructMessage mensaje;
	//mensaje.velAct = 40;
	//mensaje.velObj = 20;
	//mensaje.cruceroEncendido = OFF;
	//mensaje.cruceroActivado = ON;
	//OSSignalMsg(MSG_FOR_PANEL, (OStypeMsgP)&mensaje);
	
	TimerClearInt();
	OSTimer();
}

/******************************************************************************/
/* Main                                                                       */
/******************************************************************************/

int main (void)
{

	// ===================
	// Init peripherals
	// ===================


	// Initialize and start CAD
	//LCDInit();

	// Initialize and start CAN

	// =========================
	// Create Salvo structures
	// =========================
	// Init Salvo
	OSInit();

	// Create tasks (name, tcb, priority) and push them to ELIGIBLE STATE
	// From 1 up to OSTASKS tcbs available
	// Priorities from 0 (highest) down to 15 (lowest)
	OSCreateTask(TaskPanel, TASK_PANEL_P, PRIO_PANEL);
	OSCreateMsg(MSG_FOR_PANEL, (OStypeMsgP)0);
	Timer1Init(TIMER_PERIOD_FOR_125ms, TIMER_PSCALER_FOR_125ms, 4);
	Timer1Start();
	
	UARTConfig();

	// =============================================
	// Enter multitasking envinronment
	// =============================================
	while (1)
	{
		OSSched();
	}

}


/******************************************************************************/
/* Procedures implementation                                                  */
/******************************************************************************/
void UARTConfig(){
    U1MODE = 0;                     // Clear UART config - to avoid problems with bootloader

    // Config UART
	OpenUART1(UART_EN &             // Enable UART
              UART_DIS_LOOPBACK &   // Disable loopback mode
              UART_NO_PAR_8BIT &    // 8bits / No parity 
              UART_1STOPBIT,        // 1 Stop bit

              UART_TX_PIN_NORMAL &  // Tx break bit normal
              UART_TX_ENABLE,       // Enable Transmition
              
			  BRG);                 // Baudrate
}
