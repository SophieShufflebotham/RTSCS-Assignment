#include <stdio.h> 
#include <stdlib.h>
#define MAX_DOSES 10
#define MAX_BOOSTS 3

/*	File Name: scheduleDose.c
	Date: 22/02/2020
	Author: Sophie Shufflebotham
	Purpose: Program for the 68HC11 microcontroller to simulate a drug delivery system by turning a motor to deliver a dose.
	Required Headers: stdio.h, stdlib.h
*/


/* Structure Declarations*/
struct dose
{
	int hours;
	int mins;
	int secs;
	int status;
	int intensity;
};

struct personalInfo
{
	char forename[20];
	char surname[20];
	char id[10];

};

/* Global Variable Declarations*/
volatile unsigned int *tcnt,*toc2,delay;
volatile int hours, mins, secs, ticks, updateClockDisp, updateInfoDisp;
unsigned char *padr, *paddr, *tflg2, *pactl, *tmsk2, *scdr, *scsr, *tflg1,*tctl1,*pgddr,*pgdr, *tmsk1;
int scheduledDoses = 0;
int boostsGiven = 0;
int suspended = 0;
int previousSwitchStatus = 0;
int boostError = 0;
struct dose doseTimes[MAX_DOSES];
struct dose boostTimes[MAX_BOOSTS];
struct personalInfo patientInfo;
volatile int deliverDoseFlag = 0;
int motorOn = 0;
int fullCycleTime = 40000;
volatile int pulseDelay = 800;
int alarm = 0;
volatile int cycles = 0;
volatile int motorRunning = 0;
int boostIntensity = 0; /*0 value indicates full dose, 1 indicates half dose*/

/* Function Prototypes*/
int main(void);
void timer(void);
void deliverDose(int);
void setDoseTime(int);
void verifyDoseTime(void);
void printAllDoses(void);
void configureClock(void);
void displayMenu(void);
void serviceAlarm(void);
int getStringSerial(char *, int);
int getCharSerial(void);
int putCharSerial(void);
void clearScreen(void);
void verifyBoost(void);
struct dose advanceFiveMinutes(struct dose);
struct dose removeFiveMinutes(struct dose);
int deliverMotorDose(int, int);
void setPatientInformation(int);

/* Board Configuration
	Vectors:

	SVEC 7 (Real Time) - timer()
	SVEC C (TOC2) - turnMotor()

	Ports:
	A0 - LED
	A1 - Booster Switch (Switch should be used as a button, being toggled between on and off rather than being left on)
	A2 - Emergency Override Switch
	G0 - Servo Motor

	Delay Values:
	800  - Left
	2500 - Middle
	4800 - Right
*/

/* Function Name: main
	Purpose: Initial entry point for software
	Params: none
	Returns: (int) 0
*/
int main() 
{ 	
	int initialised = 0;

	initialised = initialise();
	
	if(initialised == 1)
	{
		clearScreen();
		printf("--- Drug Delivery System Initial Configuration ---");
		configureClock();
		setPatientInformation(0);
		
		while(1)
		{
			updateInfoDisp = 1;
			displayUI();
			displayMenu();
		}
	}
	else
	{
		printf("An error occurred. Halting system");
	}

    return 0; 
} 

/* Function Name: displayUI
	Purpose: Draws the live monitor menu
	Params: none
	Returns: (void)
*/
void displayUI()
{
	char userInput;
	char * res;
	
	updateClockDisp = 1;
	suspended = 0;
	
	clearScreen();
	
	for(;;)
	{
		if (updateClockDisp == 1)         /*Update display every second*/
		{
			printf("%2d:%2d:%2d\r", hours, mins, secs);
			updateClockDisp = 0;
		}
		
		
		if(updateInfoDisp)
		{
			clearScreen();
			printf("--- Drug Delivery System Live Monitor---\n");
			printf("--- Press 'Esc' for menu ---\n\n");

			printPatientInfo();
			printf("\nDoses\n---------------");
			printAllDoses();
			printf("\nBoosts\n---------------");
			printBoostStatus();

			if(boostError == 1)
			{
				printf("\nBoost switch may be stuck. \nFurther boosts will not be delivered until resolved\n");
			}


			updateInfoDisp = 0;
			updateClockDisp = 1;
		}
		
		userInput = getCharSerial();
		
		if(userInput == 0x1B) /* Escape */
		{
			break;
		}
	}
	
	return;	
}

