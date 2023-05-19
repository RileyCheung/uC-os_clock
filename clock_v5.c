#include <stdio.h>
#include "includes.h"
#include "io.h"

/* Definition of Task Stacks */
#define   TASK_STACKSIZE       1024
OS_STK  task_start_stk[TASK_STACKSIZE];
OS_STK  task_edit_minute_stk[TASK_STACKSIZE];
OS_STK  task_edit_hour_stk[TASK_STACKSIZE];
OS_STK	task_semcheck_stk[TASK_STACKSIZE];
OS_STK  task_hex_display_stk[TASK_STACKSIZE];
OS_STK	task_idle_time_stk[TASK_STACKSIZE];
OS_STK	ledBrightness_stk[TASK_STACKSIZE];
OS_STK	task_alarm_check_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */

#define TASK_SEMCHECK_PRIORITY 2
#define TASK_EDIT_MINUTE_PRIORITY 3
#define TASK_EDIT_HOUR_PRIORITY 4
#define TASK_HEX_DISPLAY_PRIORITY 5
#define TASK_IDLE_TIME_PRIORITY 6
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
int clock_time[3] = {0, 0, 0}; 	// [hour, minute, second]
int alarm_time[3] = {0, 0, 0};	// [hour, minute, second]


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
	if(time[1] > 59)
	{
		time[1] = 0;
		time[0]++;
	}
	if(time[0] > 23)
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
int timeToHexHourMinute(int* input)
{
  int hour = input[0];
  int minute = input[1];
	int ones_hour = IntTo7Seg(hour % 10);
	int tens_hour = IntTo7Seg(hour / 10);
	int tens_minute = IntTo7Seg(minute / 10);
	int two_hex_format = (tens_hour<<16)+(ones_hour<<8) + tens_minute;
	return two_hex_format;
}
// hex2 hex1 hex0 == minute1s second10s second1s
int timeToHexMinuteSecond(int* input)
{
  int minute = input[1];
  int second = input[2];
	int ones_second = IntTo7Seg(second % 10);
	int tens_second = IntTo7Seg(second / 10);
	int ones_minute = IntTo7Seg(minute % 10);
	int two_hex_format = (ones_minute<<16)+(tens_second<<8) + ones_second;
	return two_hex_format;
}
int resetBit(int bit_position)
{
	switch (bit_position) {
	  case 0:  return 0x3FE; //1111111110
	  case 1:  return 0x3FD; //1111111101
	  case 2:  return 0x3FB; //1111111011
	  case 3:  return 0x3F7;
	  case 4:  return 0x3EF;
	  case 5:  return 0x3DF;
	  case 6:  return 0x3BF;
	  case 7:  return 0x37F;
	  case 8:  return 0x2FF;
	  case 9:  return 0x1FF;
	  default: return 0x3FF;
	}
}
//CLOCK



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
      printf("ALARM %d : %d : %d \n", alarm_time[0], alarm_time[1], alarm_time[2]);
			break;

		case 2: // change clock time
			OSSemPend(clock_sem, 0, &clock_sem_err);
			clock_time[1]++;
			clock_time[2] = -1;
			increment_time(clock_time);
			OSSemPost(clock_sem);
      printf("CLOCK %d : %d : %d \n", clock_time[0], clock_time[1], clock_time[2]);
			break;

		default:
			break;
		}

	}
}

void task_edit_hour( void* pdata)
{
	//waits for minute signal from key1
	//reads slide switches to determine which time to increment minute
	//obtains exclusive access to time resource
	//increment minute
	//return time resource
	while(1)
	{
		OSSemPend(hour_sem, 0, &hour_sem_err);
		switch( IORD(SLIDERSW_BASE, 0)  )
		{
		case 1: // change alarm  time
			OSSemPend(alarm_sem, 0, &alarm_sem_err);
			alarm_time[0]++;
			alarm_time[2] = -1;
			increment_time(alarm_time);
			OSSemPost(alarm_sem);
      printf("ALARM %d : %d : %d \n", alarm_time[0], alarm_time[1], alarm_time[2]);
			break;

		case 2: // change clock time
			OSSemPend(clock_sem, 0, &clock_sem_err);
			clock_time[0]++;
			clock_time[2] = -1;
			increment_time(clock_time);
			OSSemPost(clock_sem);
      printf("CLOCK %d : %d : %d \n", clock_time[0], clock_time[1], clock_time[2]);
			break;

		default:
			break;
		}

	}
}

void task_semcheck( void* pdata)
{
	// [ !key1  !key0 ]

	while(1)
	{
		switch(IORD(KEYBUTTONS_BASE, 0))
		{
		case 1: //minute
			OSSemPost(min_sem);
			printf("SIGNAL MINUTE\n");
			OSTimeDlyHMSM(0,0,0,200);
			break;

		case 2: //hour
			OSSemPost(hour_sem);
			printf("SIGNAL HOUR\n");
			OSTimeDlyHMSM(0,0,0,200);
			break;

		default:
			break;
		}

		OSTimeDlyHMSM(0,0,0,30);
	}

}

