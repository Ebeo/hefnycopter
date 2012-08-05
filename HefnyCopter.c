/*
 * XXcontrol_KRHefny.c
 * 
 * Created: 20-Jul-12 5:40:52 PM
 *  Author: M.Hefny
 */ 
// **************************************************************************
// * 						GNU GPL V2 notice
// **************************************************************************
// * This is a QuadCopter code compatible with KK boards. 
// * The code is inspired by Inspired by KKmulticopter Based on assembly code by Rolf R Bakke, 
// * and C code by Mike Barton & NeXtCopterPlus
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// * 
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// * GNU General Public License for more details.
// * 
// * You should have received a copy of the GNU General Public License
// * along with this program.If not, see <http://www.gnu.org/licenses/>.
// * 
// * NB: Summary - all derivative code MUST be released with the source code!
// *
// **************************************************************************
// Version History
// 0.1 
//		* Main Skeleton
// 0.2 
//		* QuadCopter is flying
// 0.3 
//		* Settings:
//		*	- Reset
//		*	- Stick Center
//		*	- Gyro Reversing
// 0.4 
//		* Settings:
//		*	- Calibrate ESC
//		* X-QUAD Switching without the need of board orientation
//		* update misleading code comments 
// 0.45
//		* Gain for Roll & PITCH read from PITCH POT only.
//		* Fixing issues:
//			Atomic TCNT1 timer read.
//			Loop of PWM_LOW_PULSE_INTERVAL is back dependent on TCNT2 because we need ATOMIC read if we use TCNT2 that disables interrupts.
//			ISR - Throttle time variable corrected.
//			Min Motor Values =10 moved to the correct position.

#define QUAD_COPTER
/*

Quad
                    M1 CW
                     |
                     |
                     |
                   +---+
		 CCW M2----|   |----M3 CCW
                   +---+
                     |
                     |
                     |
                   M4 CW
				   
				   
Quad-X
       
           M1 CW       M3 CCW
			  \          /
               \        / 
                \ ---  /
                 |    |
                / ---  \
               /        \ 
			  /          \ 
          M2 CCW        M4 CW
		  
		  NOTE: X-QUAD motors order are different from many other code on Internet such as XXController & QuadControllerV#_#

*/

// Adjust these:
// 		down if you have too much gyro assistance
// 		up if you have maxxed your gyro gain 
#define ROLL_GAIN_MULTIPLIER 		1 //3	// 2
#define PITCH_GAIN_MULTIPLIER 		1 //3	// 2
#define YAW_GAIN_MULTIPLIER 		1 //3	// 2

#define ADC_GAIN_DIVIDER			800			// doubling value will decrease POTS range to half.
#define MAX_GYRO_VALUE				500			// max value is 1024 /2  u can set it to [300 , 200] the higher the most sensitive



/* ----------- Main Code -----------  */

#include <avr/io.h>  
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/interrupt.h> 

#include "typedefs.h"
#include "io_cfg.h"
#include "Commons.h"
#include "Headers.h"
#include "Init.h"
#include "motors.h"
#include "ISR.h"
#include "ADC.h"
#include "Pots.h"


volatile BOOL RxChannelsUpdatingFlag;

//#define Debugging

//#define Debugging used in debugging to skip initialization
/*
	Main Function
*/
int main(void)
{
	setup();
	
#ifndef Debugging
	
	ReadGainValues();
	ResetValues ();
	StickCenter();
	GyroRevereing();
	ESCThrottleCalibration();
#endif
	// flash LED
	LED = 0;
	FlashLED (100,2);
	CalibrateGyros();
	Armed=false;
	
	
	while (1)
	{
		loop();
	}

	return 1;
}