/* Function Name: initialise
	Purpose: Initialises memory addresses and default values for registers
	Params: none
	Returns: (int) 1
*/
int initialise()
{
	int res;
	padr = (unsigned char*)0x0;
	paddr = (unsigned char*)0x1;
	tmsk2 = (unsigned char*)0x24;
	tflg2 = (unsigned char*)0x25;
	pactl = (unsigned char*)0x26;
	scdr = (unsigned char*)0x2F;
	scsr = (unsigned char*)0x2E;
	tmsk1 = (unsigned char*)0x22;
	tflg1=(unsigned char*)0x23;
	toc2=(unsigned int*)0x18;
	tcnt=(unsigned int*)0x0e;
	pgddr=(unsigned char*)0x3;
	pgdr=(unsigned char*)0x02;
	tctl1=(unsigned char*)0x20;

	*paddr = 0xFA;   /*Port A Data Register all outputs apart from A0*/
	*padr = 0x00;	 /*Port A Values */
	*pactl = 0x03;   /*Prescaler - to maximum*/
	*tmsk2 = 0x40;   /*Enable RTI interrupt*/
	*pgddr =0xff; 	 /*Port G Data Register - Output*/
	*tctl1=0x00;
	*tmsk1 = 0x40;

	
	return 1;
}

/* Function Name: displayMenu
	Purpose: Displays option menu when 'Esc' is pressed in the live monitor
	Params: none
	Returns: (void)
*/
void displayMenu()
{
	char userInput [37] = "";
	char inputChar;
	char inputString [];
	int returnToDisp = 0;
	
	suspended = 1;
	updateInfoDisp = 1;
	clearScreen();
	

	while(returnToDisp == 0)
	{
		printf("--- Drug Delivery System Menu ---");
		printf("\n--- Press 'Esc' to return to live monitor ---");
		printf("\n1. Setup New Dose\n2. View All Dose Times\n3. View Current Time\n4. Edit Patient Information\n5. Alter Existing Dose\n");		
	
		getStringSerial(userInput, 37);
		
		if(userInput[0] == 0x1B)
		{
			returnToDisp = 1;
		}
				
		if(returnToDisp == 0)
		{
			/*Option 1*/
			if(userInput[0] == '1')
			{
				if(scheduledDoses >= MAX_DOSES)
				{
					clearScreen();
					printf("No more than %d doses can be scheduled", MAX_DOSES);
				}
				else
				{
					clearScreen();
					setDoseTime(-1);
					clearScreen();
				}
			}
				
			/*Option 2*/
			if(userInput[0] == '2')
			{
				clearScreen();
				printAllDoses();
				clearScreen();
			}
				
			/*Option 3*/
			if(userInput[0] == '3')
			{
				clearScreen();
				printf("\nThe current time is: %02d:%02d:%02d\r", hours, mins, secs);	
			}		
				
			/*Option 4*/
			if(userInput[0] == '4')
			{
				clearScreen();
				setPatientInformation(1);
				clearScreen();
			}	

			/*Option 5*/;
			if(userInput[0] == '5')
			{
				clearScreen();
				if(scheduledDoses > 0)
				{
					editDoseTime();
					clearScreen();
				}
				else
				{
					printf("\nNo doses scheduled to edit!");
				}

			}	
		}
	}
}


/* Interrupt Function - Real Time (SVEC 7)
	Function Name: timer
	Purpose: Tracks number of ticks to monitor current time, sets alarm flag every second
	Params: none
	Returns: (void)
*/
@interrupt void timer(void)
{
	ticks++;
	
	if (ticks == 30)
	{
		ticks = 0;
		alarm = 1;
		secs++;
	}

	if (secs == 60)
	{
		secs = 0;
		mins++;
	}
	if (mins == 60)
	{
		mins = 0;
		hours++;
	}
	if (hours == 24)
	{
		hours = 0;
	}
	*tflg2 = 0x40;                      /*Reset RTI flag*/
}

