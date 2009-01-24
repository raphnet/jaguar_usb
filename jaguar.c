/* Jaguar_to_USB: Jaguar controller to USB adapter
 * Copyright (C) 2009 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "jaguar.h"

#define REPORT_SIZE		5



// report matching the most recent bytes from the controller
static unsigned char last_read_controller_bytes[REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_reported_controller_bytes[REPORT_SIZE];

typedef struct {
	char x,y;
	char btnA,btnB,btnC;
	char btnOPT, btnPAUSE;
	char btnsKp[12]; // Given 1-9, *, 0, #
} JagCtlState;

static void readController(JagCtlState *st)
{
	int col;
	unsigned char coldat[4];

	st->x = st->y = 0x80;
	st->btnA = 0;
	st->btnB = 0;
	st->btnC = 0;
	st->btnOPT = 0;
	st->btnPAUSE = 0;
	memset(st->btnsKp, 0, sizeof(st->btnsKp));

	// Columns are invoked by driving the corresponding
	// wire 'low'. PORTC5 for Pin 1, C4 for pin 2...C2 for pin 4
	for (col = 0; col<4; col++)
	{
		unsigned char bit = 0x20 >> col;

		// output, low
		PORTC &= ~bit;
		DDRC |= bit;

		_delay_ms(2);
		coldat[col] = PINC & 0x3;
		coldat[col] |= (PINB & 0x38)>>1;
		if (PINB & 0x01)
			coldat[col] |= 0x20;

		// input, pull-up on
		DDRC &= ~bit;	
		PORTC |= bit;
	}

	st->btnPAUSE = !(coldat[3] & 0x02);

	st->btnOPT = !(coldat[0] & 0x01);
	st->btnC = !(coldat[1] & 0x01);
	st->btnB = !(coldat[2] & 0x01);
	st->btnA = !(coldat[3] & 0x01);

	st->btnsKp[2] = !(coldat[0] & 0x10); // 3
	st->btnsKp[1] = !(coldat[1] & 0x10); // 2
	st->btnsKp[0] = !(coldat[2] & 0x10); // 1
	
	if (!(coldat[3] & 0x10)) // East
		st->x = 0xff;
	
	st->btnsKp[5] = !(coldat[0] & 0x08); // 6
	st->btnsKp[4] = !(coldat[1] & 0x08); // 5
	st->btnsKp[3] = !(coldat[2] & 0x08); // 4

	if (!(coldat[3] & 0x08)) // West
		st->x = 0x00;

	st->btnsKp[8] = !(coldat[0] & 0x04); // 9
	st->btnsKp[7] = !(coldat[1] & 0x04); // 8
	st->btnsKp[6] = !(coldat[2] & 0x04); // 7

	if (!(coldat[3] & 0x04)) // South
		st->y = 0xff;
	
	st->btnsKp[11] = !(coldat[0] & 0x20); // #
	st->btnsKp[10] = !(coldat[1] & 0x20); // 0
	st->btnsKp[9] = !(coldat[2] & 0x20); // *

	if (!(coldat[3] & 0x20)) // North
		st->y = 0x00;
	
}



static void jaguarUpdate(void)
{
	JagCtlState st;
	unsigned char btns1=0, btns2=0, btns3=0, bit;
	int i;

	readController(&st);


	if (st.btnA) btns1 |= 0x01;
	if (st.btnB) btns1 |= 0x02;
	if (st.btnC) btns1 |= 0x04;
	if (st.btnOPT) btns1 |= 0x08;
	if (st.btnPAUSE) btns1 |= 0x10;

	for (i=0,bit=0x20; bit; i++, bit<<=1) 
		if (st.btnsKp[i]) btns1 |= bit;

	for (bit=0x01; bit; i++, bit<<=1) 
		if (st.btnsKp[i]) btns2 |= bit;

	if (st.btnsKp[11])
		btns3 |= 0x01;

	last_read_controller_bytes[0]=st.x;
	last_read_controller_bytes[1]=st.y;
 	last_read_controller_bytes[2]=btns1;
 	last_read_controller_bytes[3]=btns2;
 	last_read_controller_bytes[4]=btns3;
}	

static void jaguarInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();


	/* 
	 * IO  | HD15 pin | Color      | Description
	 *
	 * PC5 |  1       | Brown      | Column
	 * PC4 |  2       | Red        | Column
	 * PC3 |  3       | Orange     | Column
	 * PC2 |  4       | Yellow     | Column
	 *
	 * PC1 |  6       | Blue       | Row
	 * PC0 |  10      | Black      | Row
	 * PB5 |  11      | Grey       | Row
	 * PB4 |  12      | Pale Blue  | Row
	 * PB3 |  13      | Pink       | Row
	 * PB0 |  14      | White      | Row
	 *
	 * VCC |  7       | Green
	 * GND |  9       | Purple
	 */

	DDRC=0x00;
	PORTC=0xff; 
	DDRB=0;
	PORTB=0xff;

	jaguarUpdate();

	SREG = sreg;
}
static char jaguarChanged(void)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }
	
	return memcmp(last_read_controller_bytes, 
					last_reported_controller_bytes, REPORT_SIZE);
}

static void jaguarBuildReport(unsigned char *reportBuffer)
{
	if (reportBuffer != NULL)
	{
		memcpy(reportBuffer, last_read_controller_bytes, REPORT_SIZE);
	}
	memcpy(last_reported_controller_bytes, 
			last_read_controller_bytes, 
			REPORT_SIZE);	
}

#include "report_desc_2axe_17btn.c"

Gamepad jaguarGamepad = {
	report_size: 		REPORT_SIZE,
	reportDescriptorSize:	sizeof(usbHidReportDescriptor_2axe_17btn),
	init: 			jaguarInit,
	update: 		jaguarUpdate,
	changed:		jaguarChanged,
	buildReport:		jaguarBuildReport
};

Gamepad *jaguarGetGamepad(void)
{
	jaguarGamepad.reportDescriptor = (void*)usbHidReportDescriptor_2axe_17btn;

	return &jaguarGamepad;
}