void setup(void)
{
	
	pmm_out_step=0;
	
	//uint16_t i;	// nb was uint8_t, must be uint16_t for TRI
	uint16_t RxChannel1ZeroOffset, RxChannel2ZeroOffset, RxChannel4ZeroOffset;

	MCUCR |= (1<<PUD);	// Pull-up Disable


	InitIO();
	
	InitTimers();
	
	Initial_EEPROM_Config_Load();					// loads config at start-up 

	Init_ADC();

	GyroCalibrated = false;
	Armed = false;
	RxChannelsUpdatingFlag = 0;

	
	RxChannel1 = Config.RxChannel1ZeroOffset;		// prime the channels 1520;
	RxChannel2 = Config.RxChannel2ZeroOffset;		// 1520;
	RxChannel3 = Config.RxChannel3ZeroOffset;		// 1120;
	RxChannel4 = Config.RxChannel4ZeroOffset;		// 1520;

	
	sei();											// Global Interrupts 
	
	
}


uint16_t TCNT1_X_snapshot=0;
int16_t cROLL;
int16_t cPITCH;
int16_t cYAW;
int16_t fROLL;
int16_t fPITCH;
int16_t fYAW;
bool bXQuadMode = false;	
bool bResetTCNR1_X = true;
void loop(void)
{
	
	bResetTCNR1_X = true;
	

	RxGetChannels();
	
	
	if (RxInCollective < STICKThrottle_ARMING) 
	{	// Throttle is LOW
		// Here you can add code without caring about delays. As there quad is already off and on land.
		// here we test different positions of sticks to enable arm/disarm, Quad/X-Quad
		
		ReadGainValues(); // keep reading values of POTS here. as we can change the value while quad is armed. but sure it is on land and motors are off.
		// DisArm Check
		if ((Armed == true) && (RxInYaw < STICK_LEFT))
		{
			bResetTCNR1_X  = false;
			if (TCNT1_X_snapshot==0)  TCNT1_X_snapshot = TCNT1_X; // start counting
			if ( (TCNT1_X- TCNT1_X_snapshot) > STICKPOSITION_MIN )
			{
				Armed = false;
				LED = 0;
				FlashLED (LED_LONG_TOGGLE,4);
				TCNT1_X_snapshot =0; // reset timer
			}
		}
		
		// Arm Check
		if ((Armed == false) && (RxInYaw > STICK_RIGHT))
		{
			bResetTCNR1_X = false;
			if (TCNT1_X_snapshot==0)  TCNT1_X_snapshot = TCNT1_X; // start counting
			if ( (TCNT1_X- TCNT1_X_snapshot) > STICKPOSITION_MIN )
			{
				Armed = true;
				LED = 1;
				FlashLED (LED_LONG_TOGGLE,4);
				CalibrateGyros();
				ReadGainValues();
				FlashLED (LED_SHORT_TOGGLE,4);
				TCNT1_X_snapshot =0; // reset timer
			}		
		}
		
		
		if (!Armed)
		{	//set modes Quad , X-Quad
		
			if ((RxInRoll > STICK_RIGHT))
			{	// X-QUAD MODE
				bResetTCNR1_X = false;
				if (TCNT1_X_snapshot==0)  TCNT1_X_snapshot = TCNT1_X; // start counting
				if ( (TCNT1_X- TCNT1_X_snapshot) > STICKPOSITION_MIN )
				{
					bXQuadMode = true;
					LED = 0;
					FlashLED (100,8);
					TCNT1_X_snapshot =0; // reset timer
				}		

			
			}
			else  if ((RxInRoll < STICK_LEFT))
			{	// QUAD COPTER MODE
				bResetTCNR1_X = false;
				if (TCNT1_X_snapshot==0)  TCNT1_X_snapshot = TCNT1_X; // start counting
				if ( (TCNT1_X- TCNT1_X_snapshot) > 16 )
				{
					bXQuadMode = false;
					LED = 0;
					FlashLED (LED_LONG_TOGGLE,4);
					TCNT1_X_snapshot =0; // reset timer
				}		

			
			} 
			
			
		}
	
		// Stop motors if Throttle Stick is less than minimum.
		MotorOut1 = 0;
		MotorOut2 = 0;
		MotorOut3 = 0;
		MotorOut4 = 0;
	}
	else
	{	// Throttle stick is NOT Down
		
		if (!Armed)
		{  // However we are still DisArmed
			MotorOut1 = 0;
			MotorOut2 = 0;
			MotorOut3 = 0;
			MotorOut4 = 0;
		}
		else
		{	// Armed & Throttle Stick > MIN . . . We should Fly now.
			if (RxInCollective <( STICKThrottle_ARMING - 20)) // calibrate again before leaving ground to average vibrations.
			{
				CalibrateGyros();
			}
				
	
			MotorOut1 = RxInCollective;
			MotorOut2 = RxInCollective;
			MotorOut3 = RxInCollective;
			MotorOut4 = RxInCollective;		
	
			ReadGyros();
			ReadGainValues();
			
			
		
			gyroADC_updated[ROLL]	= (gyroADC[ROLL]);
			gyroADC_updated[PITCH]	= (gyroADC[PITCH]);
			gyroADC_updated[YAW]	= (gyroADC[YAW]);
			
			
			// LIMIT GYRO
			if (gyroADC_updated[PITCH]> MAX_GYRO_VALUE)		gyroADC_updated[PITCH] = MAX_GYRO_VALUE;
			if (gyroADC_updated[PITCH]< -MAX_GYRO_VALUE)	gyroADC_updated[PITCH] = -MAX_GYRO_VALUE;
			if (gyroADC_updated[ROLL]> MAX_GYRO_VALUE)		gyroADC_updated[ROLL] = MAX_GYRO_VALUE;
			if (gyroADC_updated[ROLL]< -MAX_GYRO_VALUE)		gyroADC_updated[ROLL] = -MAX_GYRO_VALUE;
			if (gyroADC_updated[YAW]> MAX_GYRO_VALUE)		gyroADC_updated[YAW] = MAX_GYRO_VALUE;
			if (gyroADC_updated[YAW]< -MAX_GYRO_VALUE)		gyroADC_updated[YAW] = -MAX_GYRO_VALUE;
			
			// calculate PITCH
				cPITCH   = gyroADC_updated[PITCH];
				cPITCH  *= (GainInADC[PITCH]    * PITCH_GAIN_MULTIPLIER);
				cPITCH  /= ADC_GAIN_DIVIDER;
				
				// calculate ROLL
				cROLL    = gyroADC_updated[ROLL];							
				cROLL   *= (GainInADC[PITCH] /*GainInADC[ROLL]*/  * ROLL_GAIN_MULTIPLIER);		
				cROLL   /= ADC_GAIN_DIVIDER;	
				
				// calculate YAW
				cYAW     = gyroADC_updated[YAW]; 
				cYAW	 *= (GainInADC[YAW] * YAW_GAIN_MULTIPLIER); 
				cYAW    /= ADC_GAIN_DIVIDER;
				
					
				// Add ROLL [chk reverse - add to RX - update motors]
				if (Config.RollGyroDirection == GYRO_REVERSED) cROLL = cROLL * (-1);	
				MotorOut2 -= cROLL;
				MotorOut3 += cROLL;
		
				// Add PITCH [chk reverse - add to RX - update motors]
				if (Config.PitchGyroDirection == GYRO_REVERSED) cPITCH = cPITCH * (-1);	
				MotorOut1 -= cPITCH;
				MotorOut4 += cPITCH;
		
				// Add YAW [chk reverse - add to RX - update motors]
				if (Config.YawGyroDirection== GYRO_REVERSED) cYAW = cYAW * (-1);	
				MotorOut1 += cYAW;
				MotorOut2 -= cYAW;
				MotorOut3 -= cYAW;
				MotorOut4 += cYAW;
				
				fROLL = (RxInRoll >> 2); 
				fPITCH = (RxInPitch >> 2);
				fYAW =	(RxInYaw >> 2);
				
			if (bXQuadMode==true)
			{
							
				MotorOut1 += fROLL ;
				MotorOut2 += fROLL ;
				MotorOut3 -= fROLL ;
				MotorOut4 -= fROLL ;
				
				MotorOut1 += fPITCH;
				MotorOut2 -= fPITCH;
				MotorOut3 += fPITCH;
				MotorOut4 -= fPITCH;
				
				MotorOut1 -= fYAW;
				MotorOut2 += fYAW;
				MotorOut3 += fYAW;
				MotorOut4 -= fYAW;
			}
			else
			{
				MotorOut2 += fROLL ;
				MotorOut3 -= fROLL ;
				
				MotorOut1 += fPITCH ;
				MotorOut4 -= fPITCH ;
		
				MotorOut1 -= fYAW ;
				MotorOut2 += fYAW ;
				MotorOut3 += fYAW ;
				MotorOut4 -= fYAW ;
			}
			
			
			// Save motors from turning-off
			if (MotorOut1<10) MotorOut1=10;
			if (MotorOut2<10) MotorOut2=10;
			if (MotorOut3<10) MotorOut3=10;
			if (MotorOut4<10) MotorOut4=10;
			
			
		} // End of ARMED & Throttle > Minimum
				
			
	}  // End of Throttle stick is NOT Down [Armed Could be True or not]
	

	output_motor_ppm();
	
	if (bResetTCNR1_X==true)
	{
		TCNT1_X_snapshot= 0; // reset timeout
	}
		
				
}	



