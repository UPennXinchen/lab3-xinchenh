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
volatile int flag=1;//1 when looking for a rising edge; 0 when looking for a falling edge

volatile int edge1=0;//The time that sending a trig pulse
volatile int edge2=0;//The time that receiving the first echo pulse

int interval=0;
int us=0;
int distance=0;

int echo_flag=0;//Indicate whether the receiving is finished

void initialize(){
	cli();					//Disable all global interrupts

	DDRB |= (1 << DDB3);			//set PB3 as output connects to trigger	
	DDRB &= ~(1 << DDB0);			//set PB0 as input
	
	DDRD = (1 << DDD6);			    //set PD6 as output	
	
	//timer0 prescale by 256
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);
	//set Timer0 to CTC
	TCCR0B |= (1<<WGM02);

	//toggle OC0A compare match
	TCCR0A |= (1<<COM0A0);
	//Initialize OCR0A
	OCR0A = 29 ;		//440Hz
	

	//Prescale Timer2 by 8(CS22 21 20: 0 1 0)
	//frequency=16M/8=2MHz
	//period=1/2MHz=0.5us
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

	//Enable Timer/Counter1 input capture interrupt
	TIMSK1 |= (1<<ICIE1);


	//Prescale Timer1 by 64
	//frequency=16M/64=62.5k
	//period=1/62.5k=4us
	TCCR1B |=(1<<CS11);
	TCCR1B |=(1<<CS10);
	
	//Looking for a rising edge
	TCCR1B |= (1<<ICES1);
	
	//Write a logic one to ICF1 to clear it
	TIFR1 |= (1<<ICF1);
	
	sei();	//Enable global interrupts
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
		distance=us/58;//Distance is expressed in cm.
		sprintf(String, "Distance: %d cm\n",distance);
		UART_putstring(String);
	}
	TCCR1B^=(1<<ICES1);
}



int main(void)
{
	UART_init(BAUD_PRESCALER);
	initialize();
	while (1)
	{
	}
	
}
