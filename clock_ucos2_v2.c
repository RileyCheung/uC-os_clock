#include <stdio.h>
#include "includes.h"
#include "io.h"

/* Definition of Task Stacks */
#define   TASK_STACKSIZE       1024
OS_STK    idle_display_time_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    task_start_stk[TASK_STACKSIZE];
OS_STK    task_hex_stk[TASK_STACKSIZE];
OS_STK	 task_led_ramp[TASK_STACKSIZE];
OS_STK    ledBrightness_stk[TASK_STACKSIZE];
OS_STK	task_semcheck_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */

#define IDLE_DISPLAY_TIME_PRIORITY      3
#define TASK2_PRIORITY      6
#define TASKHEX_PRIORITY	7
#define TASK_SEMCHECK_PRIORITY 2

/* creating semaphores */
INT8U min_sem_err;
INT8U hour_sem_err;
INT8U clock_sem_err;
INT8U alarm_sem_err;
OS_EVENT *min_sem; //signaling
OS_EVENT *hour_sem; // signaling
OS_EVENT *clock_sem;//protection
OS_EVENT *alarm_sem;// protection

/* global variables */
int clock_time[3]; 	// [hour, minute, second]
int alarm_time[3];	// [hour, minute, second]


void increment_time( int* input)
{
	int * time= input;
	// hours minutes seconds
	//increments the integers at the addresses of the pointers
	if(time[2]++ >= 59)
	{
		time[2] = 0;
		time[1]++;
	}
	if(time[1] >= 59)
	{
		time[1] = 0;
		time[0]++;
	}
	if(time[0] >= 24)
	{
		time[0] = 0;
	}

	input = time;
}

int IntTo7Seg(int Input) {

  switch (Input) {
  case 0:  return 0x40;
  case 1:  return 0x79;
  case 2:  return 0x24;
  case 3:  return 0x30;
  case 4:  return 0x19;
  case 5:  return 0x12;
  case 6:  return 0x02;
  case 7:  return 0x78;
  case 8:  return 0x00;
  case 9:  return 0x18;
  default: return 0x80;
  };
}

// hex5 hex4 hex3 == hour10s hour1s minute10s
int timeToHexHourMinute(int hour, int minute)
{
	int ones_hour = IntTo7Seg(hour % 10);
	int tens_hour = IntTo7Seg(hour / 10);
	int tens_minute = IntTo7Seg(minute / 10);
	int two_hex_format = (tens_hour<<16)+(ones_hour<<8) + tens_minute;
	return two_hex_format;
}
// hex2 hex1 hex0 == minute1s second10s second1s
int timeToHexMinuteSecond(int minute, int second)
{
	int ones_second = IntTo7Seg(second % 10);
	int tens_second = IntTo7Seg(second / 10);
	int ones_minute = IntTo7Seg(minute % 10);
	int two_hex_format = (ones_minute<<16)+(tens_second<<8) + ones_second;
	return two_hex_format;
}
//CLOCK
void idle_display_time(void* pdata)
{
	//prints the time on hex display
	int second = 0;
	int minute = 0;
	int hour = 0;
	int second_a = 0;
	int minute_a = 0;
	int hour_a = 0;

	while (1)
	{
		switch ( IORD(SLIDERSW_BASE, 0) ) {
			case 1:		// Clock increase
				printf("MODE : CHANGE CLOCK TIME \n");
				if( OS_EventCntGet(min_sem) ) //increment minute minute_singal_semaphore
				{
					minute++;
					second = - 1;
					//when increment_time runs the second variable will be set to 0
					//always set seconds to 0 when you change time
					increment_time(&second, &minute, &hour);
				}

				if( OSSemPend( hour_sem, 0, &hour_sem_err) ) // increment hour
				{
					hour++;
					second = - 1;
					//when increment_time runs the second variable will be set to 0
					//always set seconds to 0 when you change time
					increment_time( &second, &minute, &hour );
				}
				//display static set clock time
				break;

			case 2:		// Alarm increase
				printf("MODE : CHANGE ALARM TIME \n");

				if( OSSemPend( min_sem, 0, &min_sem_err) ) //increment minute
				{
					minute_a++;
					second_a = -1;
					increment_time(&second_a, &minute_a, &hour_a);
				}

				if( OSSemPend( hour_sem, 0, &hour_sem_err) ) // increment hour
				{
					hour_a++;
					second_a = - 1;
					increment_time(&second_a, &minute_a, &hour_a);
				}

				break;
			default:
				increment_time(&second, &minute, &hour);


				//update HEX displays
				IOWR(0x00041040, 0, timeToHexHourMinute(hour, minute) ); //turns off the HEX2to0
				IOWR(0x00041050, 0, timeToHexMinuteSecond(minute, second) ); //turns off the HEX2to0
				break;
		}
		printf("hour %d \t minute %d \t second %d \n", hour, minute, second);
		printf("hour_a %d \t minute_a %d \t second_a %d \n", hour_a, minute_a, second_a);
		OSTimeDlyHMSM(0, 0,1, 0); //delay 1 second

	}
}