//--- Get and scale RX channel inputs ---
void RxGetChannels(void)
{
	
	
	static int16_t RxChannel;

	while ( RxChannelsUpdatingFlag );

	RxChannel = RxChannel1;
	RxChannel -= Config.RxChannel1ZeroOffset;				// normalise   [ - 0 + ]
	RxInRoll = (RxChannel >> StickDivFactor);				//   -250:250  "

	while ( RxChannelsUpdatingFlag );

	RxChannel = RxChannel2;
	RxChannel -= Config.RxChannel2ZeroOffset;				// normalise	[ - 0 + ]
	RxInPitch = (RxChannel >> StickDivFactor);				//     "

	while ( RxChannelsUpdatingFlag );

	RxChannel = RxChannel3;
	RxChannel -= Config.RxChannel3ZeroOffset;				// scale 0->100	[  0  + ]
	RxInCollective = (RxChannel >> 3);						// 

	while ( RxChannelsUpdatingFlag );

	RxChannel = RxChannel4;
	RxChannel -= Config.RxChannel4ZeroOffset;				// normalise	[ - 0 + ]
	RxInYaw = (RxChannel >> StickDivFactor);				//     "
	
}




void delay_us(uint8_t time)            /* time delay for us */
{ 
 while(time--)
 {
	asm volatile ("NOP"); asm volatile ("NOP"); 
	asm volatile ("NOP"); asm volatile ("NOP"); 
	asm volatile ("NOP"); asm volatile ("NOP"); 
	asm volatile ("NOP"); 
 }
}

void delay_ms(uint16_t time)
{
	uint8_t i;
	while(time--)
	{
		for(i=0;i<10;i++) delay_us(100);
	}
}


void FlashLED (msDuration, Times)
{
	bool CurrentLED = LED;
	
	for (int i=0; i< Times; ++i)
	{
		LED = ~LED;
		delay_ms(msDuration);
		LED = ~LED;
		delay_ms(msDuration);		
	}
	
	LED = CurrentLED;
	
}


int16_t FastDiv (int16_t x, int y)
{
	
	for (int i=0;i<y;++i)
	{
		
		x = x >> 1;
	}
	
	return x;
}


int16_t FastMult (int x, int y)
{
	
	for (int i=0;i<y;++i)
	{
		
		x = x << 1;
	}
	
	return x;
}