/* 
	Function Name: deliverDose
	Purpose: Sets flag to indicate that scheduled dose should be delivered, sets dose status to delivered
	Params: (int) doseIndex - Integer indicating index of dose to be delivered in the array
	Returns: (void)
*/
void deliverDose(doseIndex)
{
	deliverDoseFlag = deliverMotorDose(doseTimes[doseIndex].intensity, (doseIndex + 1));
	updateInfoDisp = 1;
	doseTimes[doseIndex].status = 1; /*Delivered*/
}

/* 
	Function Name: setDoseTime
	Purpose: Create a new scheduled dose
	Params: (int) index - Index of dose to be overwritten, only set if applicable
	Returns: (void)
*/
void setDoseTime(int index)
{
	struct dose newDoseTime;
	char hourString [3] = "";
	char minString [3] = "";
	char secString[3] = "";
	char intensityString[2] = "";
	int validationResult = 0;
	int validTime = 0;

	while (validTime != 1)
	{
		while(validationResult != 1)
		{
			printf("\nPlease set dose hours: ");
			getStringSerial(hourString, 3);
			
			validationResult = validateTimeInput(hourString);
			
			if(validationResult == 1)
			{
				newDoseTime.hours = atoi(hourString);
				
				if(newDoseTime.hours > 23)
				{
					printf("\nHours should only be 0-23");
					validationResult = -1;
				}
			}
		}
		
		validationResult = 0; /* Reset Validation status */
		
		while(validationResult != 1)
		{
			printf("\nPlease set dose mins: ");
			getStringSerial(minString, 3);
			
			validationResult = validateTimeInput(minString);
			
			if(validationResult == 1)
			{
				newDoseTime.mins = atoi(minString);
				
				if(newDoseTime.mins > 60)
				{
					printf("\nMins should only be 0-59");
					validationResult = -1;
				}
			}
		}
		
		validationResult = 0;
		
		while(validationResult != 1)
		{
			printf("\nPlease set dose secs: ");
			getStringSerial(secString, 3);
			
			validationResult = validateTimeInput(secString);
			
			if(validationResult == 1)
			{
				newDoseTime.secs = atoi(secString);
				
				if(newDoseTime.secs > 60)
				{
					printf("\nSecs should only be 0-59");
					validationResult = -1;
				}
			}
		}

	
		validationResult = 0;
		validTime = 1; /*Note - Hardcoded due to removal of function*/

		do
		{
			printf("\nPlease set dose intensity a. 50%% b.: 100%% ");
			getStringSerial(intensityString, 2);

			if(intensityString[0] == 'a')
			{
				newDoseTime.intensity = 1;
				validationResult = 1;
			}

			if(intensityString[0] == 'b')
			{
				newDoseTime.intensity = 0;
				validationResult = 1;
			}

			if(validationResult != 1)
			{
				printf("\nPlease only use the characters 'a' and 'b' to indicate your choice");
			}
		}
		while(validationResult != 1);

	}
	newDoseTime.status = 0; /*Pending*/
	
	if(index == -1)
	{
		index = scheduledDoses;
		scheduledDoses++;
	}

	doseTimes[index] = newDoseTime;
}

/* 
	Function Name: verifyDoseTime
	Purpose: Checks whether a scheduled dose should be delivered
	Params: none
	Returns: (void)
*/
void verifyDoseTime()
{
	int i;
	
	for(i = 0; i < scheduledDoses; i++)
	{
		if(hours == doseTimes[i].hours)
		{
			if(mins == doseTimes[i].mins)
			{
				if(secs == doseTimes[i].secs)
				{
					deliverDose(i);				
				}
			}
		}	
	}
}

