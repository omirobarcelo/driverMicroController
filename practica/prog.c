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

#include "constants.h"
#include "libCAD.h"
#include "libCAN.h"
#include "libLCD.h"
#include "libTIMER.h"
#include "libKEYB.h"

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
// Tasks priorities
#define PRIO_PANEL			10
#define PRIO_SPEED			10 
#define PRIO_BRAKE			8 
#define PRIO_CONTROL		9 

/******************************************************************************/
/* Global Variable and macros declaration                                     */
/******************************************************************************/
#define ON	1
#define OFF	0

#define FXT	7372800 		// Oscillator frequency
#define PLL	16
#define FCY	(FXT * PLL) / 4 // Real system frequency

//#define TCY 1000000.0 / FCY	//33.90842014 // microseconds
#define TCY	33.90842014 		// nsecs

// UART configuration
#define BAUD_RATE	115200
#define BRG       	(FCY / (16L * BAUD_RATE)) - 1L

#define TIEMPO		16.8 //ms

volatile static float aceleracion = 0;

volatile unsigned int rxMsgSID; 	// Frame ID
volatile unsigned char rxMsgData[8];// Message data
volatile unsigned char rxMsgDLC;	// Data byte size

struct Estado {
	float velAct;
	float acelAct;
	float velObj;
	unsigned char cruceroActivado;
	unsigned char cruceroEncendido;
	unsigned char freno;
};
static struct Estado estado;

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

void TaskAcelerador (void) {
	while(1) {
		OS_WaitEFlag(EFLAG_FOR_SPEED, maskEventForSpeed, OSANY_BITS, OSNO_TIMEOUT);
		
		OS_WaitBinSem(SEM_CAN, OSNO_TIMEOUT);
		if (CANtransmissionCompleted()) { // If can send the message, then 
			CANsendMessage(ID_SPEED, &aceleracion, sizeof(aceleracion));
		}
		OSSignalBinSem(SEM_CAN);
		
		OSClrEFlag(EFLAG_FOR_SPEED, maskEventForSpeed);	
	}
}


void TaskFreno (void) {
	static char c;
	
	while (1) {
		c = getKeyNotBlocking();		
		if (c != NO_BUTTON_PRESSED && c!= 0 && c!= 3 && c != 9 && c != 6 && c != 8) {
			OS_WaitBinSem(SEM_CAN, OSNO_TIMEOUT);
			if(CANtransmissionCompleted()) {
				CANsendMessage((unsigned int)c, &c, 0);
			}
			OSSignalBinSem(SEM_CAN);
		}	
		OS_Delay(1);
	}
}


void TaskControl (void) {
	static struct StructMessage mensaje;
	
	estado.velAct = 0;
	estado.velObj = -1;
	estado.cruceroEncendido = OFF;
	estado.cruceroActivado = OFF;
	estado.acelAct = 0;
	
	while (1) {
		OS_WaitEFlag(EFLAG_FOR_CONTROL, maskEventForControl, OSANY_BITS, OSNO_TIMEOUT);
		
		// Return freno to OFF if TaskFreno interruption for forward checking
		if (rxMsgSID != ID_SPEED) {
			estado.freno = OFF;
		}
		
		switch(rxMsgSID) {
			case ID_SPEED: {
				if (estado.freno == OFF) {
					float *aux = rxMsgData;
					estado.acelAct = *aux;
				}
				break;
			}
			case ID_BRAKE_11: {
				estado.freno = ON;
				estado.acelAct = -6.0;
				estado.cruceroActivado = OFF;
				break;
			}
			case ID_BRAKE_7: {
				estado.cruceroEncendido = ON;
				estado.velObj = estado.velAct;
				break;
			}
			case ID_BRAKE_10: {
				estado.cruceroEncendido = OFF;
				estado.velObj = -1; //Null
				break;
			}
			case ID_BRAKE_2: {
				if (estado.cruceroEncendido == ON) {
					estado.velObj = estado.velObj + 0.005;
				}
				break;
			}
			case ID_BRAKE_5: {
				if (estado.cruceroEncendido == ON) {
					estado.velObj = estado.velObj - 0.005;
				}
				break;
			}
			case ID_BRAKE_1: {
				if (estado.cruceroEncendido == ON) {
					estado.cruceroActivado = ON;
				}
				break;
			}
			case ID_BRAKE_4: {
				if (estado.cruceroEncendido == ON) {
					estado.cruceroActivado = OFF;
				}
				break;
			}
		}
		
		estado.velAct = estado.velAct + estado.acelAct*(TIEMPO/1000);
		if (estado.velAct < 0) {
			estado.velAct = 0;
		}
		if (estado.cruceroEncendido == ON && estado.cruceroActivado == ON) {
			if (estado.velAct > estado.velObj) {
					estado.velAct = estado.velObj;
			}	
		}
		
		mensaje.velAct = estado.velAct;
		mensaje.velObj = estado.velObj;
		mensaje.cruceroEncendido = estado.cruceroEncendido;
		mensaje.cruceroActivado = estado.cruceroActivado;
		OSSignalMsg(MSG_FOR_PANEL, (OStypeMsgP)&mensaje);
		
		char buffer[20];
		LCDMoveFirstLine();
		sprintf(buffer, "V: %f", estado.velAct);
		LCDPrint(buffer);
		LCDMoveSecondLine();
		sprintf(buffer, "A: %f", estado.acelAct);
		LCDPrint(buffer);
		
		OSClrEFlag(EFLAG_FOR_CONTROL, maskEventForControl);
	}
}

