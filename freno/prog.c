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



#include "libCAN.h"
#include "constants.h"
#include "libKEYB.h"
#include "libTIMER.h"



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
//#define TASK_BRAKE_P		OSTCBP(1)

// Tasks priorities
#define PRIO_BRAKE			8 


// OS events control blocks (number of OS EVENT)
// Recall that the number of OS event must range from 1 to OSEVENTS (defined in salvocfg.h)
/*#define EFLAG_FOR_SPEED				OSECBP(1)
#define EFLAG_FOR_SPEED_EFCB		OSEFCBP(1)*/

/******************************************************************************/
/* Global Variable and macros declaration                                     */
/******************************************************************************/

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

void TaskFreno (void) {
	
	char c;
	
	while (1) {
			c = getKeyNotBlocking();	
			OS_WaitBinSem(SEM_CAN, OSNO_TIMEOUT);	
			if (c != NO_BUTTON_PRESSED && c!= 0 && c!= 3 && c != 9 && c != 6 && c != 8) {
				if(CANtransmissionCompleted()) {
					CANsendMessage((unsigned int)c, &c, 0);
				}
				OSSignalBinSem(SEM_CAN);
			}	
			OS_Delay(1);

	}

}



/******************************************************************************/
/* Interrupts                                                                 */
/******************************************************************************/
void _ISR _T1Interrupt(void){
   
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

	//LCDInit();

	KeybInit();

	// Initialize and start CAN
	CANinit(LOOPBACK_MODE, 1, 1, 0, 0);
	

	// =========================
	// Create Salvo structures
	// =========================
	// Init Salvo
	OSInit();

	// Create tasks (name, tcb, priority) and push them to ELIGIBLE STATE
	// From 1 up to OSTASKS tcbs available
	// Priorities from 0 (highest) down to 15 (lowest)
	OSCreateTask(TaskFreno, TASK_BRAKE_P, PRIO_BRAKE);

	// Create  event flags.
	//OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
	//OSCreateEFlag(EFLAG_FOR_SPEED, EFLAG_FOR_SPEED_EFCB, iniValueEventForSpeed);
	OSCreateBinSem(SEM_CAN, 1);

	// =============================================
	// Enable peripherals that trigger interrupts
	// =============================================
	Timer1Init(TIMER_PERIOD_FOR_50ms,TIMER_PSCALER_FOR_50ms, 4); // Timer expires every 50 ms
	Timer1Start();

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