/* 
	Function Name: printAllDoses
	Purpose: Prints all scheduled doses onto the screen
	Params: none
	Returns: (void)
*/
void printAllDoses()
{
	int i;
	char status[20] = "";
	char intensity[20] = "";
	
	if(scheduledDoses == 0)
	{
		printf("\n--No doses currently scheduled--");
	}
	
	for(i = 0; i < scheduledDoses; i++)
	{
		if(suspended == 1)
		{
			strcpy(status, "Suspended");
		}
		else
		{
			if(doseTimes[i].status == 1)
			{
				strcpy(status, "Delivered");
			}	
			else
			{
				strcpy(status, "Pending");
			}	
		}

			if(doseTimes[i].intensity == 1)
			{
				strcpy(intensity, "50");
			}	
			else
			{
				strcpy(intensity, "100");
			}	

		printf("\nDose #%d at %02d:%02d:%02d		Status: %s      Intensity: %s%%", (i + 1), doseTimes[i].hours, doseTimes[i].mins, doseTimes[i].secs, status, intensity);		
	}
	
	printf("\n%d of %d doses scheduled\n", scheduledDoses, MAX_DOSES);
}

/* 
	Function Name: configureClock
	Purpose: Sets initial clock time
	Params: none
	Returns: (void)
*/
void configureClock()
{
	char hourString [3] = "";
	char minString [3] = "";
	char secString[3] = "";
	int validationResult = 0;
	
	while(validationResult != 1)
	{
		printf("\nPlease set current hours: ");
		getStringSerial(hourString, 3);
		
		validationResult = validateTimeInput(hourString);
		
		if(validationResult == 1)
		{
			hours = atoi(hourString);
			
			if(hours > 23)
			{
				printf("\nHours should only be 0-23");
				validationResult = -1;
			}
		}
	}
	
	validationResult = 0; /* Reset Validation status */
	
	while(validationResult != 1)
	{
		printf("\nPlease set current mins: ");
		getStringSerial(minString, 3);
		
		validationResult = validateTimeInput(minString);
		
		if(validationResult == 1)
		{
			mins = atoi(minString);
			
			if(mins > 60)
			{
				printf("\nMins should only be 0-59");
				validationResult = -1;
			}
		}
	}
	
	validationResult = 0;
	
	while(validationResult != 1)
	{
		printf("\nPlease set current secs: ");
		getStringSerial(secString, 3);
		
		validationResult = validateTimeInput(secString);
		
		if(validationResult == 1)
		{
			secs = atoi(secString);
			
			if(secs > 60)
			{
				printf("\nSecs should only be 0-59");
				validationResult = -1;
			}
		}
	}		
}

/* 
	Function Name: serviceAlarm
	Purpose: Calls functions that must be executed every second. These functions are:
				- emergencyOverride - Overrides system if switch is enabled
				- verifyDoseTime - Checks if scheduled dose should be delivered
				- verifyBoostTime - Validates and delivers boost
	Params: none
	Returns: (void)
*/
void serviceAlarm()
{
	unsigned char emergencySwitch;

	emergencySwitch = *padr &0x04;
	updateClockDisp = 1;

	if(emergencySwitch > 1)
	{
		emergencyOverride(1);
	}

	if(suspended == 0)
	{
		verifyDoseTime();
		verifyBoostTime();		
	}
	alarm = 0;	
}

