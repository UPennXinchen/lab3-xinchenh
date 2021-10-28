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
volatile int pulse_length=19;
volatile int trig_flag=0;
volatile int echo_flag=0;
volatile int flag=1;//when looking for a rising edge it is 1;
volatile int edge1=0;//start time
volatile int edge2=0;//end time

int interval=0;
int us=0;
int distance=0;
char String[25];
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

void ADC_ini()
{
	//clear power reduction for ADC
	PRR &= (1<<PRADC);
	
	//Vref = ADC
	ADMUX |= (1<<REFS0);
	ADMUX &= ~(1<<REFS1);
	//Set the ADC clock: divided by 128
	ADCSRA |= (1<<ADPS0);
	ADCSRA |= (1<<ADPS1);
	ADCSRA |= (1<<ADPS2);
	//select channel 0
	ADMUX &= ~(1<<MUX0);
	ADMUX &= ~(1<<MUX1);
	ADMUX &= ~(1<<MUX2);
	ADMUX &= ~(1<<MUX3);
	//Set to free running
	ADCSRA |= (1<<ADATE);		//auto triggering of adc
	ADCSRB &= ~(1<<ADTS0);		//free running mode ADT[2:0] = 000
	ADCSRB &= ~(1<<ADTS1);
	ADCSRB &= ~(1<<ADTS2);
	//disable digital input buffer
	DIDR0 |= (1<<ADC0D);
	//Enable ADC
	ADCSRA |= (1<<ADEN);
	//Enable ADC interrupt
	ADCSRA |= (1<<ADIE);
	//start conversation
	ADCSRA |= (1<<ADSC);
}


void calculate_tone()
{
	if (mode_flag == 1)
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
	else
	{
		OCR0A = distance*0.213+8.663;
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
		// 		sprintf(String, "Distance: %d cm\n",distance);
		// 		UART_putstring(String);
	}
	TCCR1B^=(1<<ICES1);
}

ISR(ADC_vect)
{
	if (ADC>=0 && ADC<100)
	{
		duty_cycle = 5;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=100 && ADC<200)
	{
		duty_cycle = 10;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=200 && ADC<300)
	{
		duty_cycle = 15;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=300 && ADC<400)
	{
		duty_cycle = 20;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=400 && ADC<500)
	{
		duty_cycle = 25;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=500 && ADC<600)
	{
		duty_cycle = 30;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=600 && ADC<700)
	{
		duty_cycle = 35;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=700 && ADC<800)
	{
		duty_cycle = 40;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else if (ADC>=800 && ADC<900)
	{
		duty_cycle = 45;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	else
	{
		duty_cycle = 50;
		OCR0B = ceil(OCR0A * duty_cycle/100);
	}
	//Print ADC Values
	sprintf(String,"ADC = %d\n",ADC);
	UART_putstring(String);
}

void initialize()
{
	cli();					
	//Port I/O settings
	DDRB |= (1 << DDB3);			//set PB3 as output
	DDRD = (1 << DDD6);			//set PD6 as output
	DDRB &= ~(1 << DDB0);			//set PB0 as input
	DDRD |= (1 << DDD5);			//set PD5 as output
	PORTD |= (1 << PORTD6);			//Drive PD6 as high
	
	sei();	
}


int main(void)
{
	UART_init(BAUD_PRESCALER);
	Timer0_ini();
	Timer1_ini();
	Timer2_ini();
	ADC_ini();
	initialize();
	while (1);
	
}

