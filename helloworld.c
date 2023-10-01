
#include "xil_printf.h"

_Bool up_button_press();
_Bool down_button_press();
_Bool left_button_press();
_Bool right_button_press();
void set_digits_displayed(uint8_t digits[4], uint8_t data[4], uint8_t objectCount, char location);
void sseg_display_driver(uint8_t data[4]);
void set_position_LEDS(char objectLocation);
void set_strength_meter_LEDS(uint8_t StrengthMeterCounter);
char check_object_location(uint32_t frequencyLeft, uint32_t frequencyRight, uint8_t *StrengthMeterCounter, uint32_t defaultLeft, uint32_t defaultRight, _Bool doObjectDetect);
void count_objects(char objectLocation, char displayLocation, uint8_t *displayCount, uint32_t frequencyLeft, uint32_t frequencyRight, uint8_t locationCounts[]);
void rotate_display_location(char* displayLocation);
_Bool check_switches(char* displayLocation, uint8_t* displayCount, uint8_t locationCounts[]);

#define LEDS ( *(volatile unsigned *)0x40000000)
#define SW   ( *(volatile unsigned *)0x40000008)
#define JB   ( *(volatile unsigned *)0x40010000)
#define SGDP ( *(volatile unsigned *)0x40020000)
#define AN   ( *(volatile unsigned *)0x40020008)
#define BTNS ( *(volatile unsigned *)0x40030000)
#define ALARMREG0 ( *(volatile unsigned *)0x44A00000)
#define ALARMREG1 ( *(volatile unsigned *)0x44A00004)
#define ALARMREG2 ( *(volatile unsigned *)0x44A00008)
#define ALARMREG3 ( *(volatile unsigned *)0x44A0000C)
#define ROTARYENCREG0 ( *(volatile unsigned *)0x44A10000)
#define ROTARYENCREG1 ( *(volatile unsigned *)0x44A10004)
#define ROTARYENCREG2 ( *(volatile unsigned *)0x44A10008)
#define ROTARYENCREG3 ( *(volatile unsigned *)0x44A1000C)
#define JXADC1_RESULT_REG ( *(volatile unsigned *)0x44A20258)
#define JXADC2_RESULT_REG ( *(volatile unsigned *)0x44A2025C)

#define ADC_HIGH_THRESHOLD 0x00004FFF
#define ADC_LOW_THRESHOLD 0x00000FFF

#define CENTER_SUM_THRESHOLD 45000 // lower by 100 without usb
#define CENTER_LEFT_RIGHT_THRESHOLD 300
#define FAR_LEFT_FAR_RIGHT_THRESHOLDS 3000
#define LEFT_RIGHT_THRESHOLDS 800

