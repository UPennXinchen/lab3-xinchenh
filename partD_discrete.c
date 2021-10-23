#include <avr/io.h>
#include <avr/interrupt.h>

#include "uart.h"
#define F_CPU 16000000UL
#include <stdlib.h>
#include <stdio.h>
#include <util/delay.h>

#define BAUD_RATE 9600
#define BAUD_PRESCALER (((F_CPU / (BAUD_RATE * 16UL))) - 1)

volatile int time_len=0;
volatile int edge=0;
char String[25];

int pulse_length=19;
volatile int trig_flag=0;//1 when it is high; 0 when it is low; start at low
volatile int flag=1;//1 when looking for a rising edge; 0 when when looking for a falling edge

volatile int edge1=0;//The time that sending a trig pulse
volatile int edge2=0;//The time that receiving the first echo pulse

int interval=0;
int us=0;
int distance=0;

int echo_flag=0;//Indicate whether the receiving is finished

int tone_table[8] = {29,26,23,21,19,17,15,14};
void Timer0_ini()
{
	//Timer0 Prescale: Divide by 256
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);
	
	//set Timer0 to CTC Mode
	TCCR0A |= (1<<WGM01);
	
	//Initialize OCR0A
	OCR0A = 26;			//1046Hz as initial value

	//toggle on compare match
	TCCR0A |= (1<<COM0A0);
	
}

void Timer1_ini()
{
	//Enable Timer/Counter1 input capture interrupt
	TIMSK1 |= (1<<ICIE1);
	
	//Prescale Timer1 by 64
	TCCR1B |=(1<<CS11);
	TCCR1B |=(1<<CS10);
	
	//Looking for a rising edge
	TCCR1B |= (1<<ICES1);
	
	//Write a logic one to ICF1 to clear it
	TIFR1 |= (1<<ICF1);
}

void Timer2_ini()
{
	//Prescale Timer2 by 8
	TCCR2B |=(1<<CS21);

	//Enable Timer2 and set it to be CTC mode with TOP at OCR2A(WGM22 21 20: 0 1 0)
	TCCR2A |=(1<<WGM21);
	
	//Enable Timer/Counter2 compare match A interrupt
	TIMSK1 |= (1<<OCIE2A);
	
	//Toggle PB3 (OC2A) on compare match
	TCCR2A |=(1<<COM2A0);
	TCCR2A &=~(1<<COM2A1);
	
	//Begin interrupt quickly
	OCR2A=20;
	
	//Clear output compare interrupt flag
	TIFR2 |=(1<<OCF2A);
}

void initialize()
{
	cli();					//Disable all global interrupts
	//Port I/O settings
	DDRB |= (1 << DDB3);			//set PB4 as output
	DDRD = (1 << DDD6);				//set PD6 as output
	DDRB &= ~(1 << DDB0);			//set PB5 as input
	PORTD |= (1 << PORTD6);			//Drive PD6 as low
	
	sei();	//Enable global interrupts
}

void calculate_tone()
{
	int a = distance/10;
	if (a==2)
	{
		OCR0A = tone_table[7];
	}
	else if (a==3)
	{
		OCR0A = tone_table[6];
	}
	else if (a==4)
	{
		OCR0A = tone_table[5];
	}
	else if (a==5)
	{
		OCR0A = tone_table[4];
	}
	else if (a==6)
	{
		OCR0A = tone_table[3];
	}
	else if (a==7)
	{
		OCR0A = tone_table[2];
	}
	else if(a==8)
	{
		OCR0A = tone_table[1];
	}
	else
	{
		OCR0A = tone_table[0];
	}
}

ISR(TIMER2_COMPA_vect)
{
	if(echo_flag)//Only transmit a new pulse when we didn't receive echo signal
	{
		if (trig_flag==0)
		{
			OCR2A=pulse_length;
			trig_flag=1;
		}
		else
		{
			trig_flag=0;
			//After sending the trig signal, stop sending trig signals until receiving finished
			echo_flag=0;
			//Change to input pin
			DDRB &=~(1<<DDB3);
		}
	}
}

ISR(TIMER1_CAPT_vect)
{
	if (flag)//If it is a rising edge, record the start time
	{
		edge1=ICR1;
		flag=0;
	}
	else
	{
		edge2=ICR1;
		flag=1;
		echo_flag=1;
		DDRB |=(1<<DDB3);
		
		//Begin to send the next trig pulse quickly
		OCR2A=20;

		interval=edge2-edge1;
		
		us=4*interval;
		distance=us/58;      //Distance is expressed in cm.
		calculate_tone();

		//print the distance
		sprintf(String, "Distance: %d cm\n",distance);
		UART_putstring(String);
	}
	TCCR1B^=(1<<ICES1);
}

int main(void)
{
	UART_init(BAUD_PRESCALER);
	Timer0_ini();
	Timer1_ini();
	Timer2_ini();
	initialize();
	while (1);
	
}
