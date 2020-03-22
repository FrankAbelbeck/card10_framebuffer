/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-03-08
 * 
 * @section License
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @section Description
 * 
 * Demo/testbed for the card10 font library.
 */
#include <stdio.h> // uses printf()
#include <stdbool.h>

#include "epicardium.h" // card10 API access

// #include "FreeRTOS.h"
// #include "task.h"
#include "mxc_delay.h"

#include "faSurface.h" // custom bitmap surface lib
#include "faFramebuffer.h" // custom framebuffer access lib
#include "faFontFile.h" // custom font lib
#include "faReadPng.h" // custom png reader lib


#define INCLUDE_vTaskDelayUntil   1

#define T_TASK_MS    50
#define T_TASK_MUSEC 50000

#define N_SAMPLES_MAG 200
#define N_SAMPLES_ORIENT 200


void doExit(char *reason, int numError) {
	puts(reason);
	epic_bme680_deinit();
	epic_max86150_disable_sensor();
	epic_max30001_disable_sensor();
	epic_exit(numError);
}


uint8_t weeksInYear(uint16_t year) {
	uint8_t pThis = (year + year/4 - year/100 + year/400) % 7;
	year--;
	uint8_t pPrev = (year + year/4 - year/100 + year/400) % 7;
	if (pThis == 4 || pPrev == 3)
		return 53;
	else
		return 52;
}

void makeTime(uint32_t timestamp, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second, uint16_t *dayOfYear, uint8_t *dayOfWeek, uint8_t *weekOfYear) {
	// calculate date; in-depth explanation see https://howardhinnant.github.io/date_algorithms.html#civil_from_days
	// calculate days since the epoch 1970-01-01 shifted to 0000-03-01
	uint32_t numDay = timestamp / 86400 + 719468;
	uint32_t era = numDay / 146097;
	uint32_t dayOfEra = numDay % 146097;
	uint32_t yearOfEra = (dayOfEra - dayOfEra/1460 + dayOfEra/36524 - dayOfEra/146096) / 365;
	*dayOfYear = dayOfEra - (365*yearOfEra + yearOfEra/4 - yearOfEra/100);
	uint8_t monthOfYear = (5*(*dayOfYear) + 2)/153;
	*day = *dayOfYear - (153*monthOfYear+2)/5 + 1;
	*month = monthOfYear + (monthOfYear < 10 ? 3 : -9);
	*year = yearOfEra + era*400 + (*month < 3);
	
	// calculate h/min/sec from seconds since midnight
	uint32_t secsDay = timestamp % 86400;
	*hour = secsDay / 3600;
	*minute = (secsDay % 3600) / 60;
	*second = secsDay % 60;
	
	// apply Sakamoto's week-of-day algorithm (cf. Wikipedia)
	uint16_t y = *year - (*month < 3);
	uint8_t  t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
	*dayOfWeek = (y + y/4 - y/100 + y/400 + t[*month-1] + *day) % 7;
	// convention for weekday numbering: Mon=1..., Sun=7 (ISO 8601)
	// Sakamoto's numbering: Sun=0...,Sat=6; thus set 0->7 and all's fine
	if (*dayOfWeek == 0) *dayOfWeek = 7;
	
	// calculate ISO week number
	*weekOfYear = (*dayOfYear - *dayOfWeek + 10) / 7;
	if (*weekOfYear < 1)
		*weekOfYear = weeksInYear(*year-1);
	else if (*weekOfYear > weeksInYear(*year))
		*weekOfYear = 1;
}