int main()
{
	uint8_t currentDisplayCount = 0;
	uint8_t digits[4] = {0, 0, 0, 0};
	uint8_t data[4] = {~0x3F, ~0x3F, ~0x3F, ~0x3F};
	uint8_t StrengthMeterCount = 0;
	char currentDisplayLocation = 'F';
	char currentObjectLocation = 'N';
	_Bool lock_location = 0;

	// FL, L, C, R, FR, T
	uint8_t locationCounts[] = {0, 0, 0, 0, 0, 0};

	uint32_t sinePeakCountL = 0;
	_Bool inPeakL = 0;
	uint32_t sinePeakCountR = 0;
	_Bool inPeakR = 0;

	uint32_t defaultFrequencyDifference = -150;

	uint32_t defaultFrequencyL = 20000;
	uint32_t defaultFrequencyR = 20000;

	uint16_t totalCycles = 0;
	uint16_t cyclesToCalibrate = 2;

	_Bool doObjectDetect = 0;

	ALARMREG2 = 1000;
	ALARMREG3 = 200000000;

    while(1)
    {
    	// if alarm0 goes off
    	if ((ALARMREG1 & (1<<0)) == 1)
    	{
			ALARMREG2 = 1500; // set alarm offset to loop time

			set_digits_displayed(digits, data, currentDisplayCount, currentDisplayLocation);

			sseg_display_driver(data);

			if ((JXADC1_RESULT_REG > ADC_HIGH_THRESHOLD) && (!inPeakL))
			{
				inPeakL = 1;
				sinePeakCountL++;
			}
			else if (JXADC1_RESULT_REG <= ADC_LOW_THRESHOLD)
			{
				inPeakL = 0;
			}

			if ((JXADC2_RESULT_REG > ADC_HIGH_THRESHOLD) && (!inPeakR))
			{
				inPeakR = 1;
				sinePeakCountR++;
			}
			else if (JXADC2_RESULT_REG <= ADC_LOW_THRESHOLD)
			{
				inPeakR = 0;
			}

			lock_location = check_switches(&currentDisplayLocation, &currentDisplayCount, locationCounts);

    	}

    	// if alarm1 goes off
    	if ((ALARMREG1 & (1<<1)) == (1<<1))
    	{

			ALARMREG3 = 200000000; // set alarm offset to loop time (1 s)

			// offset first frequency
			sinePeakCountL -= defaultFrequencyDifference;

			sinePeakCountL = sinePeakCountL >> 1;
			sinePeakCountR = sinePeakCountR >> 1;

			currentObjectLocation = check_object_location(sinePeakCountL, sinePeakCountR, &StrengthMeterCount, defaultFrequencyL, defaultFrequencyR, doObjectDetect);

			if (totalCycles == cyclesToCalibrate)
			{
				defaultFrequencyL = sinePeakCountL;
				defaultFrequencyR = sinePeakCountR;
				doObjectDetect = 1;
			}

			if (!lock_location)
			{
				rotate_display_location(&currentDisplayLocation);
			}
			// this needs to come after so currentObjects gets assigned to count of new rotated displayLocation
			count_objects(currentObjectLocation, currentDisplayLocation, &currentDisplayCount, sinePeakCountL, sinePeakCountR, locationCounts);

			set_position_LEDS(currentObjectLocation);
			set_strength_meter_LEDS(StrengthMeterCount);

			//xil_printf("XADC1: %08x\n", JXADC1_RESULT_REG);
			//xil_printf("XADC2: %08x\n", JXADC2_RESULT_REG);

//			xil_printf("XADC1 Sine frequency: %04d.", (sinePeakCountL));
//			xil_printf("%d Hz\n", (sinePeakCountL%10));
//			xil_printf("XADC2 Sine frequency: %04d.", (sinePeakCountR));
//			xil_printf("%d Hz\n", (sinePeakCountR%10));
//			xil_printf("Current object position: %c\n", currentObjectLocation);
			xil_printf("D: %d\n", (int32_t) sinePeakCountL - sinePeakCountR);
			xil_printf("S: %04d.\n", (int32_t) sinePeakCountL + sinePeakCountR);
//			xil_printf("default frequency difference: %d \n", defaultFrequencyDifference);
//			xil_printf("default frequency L: %d \n", defaultFrequencyL);
//			xil_printf("default frequency R: %d \n", defaultFrequencyR);

			//xil_printf("sinePeaks: %08d\n", sinePeakCount);

			totalCycles++;

			sinePeakCountL = 0;
			inPeakL = 0;
			sinePeakCountR = 0;
			inPeakR = 0;
    	}
    }
    return 0;
}

_Bool up_button_press()
{
	static _Bool state = 0;
	static uint8_t cnt = 0;
	_Bool retval = 0;
	if( state != (BTNS & (1<<0)))
	{
		cnt++;
		if (cnt == 4)
		{
			state = (BTNS & (1<<0));
			retval = state;
		}
	}
	else
	{
		cnt = 0;
	}
	return retval;
}

_Bool down_button_press()
{
	static _Bool state = 0;
	static uint8_t cnt = 0;
	_Bool retval = 0;
	if( state != (BTNS & (1<<1)))
	{
		cnt++;
		if (cnt == 4)
		{
			state = (BTNS & (1<<1));
			retval = state;
		}
	}
	else
	{
		cnt = 0;
	}
	return retval;
}

_Bool left_button_press()
{
	static _Bool state = 0;
	static uint8_t cnt = 0;
	_Bool retval = 0;
	if( state != (BTNS & (1<<2)))
	{
		cnt++;
		if (cnt == 4)
		{
			state = (BTNS & (1<<2));
			retval = state;
		}
	}
	else
	{
		cnt = 0;
	}
	return retval;
}

_Bool right_button_press()
{
	static _Bool state = 0;
	static uint8_t cnt = 0;
	_Bool retval = 0;
	if( state != (BTNS & (1<<3)))
	{
		cnt++;
		if (cnt == 4)
		{
			state = (BTNS & (1<<3));
			retval = state;
		}
	}
	else
	{
		cnt = 0;
	}
	return retval;
}

