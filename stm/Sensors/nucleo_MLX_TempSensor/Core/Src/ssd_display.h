/*
 * ssd_display.h
 *
 *  Created on: Jan 9, 2026
 *      Author: angelahuynh
 */

#ifndef INC_SSD_DISPLAY_H_
#define INC_SSD_DISPLAY_H_
#include "stm32g4xx_hal.h"

void OLED_UI_Init(void);

void OLED_UI_ShowTemps(int temp_c_x100, int temp_f_x100);


#endif /* INC_SSD_DISPLAY_H_ */