/* 
	Function Name: getStringSerial
	Purpose: Builds string using custom getChar function
	Params: (char *) fullString - Pointer to destination array for full string
			(int) maxLength - Integer indicating size of array
	Returns: (int) 0
*/
int getStringSerial(char * fullString, int maxLength)
{
	char *stringPoint;
	char currentChar;
	int noOfChars = 0;

		
	stringPoint = fullString; /*create a pointer, as we don't know how big the string will be*/
	
	for (;;){
		
		currentChar = getCharSerial(); /*Get the individual character from the serial port*/
		
		if(currentChar == 0x1B) /* Escape */
		{
			fullString[0] = 0x1B;
			break;	
		}
		
		/*dealing with backspace*/
		if(currentChar == 0x08)
		{ 
			putchar(0x08);		
			putchar(' '); /*print the backspace and a blank space to the screen, this will overwrite the old character*/
			/*stringPoint--;	move the pointer to remove the deleted character*/
				
			printf("%c", currentChar);		
			if(noOfChars > 0){ /*Removes 1 from the total if there was characters existing in the first place, to stop the program creating negative numbers by pressing backspace at the begining of the string*/
				noOfChars = noOfChars - 1;
				stringPoint--;
			}								
		}
		else if((noOfChars+1) >= maxLength)
		{
			currentChar == 0xFF;
		}
		
		else if(currentChar != 0x0d && currentChar != 0xFF)
		{ /*Make sure currentChar isn't backspace or FF, the character that's returned after the alarm has been triggered and handled*/
			printf("%c",currentChar); 
			*(stringPoint++) = currentChar; /*set the current location of the pointer to the current character*/
			noOfChars = noOfChars + 1;
		}
		
		
		if(currentChar == 0x0d) /*dealing with when the enter key is pressed*/
		{ 	*stringPoint = '\0'; /* terminate string*/
			break; /*when enter is pressed, break out of the infinte for loop, so the string can be terminated and return*/
		}
		
	}
	return 0;
}

/* 
	Function Name: getCharSerial
	Purpose: Get individually input character from the serial data register
	Params: none
	Returns: (int) currentChar - Value of char input, cast to an int for comparisons
*/
int getCharSerial()
{
	char currentChar;
	
	while (!(*scsr & 0x20) && alarm == 0); /*while the alarm is 0 and the scsr is free, meaning the data entry is just sitting idle*/
	
	if(alarm == 0)
	{
	 currentChar = *scdr; /*Currently stored in the scdr is the current character, put that into currentChar so that the scdr can be freed again*/
	}
	
	if(alarm == 1) /*Alarm will only equal 1 when too much time has elapsed*/
	{ 
		serviceAlarm();
		return -1;
	}
	
	return (int) currentChar;
}

/*  
	Function Name: validateTimeInput
	Purpose: Validate whether the given time is a valid time value (supports values with leading 0)
	Params: (char *) userInput - Pointer to input value
	Returns: (int) result - Validation result - -1 for failure
*/

int validateTimeInput(char * userInput)
{
	int stringSize = 0;
	int result = 0;
	
	stringSize = strlen(userInput);
	
	if(stringSize > 2)
	{
		printf("\n[%s]Input value is too long", userInput);
		result = -1;
	}
	
	if(result == 0)
	{
		if(stringSize == 0)
		{
			printf("\nPlease enter a number");
			result = -1;
		}
	}
	
	if(result == 0) /*If validation has not already failed*/
	{
		if((int) userInput[0] < 0x30 || (int) userInput[0] > 0x39)
		{
			printf("\nPlease only use numbers");
			result = -1;	
		}
	}
	
	if(result == 0) /*If validation has not already failed*/
	{
		if(stringSize == 2)
		{
			if((int) userInput[1] < 0x30 || (int) userInput[1] > 0x39)
			{
				printf("\nPlease only use numbers");
				result = -1;	
			}			
		}
	}
	
	
	if(result == 0) /*Unmodified 'result' variable means that validation has passed*/
	{
		result = 1;
	}

	return result;
}

/*  
	Function Name: clearScreen
	Purpose: Removes all text from screen and sets cursor to top left of screen
	Params: none
	Returns: (void)
*/
void clearScreen(void)
{
	/*Below is an ANSI Escape Sequence
	\033 Represents Esc 
	[2J erases display
	[H moves cursor to specified position, or 0,0 if no position is specified*/
	printf("\033[2J\033[H"); 
}

