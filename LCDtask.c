#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"

/* include files. */
#include "GLCD.h"
#include "vtUtilities.h"
#include "LCDtask.h"

// I have set this to a large stack size because of (a) using printf() and (b) the depth of function calls
//   for some of the LCD operations
#if PRINTF_VERSION==1
#define lcdSTACK_SIZE  	8*configMINIMAL_STACK_SIZE
#else
#define lcdSTACK_SIZE		4*configMINIMAL_STACK_SIZE
#endif

// Set the task up to run every 200 ms
#define lcdWRITE_RATE_BASE	( ( portTickType ) 10 )

/* The LCD task. */
static portTASK_FUNCTION_PROTO( vLCDUpdateTask, pvParameters );

/*-----------------------------------------------------------*/

void vStartLCDTask( unsigned portBASE_TYPE uxPriority,vtLCDStruct *ptr )
{

	// Create the queue that will be used to talk to the LCD
	if ((ptr->inQ = xQueueCreate(vtLCDQLen,sizeof(vtLCDMsg))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	/* Start the task */
	portBASE_TYPE retval;
	if ((retval = xTaskCreate( vLCDUpdateTask, ( signed char * ) "LCD", lcdSTACK_SIZE, (void*)ptr, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

// If LCD_EXAMPLE_OP=0, then repeatedly write text
// If LCD_EXAMPLE_OP=1, then do a rotating ARM bitmap display
// If LCD_EXAMPLE_OP=2, then receive from a message queue and print the contents to the screen
#define LCD_EXAMPLE_OP 2
#if LCD_EXAMPLE_OP==1
// This include the file with the definition of the ARM bitmap
#include "ARM_Ani_16bpp.c"
#endif

// Convert from HSL colormap to RGB values in this weird colormap
// H: 0 to 360
// S: 0 to 1
// L: 0 to 1
// The LCD has a funky bitmap.  Each pixel is 16 bits (a "short unsigned int")
//   Red is the most significant 5 bits
//   Blue is the least significant 5 bits
//   Green is the middle 6 bits
static unsigned short hsl2rgb(float H,float S,float L)
{
	float C = (1.0 - fabs(2.0*L-1.0))*S;
	float Hprime = H / 60;
	unsigned short t = Hprime / 2.0;
	t *= 2;
	float X = C * (1-abs((Hprime - t) - 1));
	unsigned short truncHprime = Hprime;
	float R1, G1, B1;

	switch(truncHprime) {
		case 0: {
			R1 = C; G1 = X; B1 = 0;
			break;
		}
		case 1: {
			R1 = X; G1 = C; B1 = 0;
			break;
		}
		case 2: {
			R1 = 0; G1 = C; B1 = X;
			break;
		}
		case 3: {
			R1 = 0; G1 = X; B1 = C;
			break;
		}
		case 4: {
			R1 = X; G1 = 0; B1 = C;
			break;
		}
		case 5: {
			R1 = C; G1 = 0; B1 = X;
			break;
		}
		default: {
			// make the compiler stop generating warnings
			R1 = 0; G1 = 0; B1 = 0;
			VT_HANDLE_FATAL_ERROR(Hprime);
			break;
		}
	}
	float m = L - 0.5*C;
	R1 += m; G1 += m; B1 += m;
	unsigned short red = R1*32; if (red > 31) red = 31;
	unsigned short green = G1*64; if (green > 63) green = 63;
	unsigned short blue = B1*32; if (blue > 31) blue = 31;
	unsigned short color = (red << 11) | (green << 5) | blue;
	return(color); 
}

// This is the actual task that is run
static portTASK_FUNCTION( vLCDUpdateTask, pvParameters )
{
	portTickType xUpdateRate, xLastUpdateTime;
	unsigned short screenColor;
	#if LCD_EXAMPLE_OP==0
	unsigned short tscr;
	static char scrString[40];
	unsigned char curLine = 0;
	float hue=0, sat=0.2, light=0.2;
	#elif LCD_EXAMPLE_OP==1
	unsigned char picIndex = 0;
	#elif LCD_EXAMPLE_OP==2
	vtLCDMsg msgBuffer;
	unsigned char curLine = 0;
	#endif
	vtLCDStruct *lcdPtr = (vtLCDStruct *) pvParameters;

	/* Initialize the LCD */
	GLCD_Init();
	GLCD_Clear(White);

	
	int i;
	for(i = 0; i < 320; i++){
		GLCD_PutPixel(i,130);
	}
	for(i=0; i < 280; i++){
			GLCD_PutPixel(1,i);
	}

	//Graphing Vars
	char cords[30];
	char xcord[10];
	char ycord[10];
	char * pch;

	float y;
	float x;
	float x_prev;
	

	//end graphing vars

	int time = 0;
	int value = 0;
	char dig;
	char dig1;
	char dig2;

	// Scale the update rate to ensure it really is in ms
	xUpdateRate = 10* lcdWRITE_RATE_BASE / portTICK_RATE_MS;

	/* We need to initialise xLastUpdateTime prior to the first call to 
	vTaskDelayUntil(). */
	xLastUpdateTime = xTaskGetTickCount();
	// Note that srand() & rand() require the use of malloc() and should not be used unless you are using
	//   MALLOC_VERSION==1
	#if MALLOC_VERSION==1
	srand((unsigned) 55); // initialize the random number generator to the same seed for repeatability
	#endif
	// Like all good tasks, this should never exit


	char test1[20];
	xQueueReceive(lcdPtr->inQ3, &test1, portMAX_DELAY);
	printf("\n%s\n",test1);

	xQueueReceive(lcdPtr->inQ3, &test1, portMAX_DELAY);
	printf("\n%s\n",test1);

	vTaskDelay(10000/portTICK_RATE_MS);

	for(;;)
	{	
#if LCD_EXAMPLE_OP==0
		/* Ask the RTOS to delay reschduling this task for the specified time */
		vTaskDelayUntil( &xLastUpdateTime, xUpdateRate );

		// Find a new color for the screen by randomly (within limits) selecting HSL values
		#if MALLOC_VERSION==1
		hue = rand() % 360;
		sat = (rand() % 1024) / 1023.0; sat = sat * 0.5; sat += 0.5;
		light = (rand() % 1024) / 1023.0;	light = light * 0.8; light += 0.10;
		#else
		hue = (hue + 1); if (hue >= 360) hue = 0;
		sat+=0.01; if (sat > 1.0) sat = 0.20;
		light+=0.03; if (light > 1.0) light = 0.20;
		#endif
		screenColor = hsl2rgb(hue,sat,light);
		// Now choose a complementary value for the text color
		hue += 180;
		if (hue >= 360) hue -= 360;
		tscr = hsl2rgb(hue,sat,light);
		GLCD_SetTextColor(tscr);
		GLCD_SetBackColor(screenColor);
		/* create a string for printing to the LCD */
		sprintf(scrString,"LCD: %d %d %d",screenColor,(int)xUpdateRate,(int)xLastUpdateTime);
		GLCD_ClearLn(curLine,1);
		GLCD_DisplayString(curLine,0,1,(unsigned char *)scrString);
		curLine++;
		if (curLine == 10) {
			/* clear the LCD at the end of the screen */
			GLCD_Clear(screenColor);
			curLine = 0;
		}

		printf("BackColor (RGB): %d %d %d - (HSL) %3.0f %3.2f %3.2f \n",
			(screenColor & 0xF800) >> 11,(screenColor & 0x07E0) >> 5,screenColor & 0x001F,
			hue,sat,light);
		printf("TextColor (RGB): %d %d %d\n",
			(tscr & 0xF800) >> 11,(tscr & 0x07E0) >> 5,tscr & 0x001F);

		// Here is a way to do debugging output via the built-in hardware -- it requires the ULINK cable and the
		//   debugger in the Keil tools to be connected.  You can view PORT0 output in the "Debug(printf) Viewer"
		//   under "View->Serial Windows".  You have to enable "Trace" and "Port0" in the Debug setup options.  This
		//   should not be used if you are using Port0 for printf()
		// There are 31 other ports and their output (and port 0's) can be seen in the "View->Trace->Records"
		//   windows.  You have to enable the prots in the Debug setup options.  Note that unlike ITM_SendChar()
		//   this "raw" port write is not blocking.  That means it can overrun the capability of the system to record
		//   the trace events if you go too quickly; that won't hurt anything or change the program execution and
		//   you can tell if it happens because the "View->Trace->Records" window will show there was an overrun.
		vtITMu16(vtITMPortLCD,screenColor);

#elif 	LCD_EXAMPLE_OP==1
		/* Ask the RTOS to delay reschduling this task for the specified time */
		vTaskDelayUntil( &xLastUpdateTime, xUpdateRate );
  		/* go through a  bitmap that is really a series of bitmaps */
		picIndex = (picIndex + 1) % 9;
		GLCD_Bmp(99,99,120,45,(unsigned char *) &ARM_Ani_16bpp[picIndex*(120*45*2)]);

#elif 	LCD_EXAMPLE_OP==2
		// wait for a message from another task telling us to send/recv over i2c
		if (xQueueReceive(lcdPtr->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			VT_HANDLE_FATAL_ERROR(0);
		}

		if(time == 320){
		  GLCD_Clear(White);
		  time=0;
		}
		if(value == 240){
          value = 0;
		}
	//	GLCD_SetWindow (unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
		//Log that we are processing a message
		vtITMu8(vtITMPortLCDMsg,msgBuffer.length);
		// Decide what color and then clear the line
		GLCD_SetTextColor(Blue);
		GLCD_SetBackColor(Green);
	
	    
		// show the text
	//	GLCD_Bargraph       (unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int val);
    /*    int big = value/100;
		big = 5;
	    dig = (char)('0' + big);
		int middle = (value/10) - (big*10);
		middle = 6;
		dig1 = (char)('0' + middle);
		int small = value -(big*100)-(middle*10);
		small = 7;
		dig1 = (char)('0' + small);	 */
		//	GLCD_ClearLn(9,1);
	//	GLCD_DisplayChar (9, 1, 1, big);
	//	GLCD_DisplayChar (9, 2, 1, middle);
	//	GLCD_DisplayChar (9, 3, 1, small);
	
		//GLCD_DisplayString(curLine,0,1,(unsigned char *)msgBuffer.buf);


	//CONVERTING DATA FOR GRAPHING
	sprintf(cords,"%s",(char *) msgBuffer.buf);
	//GLCD_DisplayString(2,0,1,(unsigned char *)cords);
	
	pch = strtok(cords,",");
	sprintf(xcord,"%s",pch);
	pch = strtok(NULL,",");
	sprintf(ycord,"%s",pch);
	pch = strtok(NULL,",");
//	//GLCD_DisplayString(3,0,1,(unsigned char *)xcord);
//	//GLCD_DisplayString(4,0,1,(unsigned char *)ycord);
//
	x = (int)atof(xcord);
	y = (int)atof(ycord);

//	printf("%d %d\n",x,y);

	y=-y*.25;
	y=y+130;
	

	if(x == 0){
		GLCD_Clear(White);
		
		for(i = 0; i < 320; i++){
			GLCD_PutPixel(i,130);
		}
		for(i=0; i < 280; i++){
			GLCD_PutPixel(1,i);
		}
		
	}

	//x=(x*10); //10

	GLCD_PutPixel(x,y);


//GLCD_DisplayString(curLine,0,1,(unsigned char *)msgBuffer.buf);


	/*******************************************************************************
* Draw bargraph                                                                *
*   Parameter:      x:        horizontal position                              *
*                   y:        vertical position                                *
*                   w:        maximum width of bargraph (in pixels)            *
*                   val:      value of active bargraph (in 1/1024)             *
*   Return:                                                                    *
*******************************************************************************/

	   // GLCD_Bargraph(100,100,320,240,1);
	 //  GLCD_PutPixel(time,value);
	   //time+=4;
	  // value+=2;
//		curLine++;
//		if (curLine == 10) {
//			/* clear the LCD at the end of the screen */
//			GLCD_Clear(Blue);
//			curLine = 0;
//		} 	  
#else
		Bad setting
#endif	
	}
}