void TaskPanel (void) {
	OStypeMsgP msgP;
	static struct StructMessage *mensaje;
	
	while (1) {
			OS_WaitMsg(MSG_FOR_PANEL, &msgP, OSNO_TIMEOUT);
			mensaje = (struct StructMessage *)msgP;
			
			float velActKMH, velObjKMH;
			velActKMH = (*mensaje).velAct*3.6;
			velObjKMH = (*mensaje).velObj*3.6;
			
			WriteUART1(12);
			while (BusyUART1()) {} ;
			
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
			
			if (velObjKMH < 0) {
				sprintf(buffer, "Velocidad objetivo: DESACTIVADA\r\n");
			} else {
				sprintf(buffer, "Velocidad objetivo: %f\r\n", velObjKMH);
			}
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
			
			OS_Delay(2);
	}	
}

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/
void _ISR _ADCInterrupt(void) {
	unsigned int valueCAD = CADGetValue();
	CADClearInt();
	
	// Convertion 
	if(valueCAD < 512) {
		aceleracion = (512 - valueCAD) * (-0.005);
	} else {
		aceleracion = (valueCAD - 512) * 0.005;
	}

	//Set of Flags.
	OSSetEFlag(EFLAG_FOR_SPEED, 0xff);
}

void _ISR _C1Interrupt(void) {
	CANclearGlobalInt ();
	
	if (CANrxInt ()) {
		CANclearRxInt () ;
		// Read SID, DLC and DATA
		rxMsgSID = CANreadRxMessageSID();
		rxMsgDLC = CANreadRxMessageDLC();
		CANreadRxMessageDATA (rxMsgData);
	
		CANclearRxBuffer();
		OSSetEFlag(EFLAG_FOR_CONTROL, 0xff);
	}
}

void _ISR _T1Interrupt(void) {
	TimerClearInt();
	OSTimer();
}

/******************************************************************************/
/* Main                                                                       */
/******************************************************************************/

int main (void) {

	// ===================
	// Init peripherals
	// ===================

	// Initialize LCD
	LCDInit();
	
	// Initialize KeyBoard
	KeybInit();
	
	// Initialize and start CAD
	CADInit(CAD_INTERACTION_BY_INTERRUPT, 4);
	CADStart(CAD_INTERACTION_BY_INTERRUPT);

	// Initialize CAN
	CANinit(LOOPBACK_MODE, 1, 0, 0, 0);
	
	// Initialize and start TIMER
	Timer1Init(TIMER_PERIOD_FOR_62_5ms, TIMER_PSCALER_FOR_62_5ms, 4);
	Timer1Start();
	
	// Initialize UART
	UARTConfig();

	// =========================
	// Create Salvo structures
	// =========================
	// Init Salvo
	OSInit();

	// Create tasks (name, tcb, priority) and push them to ELIGIBLE STATE
	// From 1 up to OSTASKS tcbs available
	// Priorities from 0 (highest) down to 15 (lowest)
	OSCreateTask(TaskAcelerador, TASK_SPEED_P, PRIO_SPEED);
	OSCreateTask(TaskPanel, TASK_PANEL_P, PRIO_PANEL);
	OSCreateTask(TaskFreno, TASK_BRAKE_P, PRIO_BRAKE);
	OSCreateTask(TaskControl, TASK_CONTROL_P, PRIO_CONTROL);
	
	// Control structures for Task Acelerador
	OSCreateBinSem(SEM_CAN, 1);
	OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
	
	// Control structures for Task Control
	OSCreateEFlag(EFLAG_FOR_CONTROL, EFLAG_FOR_CONTROL_EFCB, iniValueEventForControl);
	
	// Control structures for Task 	Panel
	OSCreateMsg(MSG_FOR_PANEL, (OStypeMsgP)0);

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
void UARTConfig() {
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