/*  
	Function Name: setPatientInformation
	Purpose: Set values containing patient personal information
	Params: (int) initialRunComplete - Flag to indicate if information is being initialised or overwritten
	Returns: (void)
*/
void setPatientInformation(int initialRunComplete)
{
	char name [20];
	char surname [20];
	char id [10];
	char intensity [2];
	char yesNo [2];
	int i;
	int validResponse = 0;

	printf("\nPlease set patient forename: ");
	getStringSerial(name, 20);
	strcpy(patientInfo.forename, name);

	printf("\nPlease set patient surname: ");
	getStringSerial(surname, 20);
	strcpy(patientInfo.surname, surname);

	printf("\nPlease set patient id: ");
	getStringSerial(id, 10);
	strcpy(patientInfo.id, id);

	do
	{
		printf("\nSet patient's boost intensity, a. 50%% b.100%%: ");
		getStringSerial(intensity, 2);

		if(intensity[0] == 'a')
		{
			boostIntensity = 1;
			validResponse = 1;
		}

		if(intensity[0] == 'b')
		{
			boostIntensity = 0;
			validResponse = 1;
		}

		if(validResponse != 1)
		{
			printf("\nPlease only use the characters 'a' and 'b' to indicate your choice\n");
		}
	}
	while(validResponse != 1);

	updateInfoDisp = 1;

	if(initialRunComplete)
	{
	do
	{
		printf("\nWould you like to reset the system? (This includes scheduled doses and administered boosts)\na. Yes b. No\n");
		getStringSerial(yesNo, 2);

		if(yesNo[0] == 'a')
		{
			scheduledDoses = 0;
			boostsGiven = 0;
			validResponse = 1;
		}

		if(yesNo[0] == 'b')
		{
			validResponse = 1;
		}

		if(validResponse != 1)
		{
			printf("\nPlease only use characters 'a' and 'b' to indicate your choice\n");
		}
	}
	while(validResponse != 1);
	}
}

/*  
	Function Name: printPatientInfo
	Purpose: Print the patient information on screen with formatting
	Params: none
	Returns: (void)
*/
void printPatientInfo()
{
	printf("\nPatient name: %s %s", patientInfo.forename, patientInfo.surname);
	printf("\nID: %s", patientInfo.id);
}

/*  
	Function Name: printBoostStatus
	Purpose: Print the current boost status - how many boosts have been delivered, and list the intensity
	Params: none
	Returns: (void)
*/
void printBoostStatus()
{
	int i;
	char intensityString[5];

	if(boostIntensity == 1)
	{
		strcpy(intensityString, "50");
	}
	else
	{
		strcpy(intensityString, "100");
	}

	printf("\n%d of %d boosts delivered     -     Intensity: %s%%", boostsGiven, MAX_BOOSTS, intensityString);

	if(boostsGiven > 0)
	{
		for(i = 0; i < boostsGiven; i++)
		{
			printf("\nBoost #%d delivered at %02d:%02d:%02d", (i + 1), boostTimes[i].hours, boostTimes[i].mins, boostTimes[i].secs);
		}
	}
	printf("\n");
}

/*  
	Function Name: verifyBoostTime
	Purpose: Validate whether a boost can be delivered
	Params: none
	Returns: (void)
*/
void verifyBoostTime()
{
	unsigned char switchIn;

	switchIn = *padr &0x01;

	if(switchIn == 0)
	{
		previousSwitchStatus = 0;

		if(boostError == 1)
		{
			boostError = 0;
			updateInfoDisp = 1;
		}	
	}

	if(switchIn == 1)
	{
		if(previousSwitchStatus == 1)
		{
			if(boostError == 0)
			{
				boostError = 1;
				updateInfoDisp = 1; 
			}	
		}

		if(boostsGiven < MAX_BOOSTS && boostError == 0)
		{
			deliverBoost();
		}
		previousSwitchStatus = 1;
	}
}

/*  
	Function Name: deliverBoost
	Purpose: Sets flag to indicate that boost should be delivered, sets boost status to delivered
	Params: none
	Returns: (void)
*/
void deliverBoost()
{
	deliverDoseFlag = deliverMotorDose(boostIntensity, 11);
	boostTimes[boostsGiven].hours = hours;
	boostTimes[boostsGiven].mins = mins;
	boostTimes[boostsGiven].secs = secs;

	boostsGiven++;
	updateInfoDisp = 1;
}

