#include <stdio.h>
#include "includes.h"
#include "io.h"

/* Definition of Task Stacks */
#define   TASK_STACKSIZE       1024
OS_STK    idle_display_time_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    task_start_stk[TASK_STACKSIZE];
OS_STK    task_hex_stk[TASK_STACKSIZE];
OS_STK	  task_led_ramp[TASK_STACKSIZE];
OS_STK    ledBrightness_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */

#define IDLE_DISPLAY_TIME_PRIORITY      2
#define TASK2_PRIORITY      5
#define TASKHEX_PRIORITY	4






void increment_time(int* second_ptr, int* minute_ptr, int* hour_ptr)
{
	int second = *second_ptr;
	int minute = *minute_ptr;
	int hour = *hour_ptr;

	//increments the integers at the addresses of the pointers
	if(second++ >= 59)
	{
		second = 0;
		minute++;
	}
	if(minute >= 59)
	{
		minute = 0;
		hour++;
	}
	if(hour >= 24)
	{
		hour = 0;
	}
	*second_ptr = second;
	*minute_ptr = minute;
	*hour_ptr = hour;
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
		switch (IORD(SLIDERSW_BASE, 0)) {
			case 1:		// Clock increase
				printf("change clock time \n");
				if(1) //increment minute
				{
					minute++;
					second--;
					increment_time(&second, &minute, &hour);;
				}

				if(0) // increment hour
				{
					hour++;
					second--;
					increment_time(&second, &minute, &hour);;
					;
				}
				//display static set clock time
				break;

			case 2:		// Alarm increase
				printf("change alarm time \n");

				if(1) //increment minute
				{
					minute_a++;
					second_a--;
					increment_time(&second_a, &minute_a, &hour_a);
				}

				if(0) // increment hour
				{
					hour_a++;
					second_a--;
					increment_time(&second_a, &minute_a, &hour_a);;
					;
				}
				//display static alarm time

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
		OSTimeDlyHMSM(0, 0, 1, 0); //delay 1 second

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
				IOWR(0x00041030, 0,IORD(0x00041030, 0)&resetBit(i));
			}
		}
		OSTimeDly(1);
	}
}


void start_task(void* pdata)
{
	//initial statistics
	OSStatInit();

	//creating other tasks
  /**/
	OSTaskCreateExt(idle_display_time,
					  NULL,
					  (void *)&idle_display_time_stk[TASK_STACKSIZE-1],
					  IDLE_DISPLAY_TIME_PRIORITY,
					  IDLE_DISPLAY_TIME_PRIORITY,
					  idle_display_time_stk,
					  TASK_STACKSIZE,
					  NULL,
					  0);


	  /*
	  OSTaskCreateExt(task2,
					  NULL,
					  (void *)&task2_stk[TASK_STACKSIZE-1],
					  TASK2_PRIORITY,
					  TASK2_PRIORITY,
					  task2_stk,
					  TASK_STACKSIZE,
					  NULL,
					  0);
	  */
	  OSTaskCreateExt(task_led_wave,
	  					  NULL,
	  					  (void *)&task_led_ramp[TASK_STACKSIZE-1],
	  					  10,
	  					  10,
						  task_led_ramp,
	  					  TASK_STACKSIZE,
	  					  NULL,
	  					  0);

	  /*OSTaskCreateExt(taskhex,
	  					  NULL,
	  					  (void *)&task_hex_stk[TASK_STACKSIZE-1],
						  TASKHEX_PRIORITY,
						  TASKHEX_PRIORITY,
						  task_hex_stk,
	  					  TASK_STACKSIZE,
	  					  NULL,
	  					  0);
	  					  */
  while(1){

	printf("IdleCountWith100_ISRs = %ld\n ", OSIdleCtrMax);
	    //OSTimeDlyHMSM(0, 0, 3, 0);
	  OSTimeDlyHMSM(1,10,0,0);

  }

}


/* The main function creates two task and starts multi-tasking */
int main(void)
{
	OSInit(); //starts OS

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

// Tasks to do:
// Only allow the modulating LEDs to run on the actual alarm time
// Tell if we enable or disable the alarm
// Live update the 7-segment display when we are changing the time
//