void set_digits_displayed(uint8_t digits[4], uint8_t data[4], uint8_t objectCount, char location)
{
	static uint8_t seven_seg_code[10] = {~0x3F, ~0x06, ~0x5B, ~0x4F, ~0x66, ~0x6D, ~0x7D, ~0x07, ~0x7F, ~0x6F};
	static uint8_t position_letters[6] = {~0x71, ~0x38, ~0x39, ~0x78, ~0x50, ~0x00}; // F, L, C, t, R, " "

	uint8_t tempCount = objectCount;

	digits[0] = 0;
	digits[1] = 0;

	while(tempCount >= 10)
	{
		digits[1]++;
		tempCount -= 10;
	}
	digits[0] = tempCount;

	for (int i = 1; i >= 0; i--)
	{
		data[i] = seven_seg_code[digits[i]];
	}

	switch(location)
	{
		case 'L':
			data[3] = position_letters[1]; // L
			data[2] = position_letters[5]; // " "
			break;
		case 'R':
			data[3] = position_letters[4]; // R
			data[2] = position_letters[5]; // " "
			break;
		case 'C':
			data[3] = position_letters[2]; // C
			data[2] = position_letters[5]; // " "
			break;
		case 'F':
			data[3] = position_letters[0]; // F
			data[2] = position_letters[1]; // L
			break;
		case 'G':
			data[3] = position_letters[0]; // F
			data[2] = position_letters[4]; // R
			break;
		case 't':
			data[3] = position_letters[3]; // t
			data[2] = position_letters[5]; // " "
			break;
		default:
			// error
			data[3] = seven_seg_code[0];
			data[2] = seven_seg_code[0];
	}
}

void sseg_display_driver(uint8_t data[4])
{
	static uint16_t cnt;

	AN = ~(1<<cnt);

	SGDP = data[cnt];

	cnt = (cnt + 1) % 4;
}


_Bool check_switches(char* displayLocation, uint8_t* displayCount, uint8_t locationCounts[])
{
	char newDisplayLocation = 'N';
	uint8_t switchCount = 0;

	if ((SW & (1<<15))>>15)
	{
		newDisplayLocation = 'F';
		switchCount++;
	}
	if ((SW & (1<<14))>>14)
	{
		newDisplayLocation = 'L';
		switchCount++;
	}
	if ((SW & (1<<13))>>13)
	{
		newDisplayLocation = 'C';
		switchCount++;
	}
	if ((SW & (1<<12))>>12)
	{
		newDisplayLocation = 'R';
		switchCount++;
	}
	if ((SW & (1<<11))>>11)
	{
		newDisplayLocation = 'G';
		switchCount++;
	}
	if ((SW & (1<<10))>>10)
	{
		newDisplayLocation = 't';
		switchCount++;
	}

	if (switchCount != 1)
	{
		return 0;
	}

	*displayLocation = newDisplayLocation;

	switch(newDisplayLocation)
	{
		case 'F':
			*displayCount = locationCounts[0];
			break;
		case 'L':
			*displayCount = locationCounts[1];
			break;
		case 'C':
			*displayCount = locationCounts[2];
			break;
		case 'R':
			*displayCount = locationCounts[3];
			break;
		case 'G':
			*displayCount = locationCounts[4];
			break;
		case 't':
			*displayCount = locationCounts[5];
			break;
		default:
			*displayCount = locationCounts[0];
	}

	return 1;
}

void set_position_LEDS(char objectLocation)
{
	LEDS = 0;

	switch(objectLocation)
	{
		case 'L':
			LEDS |= (1<<14);
			break;
		case 'R':
			LEDS |= (1<<12);
			break;
		case 'C':
			LEDS |= (1<<13);
			break;
		case 'F':
			LEDS |= (1<<15);
			break;
		case 'G':
			LEDS |= (1<<11);
			break;
		case 't':
			LEDS = 0;
			break;
		default:
			// error
			LEDS = 0;
	}

}

void set_strength_meter_LEDS(uint8_t StrengthMeterCounter)
{
	// StrengthMeterCounter ranges from 1 (far left) to 11 (far right) and is used to determine which LEDS should be turned on - shows distance of object from center using LEDS
	// AND operation used as mask to ensure that 5 MSB LEDS used for current position remain on, and the LEDS from previous strength meter are shut off

	//xil_printf("%d", StrengthMeterCounter);

	switch (StrengthMeterCounter)
	{
	case 1:
		LEDS |= (0x3F << 5);
		break;
	case 2:
		LEDS |= (0x1F << 5);
		break;
	case 3:
		LEDS |= (0xF << 5);
		break;
	case 4:
		LEDS |= (0x7 << 5);
		break;
	case 5:
		LEDS |= (0x3 << 5);
		break;
	case 6:
		LEDS |= (0x1 << 5);
		break;
	case 7:
		LEDS |= (0x3 << 4);
		break;
	case 8:
		LEDS |= (0x7 << 3);
		break;
	case 9:
		LEDS |= (0xF << 2);
		break;
	case 10:
		LEDS |= (0x1F << 1);
		break;
	case 11:
		LEDS |= (0x3F << 0);
		break;
	default:
		// error or no object detected
		LEDS = 0;
	}

}