// Read switches
// IORD(SLIDERSW_BASE, 0);
// Read button
// IORD(KEYBUTTONS_BASE, 0);


void task2(void* pdata)
{

	int brightness  = 0;
	int sum = 0;
  while (1){
	  brightness = IORD(0x00041060, 0);
	  brightness &= 0xf; // disable top switches
	  sum += brightness;
	  if(sum >= 16)
	  {
		  IOWR(0x00041030, 0,0xfff);
		  sum -= 16;
	  }else{
		  IOWR(0x00041030, 0,0x000);
	  }
	  OSTimeDly(1);
  }
}

void taskhex(void* pdata)
{
	//updates hex display with cpu usage
	int ones;
	int tens;
	IOWR(0x00041040, 0, 0xffffff); //turns off the HEX5to3
	IOWR(0x00041050, 0, 0xffffff); //turns off the HEX2to0
	while(1)
	{
		ones = IntTo7Seg( OSCPUUsage%10 );
		tens = IntTo7Seg( OSCPUUsage/10 );
		IOWR(0x00041040, 0,(tens<<16)+(ones<<8)+(0xff));
		OSTimeDlyHMSM(0,0,5,0);
	}
}

void task_led_constant(void* pdata)
{

	INT8U brightness = 8, accumulator=0;
	while (1)
	{
		accumulator += brightness;
	if(accumulator >= 32)
	{
		IOWR(0x00041030, 0,0xfff);
		accumulator -= 32; /* take off the 32 corresponding to a 1 LED output */
	 }
	 else
	{
		 IOWR(0x00041030, 0,0x0);
	}
	OSTimeDly(1); // delay a timer tick and give other tasks a chance to run
	}


}

void ledBrightness(void* pdata)
{	//takes in an int array of brightness levels;
	//task rotates array every interval
	int* brightness = (int*)pdata;
	int temp;
	while(1)
	{
		temp = brightness[0];
		for(int i = 0 ; i < 9 ; i++)
		{
			brightness[i] = brightness[i+1];
		}
		brightness[9] = temp;
		OSTimeDlyHMSM(0,0,0,500);
	}

}

void task_led_wave( void* pdata)
{
	int accumulator[10];
	int brightness[10] = {5,10,15,20,30,30,20,15,10,5};

	OSTaskCreateExt(ledBrightness,
			brightness,
						  (void *)&ledBrightness_stk[TASK_STACKSIZE-1],
						  4,
						  4,
						  ledBrightness_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);

	while(1)
	{
		for(int i = 0 ; i < 10 ; i++)
		{
			accumulator[i]+=brightness[i];
			if(accumulator[i] >= 32)
			{
				//when led turns on set the corresponding bit
				IOWR(0x00041030, 0,IORD(0x00041030, 0)|(0x1<<i));
				accumulator[i] -= 32;
			}else
			{
				//when led turns off reset corresponding bit
				IOWR(0x00041030, 0,IORD(0x00041030, 0)& ((0x1<<i)^0xffff ));
			}
		}
		OSTimeDly(1);
	}
}