/*  UNUSED
	Function Name: validateTimeBetweenDoses 
	Purpose: Validate that the new dose is not too close to an existing dose
	Params: (struct dose) inputTime - Structure containing time input by user
	Returns: (int) validationResult - Result of validation, -1 for failure
*/
int validateTimeBetweenDoses(struct dose inputTime)
{
	int validationResult = 0;
	int i = 0;
	struct dose fiveMinsPrior;
	struct dose fiveMinsAhead;

	fiveMinsAhead = advanceFiveMinutes(inputTime);
	fiveMinsPrior = removeFiveMinutes(inputTime);

	for(i = 0; i < scheduledDoses; i++)
	{
		if(doseTimes[i].hours == fiveMinsAhead.hours)
		{
			if(doseTimes[i].mins == fiveMinsAhead.mins)
			{
				validationResult = -1;
				break;
			}
		}
		if(doseTimes[i].hours == fiveMinsPrior.hours)
		{
			if(doseTimes[i].mins == fiveMinsPrior.mins)
			{
				validationResult = -1;
				break;
			}
		}
	}

	if(validationResult != -1)
	{
		validationResult = 1;
	}

	return validationResult;
}

/*  UNUSED
	Function Name: advanceFiveMinutes
	Purpose: Advance the input time by 5 minutes
	Params: (struct dose) originalTime - Time to be advanced
	Returns: (struct dose) newTime - Original time advanced by 5 minutes
*/
struct dose advanceFiveMinutes(struct dose originalTime)
{
	struct dose newTime;
	int tempNum = 0;
	int minsDifference = 0;

	if(originalTime.mins > 54)
	{
		newTime.hours = (originalTime.hours + 1);

		if(newTime.hours == 24)
		{
			newTime.hours = 0;
		}
	}
	else
	{
		newTime.hours = originalTime.hours;
	}

	newTime.mins = (originalTime.mins + 5);

	if(newTime.mins > 59)
	{
		tempNum = 60 - newTime.mins; /*How many mins from 60*/
		minsDifference = 5 - tempNum; /*Take that away from 5*/
		newTime.mins = (0 + minsDifference);
	}

	newTime.secs = 0;

	return newTime;
}

/*  UNUSED
	Function Name: removeFiveMinutes
	Purpose: Remove 5 minutes from the input time
	Params: (struct dose) originalTime - Time to be altered
	Returns: (struct dose) newTime - Original time with 5 minutes removed
*/
struct dose removeFiveMinutes(struct dose originalTime)
{
	struct dose newTime;
	int tempNum = 0;
	int minsDifference = 0;
	int wrappedTime = 0;

	if(originalTime.mins <= 4)
	{
		newTime.hours = (originalTime.hours - 1);

		if(newTime.hours == 0)
		{
			wrappedTime = 1; /*Don't worry about yesterday's doses*/
		}
	}
	else
	{
		newTime.hours = originalTime.hours;
	}

	if(wrappedTime != 1) 
	{
		if(newTime.mins <= 4)
		{
			minsDifference = 5 - newTime.mins;
			newTime.mins = 60 - minsDifference;
		}
		else
		{
			printf("\nnew mins before: %d", newTime.mins);
			newTime.mins = (originalTime.mins - 5);
			printf("\nnew mins after: %d", newTime.mins);
		}
	}


	printf("\nOG mins: %d", originalTime.mins);
	newTime.secs = 0;

	return newTime;
}

/*  Interrupt Function - TOC 2 (SVEC C)
	Function Name: turnMotor
	Purpose: Create a PWM waveform in order to turn the motor
	Params: none
	Returns: (void)
*/
@interrupt void turnMotor()
{
	int i;
	int localPulseDelay = pulseDelay;

	if (motorRunning == 1)
	{
		*padr = 0xff;
	}
	else
	{
		*padr = 0x00;
	}

	if(deliverDoseFlag > 0)
	{
		cycles++;
		motorRunning = 1;
	}

	if(motorOn == 0)
	{
		/*On*/
		*pgdr = 0x1;
		motorOn = 1;
		*tflg1=0x40; /*Clear TOC2 Flag*/
		*toc2=*tcnt+localPulseDelay; /*Read timer and add offset period*/
	}
	else
	{
		/*Off*/
		*pgdr = 0x00;
		motorOn = 0;
		*tflg1=0x40;/*Clear TOC2 Flag*/
		*toc2=*tcnt+(fullCycleTime - localPulseDelay); /*Read timer and add offset period*/
	}

	if(cycles > 100 && deliverDoseFlag > 0)
	{
		cycles = 0;
		resetMotor();
	}
}

