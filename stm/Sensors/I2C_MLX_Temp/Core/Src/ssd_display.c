/*
 * ssd_display.c
 *
 *  Created on: Jan 9, 2026
 *      Author: angelahuynh
 */


#include "ssd_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include your SSD1306 library headers here
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#define OLED_WIDTH 128

static uint8_t center_x(const char *text, const SSD1306_Font_t *font)
{
    return (OLED_WIDTH - (strlen(text) * font->width)) / 2;
}


void OLED_UI_Init(void)
{
	ssd1306_Init();
	  ssd1306_Fill(Black);
	  ssd1306_SetCursor(1, 1);
	  ssd1306_WriteString("starting...", Font_7x10, White);
	  ssd1306_UpdateScreen();
}

void OLED_UI_ShowTemps(int temp_c_x100, int temp_f_x100)
{
	char title[] = "Temperature";
	    char line1[24];
	    char line2[24];

	    snprintf(line1, sizeof(line1),
	             "C: %d.%02d",
	             temp_c_x100 / 100,
	             abs(temp_c_x100 % 100));

	    snprintf(line2, sizeof(line2),
	             "F: %d.%02d",
	             temp_f_x100 / 100,
	             abs(temp_f_x100 % 100));

	    ssd1306_Fill(Black);

	    ssd1306_SetCursor(center_x(title, &Font_7x10), 0);
	    ssd1306_WriteString(title, Font_7x10, White);

	    ssd1306_SetCursor(center_x(line1, &Font_11x18), 18);
	    ssd1306_WriteString(line1, Font_11x18, White);

	    ssd1306_SetCursor(center_x(line2, &Font_11x18), 40);
	    ssd1306_WriteString(line2, Font_11x18, White);

	    ssd1306_UpdateScreen();
}