void task_semcheck( void* pdata)
{
	// [ !key1  !key0 ]

	while(1)
	{
		switch(IORD(KEYBUTTONS_BASE, 0))
		{
		case 0: //minute and hour
			OSSemPost(min_sem);
			OSSemPost(hour_sem);
			break;

		case 1: //minute
			OSSemPost(min_sem);
			break;

		case 2: //hour
			OSSemPost(hour_sem);
			break;

		default:
			break;
		}

		OSTimeDlyHMSM(0,0,0,10);
	}

}

void increment_minute() //increments the minute of either clock or alarm based on switch
{

}

void task_edit_minute( void* pdata)
{
	//waits for minute signal from key1
	//reads slide switches to determine which time to increment minute
	//obtains exclusive access to time resource
	//increment minute
	//return time resource
	while(1)
	{
		OSSemPend(min_sem, 0, &min_sem_err);
		switch( IORD(SLIDERSW_BASE, 0)  )
		{
		case 1: // change alarm  time
			OSSemPend(alarm_sem, 0, &alarm_sem_err);
			alarm_time[1]++;
			alarm_time[2] = -1;
			increment_time(alarm_time);
			OSSemPost(alarm_sem);
			break;

		case 2: // change clock time
			OSSemPend(clock_sem, 0, &clock_sem_err);
			clock_time[1]++;
			clock_time[2] = -1;
			increment_time(clock_time);
			OSSemPost(clock_sem);
			break;

		default:
			break;
		}
		OSSemPost(min_sem);
	}
}
void start_task(void* pdata)
{
	//initial statistics
	OSStatInit();

	//creating other tasks
  /* TASKS
   * Idle Display Time
   * Led Wave
   * Semaphore Check
   * */
	OSTaskCreateExt(idle_display_time,
					  NULL,
					  (void *)&idle_display_time_stk[TASK_STACKSIZE-1],
					  IDLE_DISPLAY_TIME_PRIORITY,
					  IDLE_DISPLAY_TIME_PRIORITY,
					  idle_display_time_stk,
					  TASK_STACKSIZE,
					  NULL,
					  0);

	  OSTaskCreateExt(task_led_wave,
	  					  NULL,
	  					  (void *)&task_led_ramp[TASK_STACKSIZE-1],
	  					  10,
	  					  10,
						  task_led_ramp,
	  					  TASK_STACKSIZE,
	  					  NULL,
	  					  0);

	  OSTaskCreateExt(task_semcheck,
			  NULL,
			  (void*)&task_semcheck_stk[TASK_STACKSIZE-1],
			  TASK_SEMCHECK_PRIORITY,
			  TASK_SEMCHECK_PRIORITY,
			  task_semcheck_stk,
			  TASK_STACKSIZE,
			  NULL,
			  0);

  while(1){

	printf("IdleCountWith100_ISRs = %ld\n ", OSIdleCtrMax);
	    //OSTimeDlyHMSM(0, 0, 3, 0);
	  OSTimeDlyHMSM(1,10,0,0);

  }

}


/* The main function creates two task and starts multi-tasking */
int main(void)
{
	OSInit();

	//semaphores
	min_sem = OSSemCreate(0);
	hour_sem = OSSemCreate(0);
	clock_sem = OSSemCreate(1);
	alarm_sem = OSSemCreate(1);

	OSTaskCreateExt(start_task,
						  NULL,
						  (void *)&task_start_stk[TASK_STACKSIZE-1],
						  1,
						  1,
						  task_start_stk,
						  TASK_STACKSIZE,
						  NULL,
						  0);


  OSStart();
  return 0;
}

/*
1)	Task
•	Resource0
•	Resource1
•	…

1)	hex display
•	hex display
•	alarm time
•	clock time

2)	edit hour
•	alarm time
•	clock time

3)	edit minute
•	alarm time
•	clock time

4)	alarm status
•	alarm time
•	clock time
•	LEDR

5)	increment time
•	clock time

*/