char check_object_location(uint32_t frequencyLeft, uint32_t frequencyRight, uint8_t *StrengthMeterCounter, uint32_t defaultLeft, uint32_t defaultRight, _Bool doObjectDetect)
{
	static char previousLocation = 'N';
	char newLocation = 'N';

	uint32_t frequencySum = frequencyLeft + frequencyRight;

	int16_t frequencyDeviationL = defaultLeft - frequencyLeft;
	int16_t frequencyDeviationR = defaultRight - frequencyRight;

	//xil_printf("FREQUENCY SUM: %d\n", frequencySum);
	//xil_printf("frequencyDeviationL: %d\n", frequencyDeviationL);
	//xil_printf("frequencyDeviationR: %d\n", frequencyDeviationR);

	(*StrengthMeterCounter) = 0;

	if (frequencyDeviationL > FAR_LEFT_FAR_RIGHT_THRESHOLDS) // far left
	{
		newLocation = 'F';

		if (frequencyDeviationL > 4000)
			(*StrengthMeterCounter) = 1;
		else
			(*StrengthMeterCounter) = 2;

	}
	else if (frequencyDeviationL > LEFT_RIGHT_THRESHOLDS) // left
	{
		newLocation = 'L';

		if (frequencyDeviationL > 1000)
			(*StrengthMeterCounter) = 3;
		else
			(*StrengthMeterCounter) = 4;
	}
	else if ((frequencySum <= CENTER_SUM_THRESHOLD) && (frequencyDeviationL < CENTER_LEFT_RIGHT_THRESHOLD) && (frequencyDeviationR < CENTER_LEFT_RIGHT_THRESHOLD)) // center
	{
		newLocation = 'C';

		if ((frequencyDeviationL < 75) && (frequencyDeviationL > 0))
			(*StrengthMeterCounter) = 5;
		else if ((frequencyDeviationR < 75) && (frequencyDeviationR > 0))
			(*StrengthMeterCounter) = 7;
		else
			(*StrengthMeterCounter) = 6;
	}
	else if (frequencyDeviationR > FAR_LEFT_FAR_RIGHT_THRESHOLDS) // far right
	{
		newLocation = 'G';

		if (frequencyDeviationR > 4000)
			(*StrengthMeterCounter) = 11;
		else
			(*StrengthMeterCounter) = 10;

	}
	else if (frequencyDeviationR > LEFT_RIGHT_THRESHOLDS) // right
	{
		newLocation = 'R';

		if (frequencyDeviationR > 1000)
			(*StrengthMeterCounter) = 9;
		else
			(*StrengthMeterCounter) = 8;
	}
	else
	{
		newLocation = 'N'; // no object detected
	}

	return newLocation;

}

void count_objects(char objectLocation, char displayLocation, uint8_t *displayCount, uint32_t frequencyLeft, uint32_t frequencyRight, uint8_t locationCounts[])
{
	static char previousLocation = 'C';

	if (previousLocation != objectLocation)
	{
		previousLocation = objectLocation;
		switch(objectLocation)
		{
			case 'F':
				locationCounts[0]++;
				break;
			case 'L':
				locationCounts[1]++;
				break;
			case 'C':
				locationCounts[2]++;
				break;
			case 'R':
				locationCounts[3]++;
				break;
			case 'G':
				locationCounts[4]++;
				break;
			case 'N':
				break;
			default:
				break;
		}
		locationCounts[5] = locationCounts[0] + locationCounts[1] + locationCounts[2] + locationCounts[3] + locationCounts[4];
	}

	switch(displayLocation)
	{
		case 'F':
			*displayCount = locationCounts[0];
			break;
		case 'L':
			*displayCount = locationCounts[1];
			break;
		case 'C':
			*displayCount = locationCounts[2];
			break;
		case 'R':
			*displayCount = locationCounts[3];
			break;
		case 'G':
			*displayCount = locationCounts[4];
			break;
		case 't':
			*displayCount = locationCounts[5];
			break;
		default:
			// error
			*displayCount = locationCounts[0];
	}
}

void rotate_display_location(char* displayLocation)
{
	switch(*displayLocation)
	{
		case 'F': // far left
			*displayLocation = 'L';
			break;
		case 'L': // left
			*displayLocation = 'C';
			break;
		case 'C': // center
			*displayLocation = 'R';
			break;
		case 'R': // right
			*displayLocation = 'G';
			break;
		case 'G': // far right
			*displayLocation = 't';
			break;
		case 't': // total
			*displayLocation = 'F';
			break;
		default:
			// error
			*displayLocation = 'L';
	}
}
