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

#include <string.h>
#include <stdlib.h>



#include "libLCD.h"
#include "libCAN.h"
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
//#define TASK_CONTROL_P		OSTCBP(1)

// Tasks priorities
#define PRIO_CONTROL			9 
#define TIEMPO					16.8 //ms
// OS events control blocks (number of OS EVENT)
// Recall that the number of OS event must range from 1 to OSEVENTS (defined in salvocfg.h)
/*#define EFLAG_FOR_SPEED				OSECBP(1)
#define EFLAG_FOR_SPEED_EFCB		OSEFCBP(1)*/

/******************************************************************************/
/* Global Variable and macros declaration                                     */
/******************************************************************************/
#define ON	1
#define OFF	0

volatile unsigned int rxMsgSID; //IDENTIFICADOR DE LA TRAMA RECIBIDA.
volatile unsigned char rxMsgData[8];//DATOS RECIBIDOS.
volatile unsigned char rxMsgDLC;//NUMERO DE BYTES DE LA TRAMA RECIBIDA.

struct Estado {
	float velAct;
	float acelAct;
	float velObj;
	unsigned char cruceroActivado;
	unsigned char cruceroEncendido;
	unsigned char freno;
};


/******************************************************************************/
/* Procedures declaration                                                     */
/******************************************************************************/

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

void TaskControl (void)
{
	static struct StructMessage mensaje;
	struct Estado estado;
	while (1) {
		OS_WaitEFlag(EFLAG_FOR_CONTROL, maskEventForControl, OSANY_BITS, OSNO_TIMEOUT);
		OSClrEFlag(EFLAG_FOR_CONTROL, maskEventForControl);
		estado.freno = OFF;
		switch(rxMsgSID) {
			case ID_SPEED: {
				float *aux = rxMsgData;
				estado.acelAct = *aux;
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
				estado.velObj = -1;//Null
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
		
		estado.velAct = estado.velAct + estado.acelAct*TIEMPO;
		
		mensaje.velAct = estado.velAct;
		mensaje.velObj = estado.velObj;
		mensaje.cruceroEncendido = estado.cruceroEncendido;
		mensaje.cruceroActivado = estado.cruceroActivado;
		OSSignalMsg(MSG_FOR_PANEL, (OStypeMsgP)&mensaje);
		
		char buffer[20];
		LCDClear();
		sprintf(buffer, "vel: %f", estado.velAct);
		LCDPrint(buffer);
		LCDMoveSecondLine();
		sprintf(buffer, "acel: %f", estado.acelAct);
		LCDPrint(buffer);
	}
}

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/

void _ISR _C1Interrupt(void) {
	
	CANclearGlobalInt (); //Hacemos un clear de la interrupción CAN.
	
	if (CANrxInt ()) //Si se ha interrumpido ha causa de una recepción...
		{
			CANclearRxInt () ;
			// Read SID, DLC and DATA
			rxMsgSID = CANreadRxMessageSID();
			rxMsgDLC = CANreadRxMessageDLC();
			CANreadRxMessageDATA (rxMsgData);//Llenamos el array de recepción a partir de los datos recibidos.
	
			// Clear rx buffer
			CANclearRxBuffer();
			OSSetEFlag(EFLAG_FOR_CONTROL, 0xff);
		}
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
	LCDInit();

	// Initialize and start CAN
	CANinit(LOOPBACK_MODE, 0, 1, 0, 0);

	// =========================
	// Create Salvo structures
	// =========================
	// Init Salvo
	OSInit();

	// Create tasks (name, tcb, priority) and push them to ELIGIBLE STATE
	// From 1 up to OSTASKS tcbs available
	// Priorities from 0 (highest) down to 15 (lowest)
	OSCreateTask(TaskControl, TASK_CONTROL_P, PRIO_CONTROL);

	// Create  event flags.
	OSCreateEFlag(EFLAG_FOR_CONTROL, EFLAG_FOR_CONTROL_EFCB, iniValueEventForControl);
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