/*  
	Function Name: deliverMotorDose
	Purpose: Set the pulse delay for the motor based on the intensity of the dose to be delivered
	Params: (int) intensity - Flag indicating if the dose to be delivered is a half or full dose
			(int) doseIndex - Index of dose to be delivered
	Returns: (int) doseIndex - Index of dose delivered
*/
int deliverMotorDose(int intensity, int doseIndex)
{
	motorRunning = 1;

	if(intensity == 1)
	{
		pulseDelay = 2500;
	}
	else
	{
		pulseDelay = 4800;
	}

	return doseIndex;
}

/*  
	Function Name: resetMotor
	Purpose: Set the pulse delay to turn the motor all the way to the left, and reset all associated flags
	Params: none
	Returns: (void)
*/
void resetMotor()
{
	motorRunning = 0;
	deliverDoseFlag = 0;
	pulseDelay = 800;
}

/*  
	Function Name: emergencyOverride
	Purpose: Prevent any further action being taken by the system
	Params: (int) statusCode - An integer indicating the reason for the override
	Returns: (void)
*/
void emergencyOverride(int statusCode)
{
	suspended = 1;
	clearScreen();
	printf("Emergency Mode active\nReason: ");

	switch(statusCode)
	{
		case '1':
		printf("\nManual override engaged\nPlease restart the system");
		break;

		case '2':
		printf("\nAn unexpected error occurred\nPlease restart the system");
		break;
	}
	while(1);
}

/*  
	Function Name: editDoseTime
	Purpose: Accept a new dose time from the user, and overwrite the selected dose with the new time
	Params: none
	Returns: (void)
*/
void editDoseTime()
{
	char userInput [2];
	int doseToChange;
	int validationResult = 0;

	clearScreen();


	do
	{
		printf("\nWhich dose would you like to edit?\n");
		printAllDoses();
		getStringSerial(userInput, 3);
		doseToChange = (atoi(userInput) - 1);

		if(doseToChange > (scheduledDoses - 1))
		{
			printf("Invalid dose\n");
			validationResult = -1;
		}
		else
		{
			validationResult = 1;
		}

		if(doseTimes[doseToChange].status == 1)
		{
			printf("Delivered doses cannot be edited\n");
			validationResult = -1;
		}
		else
		{
			if(validationResult == 0)
			{
				validationResult = 1;
			}
		}
	}
	while (validationResult != 1);

	validationResult = 0;

	do
	{
		printf("\nDose %d at %2d:%2d is selected\nWhat would you like to do? \na. Edit Dose b. Remove Dose c. Cancel\n");
		getStringSerial(userInput, 3);

		if(userInput[0] == 'a')
		{
			validationResult = 1;
			setDoseTime(doseToChange);
		}

		if(userInput[0] == 'b')
		{
			validationResult = 1;
			removeDoseTime(doseToChange);
		}

		if(userInput[0] == 'c')
		{
			validationResult = 1;
		}

		if(validationResult != 1)
		{
			printf("\nPlease only use the characters 'a', 'b' or 'c' to indicate your choice \n");
		}
	}
	while (validationResult != 1);
}

/*  
	Function Name: removeDoseTimes
	Purpose: Remove the selected dose from the array of doses
	Params: (int) doseIndex - Index of dose to be removed
	Returns: (void)
*/
void removeDoseTime(doseIndex)
{
	struct dose tempArray[MAX_DOSES];
	int i;
	int elements = 0;
	/*Copy array*/

	for(i = 0; i < scheduledDoses; i++)
	{
		if(i != doseIndex)
		{
			tempArray[elements] = doseTimes[i];
			elements++;
		}
	}

	scheduledDoses--;

	for(i = 0; i < elements; i++)
	{
		doseTimes[elements] = tempArray[elements];
	}
}
