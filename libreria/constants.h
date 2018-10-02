// Tasks TCBs
#define TASK_SPEED_P		OSTCBP(1)
#define TASK_CONTROL_P		OSTCBP(2)
#define TASK_BRAKE_P		OSTCBP(3)
#define TASK_PANEL_P		OSTCBP(4)

// Control structures
#define SEM_CAN					OSECBP(1)

#define EFLAG_FOR_SPEED			OSECBP(2)
#define EFLAG_FOR_SPEED_EFCB	OSEFCBP(1)
#define iniValueEventForSpeed	0x00
#define maskEventForSpeed		0xff

#define EFLAG_FOR_CONTROL		OSECBP(3)
#define EFLAG_FOR_CONTROL_EFCB	OSEFCBP(2)

#define iniValueEventForControl	0x00
#define maskEventForControl		0xff

#define MSG_FOR_PANEL			OSECBP(4)

// Message structure
struct StructMessage {
	float velAct;
	float velObj;
	unsigned char cruceroEncendido;
	unsigned char cruceroActivado;
};

// CAN identifiers
#define ID_SPEED	3

#define ID_BRAKE_1	1
#define ID_BRAKE_2	2
#define ID_BRAKE_4	4
#define ID_BRAKE_5	5
#define ID_BRAKE_7	7
#define ID_BRAKE_10 10
#define ID_BRAKE_11	11

	







