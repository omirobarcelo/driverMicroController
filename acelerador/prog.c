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



#include "libCAD.h"
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
//#define TASK_SPEED_P		OSTCBP(1)

// Tasks priorities
#define PRIO_SPEED			10 

// OS events control blocks (number of OS EVENT)
// Recall that the number of OS event must range from 1 to OSEVENTS (defined in salvocfg.h)
/*#define EFLAG_FOR_SPEED				OSECBP(1)
#define EFLAG_FOR_SPEED_EFCB		OSEFCBP(1)*/

/******************************************************************************/
/* Global Variable and macros declaration                                     */
/******************************************************************************/

volatile static float aceleracion = 0;
/*
#define iniValueEventForSpeed	0x00
#define maskEventForSpeed		0xff*/

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

void TaskAcelerador (void)
{
	while(1) {
		//OS_WaitEFlag(EFLAG_FOR_SPEED, maskEventForSpeed, OSANY_BITS, OSNO_TIMEOUT);
		//OSClrEFlag(EFLAG_FOR_SPEED, maskEventForSpeed);
		OS_WaitEFlag(EFLAG_FOR_SPEED, maskEventForSpeed, OSANY_BITS, OSNO_TIMEOUT);
		OSClrEFlag(EFLAG_FOR_SPEED, maskEventForSpeed);
		OS_WaitBinSem(SEM_CAN, OSNO_TIMEOUT);
		if (CANtransmissionCompleted()) { // If can send the message, then 
			//unsigned int valorAceleracion;
			CANsendMessage(ID_SPEED, &aceleracion, sizeof(aceleracion));
			//CANsendMessage(1, c, 2);
		}
		OSSignalBinSem(SEM_CAN);	
	}
}

/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/
void _ISR _ADCInterrupt(void){
	unsigned int valueCAD = CADGetValue();
	CADClearInt();
	
	//Convertion 
	if(valueCAD < 512) {
		aceleracion = (512 - valueCAD) * (-0.005);
	} else {
		aceleracion = (valueCAD - 512) * 0.005;
	}

	//Set of Flags.
	//OSSetEFlag(EFLAG_FOR_SPEED, 0xff);
	OSSetEFlag(EFLAG_FOR_SPEED, 0xff);
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
	CADInit(CAD_INTERACTION_BY_INTERRUPT, 4);
	CADStart(CAD_INTERACTION_BY_INTERRUPT);

	// Initialize and start CAN
	CANinit(LOOPBACK_MODE, 0, 0, 0, 0);

	// =========================
	// Create Salvo structures
	// =========================
	// Init Salvo
	OSInit();

	// Create tasks (name, tcb, priority) and push them to ELIGIBLE STATE
	// From 1 up to OSTASKS tcbs available
	// Priorities from 0 (highest) down to 15 (lowest)
	OSCreateTask(TaskAcelerador, TASK_SPEED_P, PRIO_SPEED);

	// Create  event flags.
	//OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
	//OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
	OSCreateBinSem(SEM_CAN, 1);
	OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
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