int main(int argc, char **argv) {
	// variables
	union disp_framebuffer *framebuffer = NULL;
	SurfaceMod *mask = NULL;
	Surface *background = NULL;
	Surface *frontbuffer = NULL;
// 	Surface *sprite = NULL;
// 	FontFileData *fontLarge = NULL;
	FontFileData *fontTiny = NULL;
	
	uint8_t buttons;
// 	uint8_t buttonsPressed;
	uint8_t buttonsReleased;
	uint8_t buttonsOld = 0;
	
// 	Point pHour = createPoint(0,8);
// 	Point pMinute = createPoint(96,8);
// 	Point pSecondColon = createPoint(64,8);
// 	Point pDate = createPoint(0,0);
// 	Point pBattery = createPoint(115,0);
// 	Point pClimate = createPoint(0,72);
	Point p0 = createPoint(0,0);
	Point p1 = createPoint(0,9);
	Point p2 = createPoint(0,18);
	Point p3 = createPoint(0,45);
	Point p4 = createPoint(0,54);
	Point p5 = createPoint(0,63);
	Point p6 = createPoint(0,72);
	
	uint16_t year,dayOfYear;
	uint8_t month,day,hour,minute,second,dayOfWeek,weekOfYear;
	
	struct bme680_sensor_data dataClimate;
	
	float voltage,current;
	
	int sdMagSensor,sdOrientSensor;
	struct bhi160_sensor_config  cfgMagSensor;
	struct bhi160_sensor_config  cfgOrientSensor;
	struct bhi160_data_vector    dataMagSensor[N_SAMPLES_MAG];
	struct bhi160_data_vector    dataOrientSensor[N_SAMPLES_ORIENT];
	
	puts("starting fontdemo...");
	
	// set up font
// 	puts("loading font");
// 	fontLarge = fontFileLoad("png/faLargeDigits.bin");
// 	if (fontLarge == NULL) doExit("could not load faLargeFont",-1);
// 	fontLarge->distChar = 0;
// 	fontLarge->distLine = 0;
// 	fontLarge->colour = MKRGB565(192,224,192);
// 	fontLarge->alpha = 192;
// 	fontLarge->colourBg = 0;
// 	fontLarge->alphaBg = 0;
// 	fontLarge->mode = BLEND_OVER;
	
	fontTiny = fontFileLoad("png/faTinyFont.bin");
	if (fontTiny == NULL) doExit("could not load faTinyFont",-1);
	fontTiny->distChar = 1;
	fontTiny->distLine = 1;
	fontTiny->colour = MKRGB565(0,255,0);
	fontTiny->alpha = 255;
	fontTiny->colourBg = 0;
	fontTiny->alphaBg = 0;
	fontTiny->mode = BLEND_OVER;
	
	// prepare surface update mask
	puts("creating update mask");
	mask = surfaceModConstruct(DISP_HEIGHT);
	if (mask == NULL) doExit("could not set up update mask",-1);
	
	// prepare framebuffer and sprite
	puts("creating framebuffer");
	framebuffer = framebufferConstruct(0);
	if (framebuffer == NULL) doExit("could not set up framebuffer",-1);
	
	// set up background image
	background = pngDataLoad("png/earthrise.png");
	if (background == NULL) doExit("could not set up background surface",-1);
	
	// set up front buffer surface
	puts("creating frontbuffer surface");
	frontbuffer = surfaceClone(background);
	if (frontbuffer == NULL) doExit("could not set up frontbuffer surface",-1);
	
	puts("initialising BME680 climate sensor");
	switch (epic_bme680_init()) {
		case -EFAULT:
			doExit("BME680 init error: NULL pointer",-1);
			break; // unnecessary, but stops extra fallthrough warnings
		case -EINVAL:
			doExit("BME680 init error: invalid config",-1);
			break; // unnecessary, but stops extra fallthrough warnings
		case -EIO:
			doExit("BME680 init error: communication failed",-1);
			break; // unnecessary, but stops extra fallthrough warnings
		case -ENODEV:
			doExit("BME680 init error: device not found",-1);
			break; // unnecessary, but stops extra fallthrough warnings
	}
	
	puts("initialising BHI160 magnetometer");
	cfgMagSensor.sample_buffer_len = N_SAMPLES_MAG;
	cfgMagSensor.sample_rate = 4;
	cfgMagSensor.dynamic_range = 1;
	sdMagSensor = epic_bhi160_enable_sensor(BHI160_MAGNETOMETER,&cfgMagSensor);
	switch (sdMagSensor) {
		case -EBUSY:
			doExit("BHI160 magnetometer init error: task busy",-1);
			break;
		default:
			fputs("Got magnetometer descriptor ",stdout); printInt(sdMagSensor); puts("");
	}
	
	puts("initialising BHI160 orientation sensor");
	cfgOrientSensor.sample_buffer_len = N_SAMPLES_ORIENT;
	cfgOrientSensor.sample_rate = 4;
	cfgOrientSensor.dynamic_range = 2;
	sdOrientSensor = epic_bhi160_enable_sensor(BHI160_ORIENTATION,&cfgOrientSensor);
	switch (sdOrientSensor) {
		case -EBUSY:
			doExit("BHI160 orientation sensor init error: task busy",-1);
			break;
		default:
			fputs("Got orientation sensor descriptor ",stdout); printInt(sdMagSensor); puts("");
	}
	
	puts("starting loop");
	
// 	TickType_t xDelay = pdMS_TO_TICKS(T_TASK_MS);
// 	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint64_t t0;
	uint16_t dt = 0;
	bool isrunning = true;
	bool domeasure = true;
	int retval;
	while (isrunning) {
		t0 = epic_rtc_get_milliseconds();
		
		//
		// check buttons
		//
		buttons = epic_buttons_read(BUTTON_LEFT_BOTTOM | BUTTON_RIGHT_BOTTOM | BUTTON_LEFT_TOP | BUTTON_RIGHT_TOP);
// 		buttonsPressed = (buttonsOld ^ buttons) & buttons;
		buttonsReleased = (buttonsOld ^ buttons) & buttonsOld;
		if (buttonsReleased & BUTTON_RIGHT_TOP) {
			// select button released
			isrunning = false;
		} else if (buttonsReleased & BUTTON_LEFT_BOTTOM) {
			// left menu button released
			domeasure = false;
		} else if (buttonsReleased & BUTTON_RIGHT_BOTTOM) {
			// right menu button released
			domeasure = true;
		} else if (buttonsReleased & BUTTON_LEFT_TOP) {
			// reset button released; try to clean up before app terminates
			doExit("exiting fontdemo (reset button pressed)",0);
		}
		buttonsOld = buttons;
		
		// get time and calculate Y-M-D h:m:s
		makeTime(epic_rtc_get_seconds(),&year,&month,&day,&hour,&minute,&second,&dayOfYear,&dayOfWeek,&weekOfYear);
		
		// read sensors
		if (domeasure) {
			epic_bme680_read_sensors(&dataClimate);
			epic_read_battery_voltage(&voltage);
			epic_read_battery_current(&current);
			retval = epic_stream_read(sdMagSensor,&dataMagSensor,sizeof(dataMagSensor));
			switch (retval) {
				case -ENODEV:
					puts("magnetometer currently unavailable");
					break;
				case -EBADF:
					puts("magnetometer descriptor unknown");
					break;
				case -EINVAL:
					puts("magnetometer size-to-read invalid");
					break;
				case -EBUSY:
					puts("magnetometer currently busy");
					break;
				default:
					if (retval < N_SAMPLES_MAG) {
						fputs("read only ",stdout); printInt(retval); puts(" magnetometer samples");
					}
			}
			retval = epic_stream_read(sdOrientSensor,&dataOrientSensor,sizeof(dataOrientSensor));
			switch (retval) {
				case -ENODEV:
					puts("Orientation sensor currently unavailable");
					break;
				case -EBADF:
					puts("Orientation sensor descriptor unknown");
					break;
				case -EINVAL:
					puts("Orientation sensor size-to-read invalid");
					break;
				case -EBUSY:
					puts("Orientation sensor currently busy");
					break;
				default:
					if (retval < N_SAMPLES_ORIENT) {
						fputs("read only ",stdout); printInt(retval); puts(" orientation sensor samples");
					}
			}
		}
		
		// print information
// 		fontFilePrint(frontbuffer,mask,fontLarge,pHour,"%02i",hour);
// 		fontFilePrint(frontbuffer,mask,fontLarge,pMinute,"%02i",minute);
// 		fontFilePrint(frontbuffer,mask,fontLarge,pSecondColon,":");
		
// 		fontFilePrint(frontbuffer,mask,fontTiny,pDate,"%04i-%02i-%02i",year,month,day);
// 		fontFilePrint(frontbuffer,mask,fontTiny,pBattery,"%i.%i V",(int)voltage,(int)((voltage-(int)voltage)*10));
// 		fontFilePrint(frontbuffer,mask,fontTiny,pClimate,"%i °C | %i %%rel | %i hPa",(int)dataClimate.temperature,(int)(dataClimate.humidity),(int)dataClimate.pressure);
// 		printInt((int)climateData.gas_resistance);
		
		fontFilePrint(frontbuffer,mask,fontTiny,p0,"%04i-%02i-%02i T %02i:%02i:%02i",year,month,day,hour,minute,second);
		fontFilePrint(frontbuffer,mask,fontTiny,p1,"%i.%i V   %4i mA",(int)voltage,(int)((voltage-(int)voltage)*10),(int)(current/1000));
		fontFilePrint(frontbuffer,mask,fontTiny,p2,"%2i °C   %2i %%rel   %4i hPa",(int)dataClimate.temperature,(int)dataClimate.humidity,(int)dataClimate.pressure);
		fontFilePrint(frontbuffer,mask,fontTiny,p3,"gas: %i Ω",(int)dataClimate.gas_resistance);
		fontFilePrint(frontbuffer,mask,fontTiny,p4,"mag: %i,%i,%i",dataMagSensor[0].x,dataMagSensor[0].y,dataMagSensor[0].z);
		fontFilePrint(frontbuffer,mask,fontTiny,p5,"ori: %i,%i,%i",dataOrientSensor[0].x,dataOrientSensor[0].y,dataOrientSensor[0].z);
		fontFilePrint(frontbuffer,mask,fontTiny,p6,"dt=%i ms",dt);
		
		// update framebuffer
		framebufferCopySurface(framebuffer,frontbuffer);
		framebufferRedraw(framebuffer);
		surfaceCopyMask(background,frontbuffer,mask);
		surfaceModClear(mask);
		
		
		// suspend; use this function to ensure a constant wake-up frequency
// 		vTaskDelayUntil(&xLastWakeTime,xDelay);
		
		dt = (uint16_t)(epic_rtc_get_milliseconds() - t0);
// 		if (dt < T_TASK_MS) mxc_delay(1000*(T_TASK_MS-dt));
	}
	
	// clean up and exit
	doExit("exiting fontdemo",0);
	return 0;
}