void task_hex_display( void* pdata)
{
  //reads mode of clock
  //displays time of clock on the hex display
  int hex_display[2]; //[hex_bottom , hex_top]
  while(1)
  {
    switch( IORD(SLIDERSW_BASE, 0)  )
		{

		case 1: // display alarm  time
			//time -> f(x) -> hex_time_code
      OSSemPend(alarm_sem, 0, &alarm_sem_err);
        hex_display[0] = timeToHexMinuteSecond(alarm_time);
        hex_display[1] = timeToHexHourMinute(alarm_time);
      OSSemPost(alarm_sem);
      //upload hex_time_code -> hex
      IOWR(HEXDISPLAYS2TO0_BASE, 0, hex_display[0] );
      IOWR(HEXDISPLAYS5TO3_BASE, 0, hex_display[1] );

			break;

		default: //display clock time

      OSSemPend(clock_sem, 0, &clock_sem_err);
      	hex_display[0] = timeToHexMinuteSecond(clock_time);
        hex_display[1] = timeToHexHourMinute(clock_time);
      OSSemPost(clock_sem);

      IOWR(HEXDISPLAYS2TO0_BASE, 0, hex_display[0] );
      IOWR(HEXDISPLAYS5TO3_BASE, 0, hex_display[1] );

			break;

		}

    OSTimeDlyHMSM(0,0,0,100);
  }

}

void task_idle_time(void* input)
{
  while(1)
  {
    OSSemPend(clock_sem, 0, &clock_sem_err);
    increment_time(clock_time);
    OSSemPost(clock_sem);
    OSTimeDlyHMSM(0,0,1,0);
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

void task_alarm_check(void* pdata)
{
  int accumulator[10];
	int brightness[10] = {5,10,15,20,30,30,20,15,10,5};

	OSTaskCreateExt(ledBrightness,
			brightness,
			(void *)&ledBrightness_stk[TASK_STACKSIZE-1],
			8,
			8,
			ledBrightness_stk,
			TASK_STACKSIZE,
			NULL,
			0);
  while(1)
  {
	  //
    if(clock_time[0] == alarm_time[0] && clock_time[1] == alarm_time[1])
    {

        for(int i = 0 ; i < 10 ; i++)
      {
        accumulator[i]+=brightness[i];
        if(accumulator[i] >= 32)
        {
          //when led turns on set the corresponding bit
          IOWR(LEDRED_BASE, 0,IORD(LEDRED_BASE, 0)|(0x1<<i));
          accumulator[i] -= 32;
        }else
        {
          //when led turns off reset corresponding bit
          IOWR(LEDRED_BASE, 0,IORD(LEDRED_BASE, 0)&resetBit(i));
        }
      }
    } else {
    	IOWR(LEDRED_BASE, 0, 0);
    }
    OSTimeDlyHMSM(0,0,0,50);

  }
}

void start_task(void* pdata)
{

	  OSTaskCreateExt(
			  task_semcheck,
			  NULL,
			  (void*)&task_semcheck_stk[TASK_STACKSIZE-1],
			  TASK_SEMCHECK_PRIORITY,
			  TASK_SEMCHECK_PRIORITY,
			  task_semcheck_stk,
			  TASK_STACKSIZE,
			  NULL,
			  0);


    OSTaskCreateExt(
			  task_edit_minute,
			  NULL,
			  (void*)&task_edit_minute_stk[TASK_STACKSIZE-1],
			  TASK_EDIT_MINUTE_PRIORITY,
			  TASK_EDIT_MINUTE_PRIORITY,
			  task_edit_minute_stk,
			  TASK_STACKSIZE,
			  NULL,
			  0);

    OSTaskCreateExt(task_edit_hour,
    		NULL,
			(void*)&task_edit_hour_stk[TASK_STACKSIZE-1],
			TASK_EDIT_HOUR_PRIORITY,
			TASK_EDIT_HOUR_PRIORITY,
			task_edit_hour_stk,
			TASK_STACKSIZE,
			NULL,
			0
    );

    OSTaskCreateExt(task_hex_display,
        		NULL,
    			(void*)&task_hex_display_stk[TASK_STACKSIZE-1],
    			TASK_HEX_DISPLAY_PRIORITY,
				TASK_HEX_DISPLAY_PRIORITY,
				task_hex_display_stk,
    			TASK_STACKSIZE,
    			NULL,
    			0
        );

    OSTaskCreateExt(task_idle_time,
    		NULL,
          (void*)&task_idle_time_stk[TASK_STACKSIZE-1],
          TASK_IDLE_TIME_PRIORITY,
          TASK_IDLE_TIME_PRIORITY ,
          task_idle_time_stk,
          TASK_STACKSIZE,
          NULL,
          0
        );

    OSTaskCreateExt(task_alarm_check,
    	  					  NULL,
    	  					  (void *)&task_alarm_check_stk[TASK_STACKSIZE-1],
    	  					  10,
    	  					  10,
							  task_alarm_check_stk,
    	  					  TASK_STACKSIZE,
    	  					  NULL,
    	  					  0);

  while(1)
  {
	  OSTimeDlyHMSM(1,10,0,0);
  }

}


/* The main function creates two task and starts multi-tasking */
int main(void)
{
  printf("MicroC/OS-II \n");
  printf("============================\n");
  printf("Modes\n");
  printf("SlideSwitch0 : Set Alarm\n");
  printf("SlideSwitch1 : Set Time\n");

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
	Resource0
	Resource1

1)	hex display
	hex display
	alarm time
	clock time

2)	edit hour
	alarm time
	clock time

3)	edit minute
	alarm time
	clock time

4)	alarm status
	alarm time
	clock time
	LEDR

5)	increment time
	clock time

*/
