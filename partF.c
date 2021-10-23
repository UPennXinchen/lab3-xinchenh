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
volatile int trig_flag=0;		//1 when it is high; 0 when it is low; start at low
volatile int flag=1;			//1 when looking for a rising edge; 0 when when looking for a falling edge
int echo_flag=0;				//Finished Receive or not?
int mode_flag=1;				//1 for continuous; 0 for discrete
volatile int edge1=0;			//The time that sending a trig pulse
volatile int edge2=0;			//The time that receiving the first echo pulse
int duty_cycle = 0;				//set duty cycle for ADC
int high_or_low = 0;

int interval=0;
int us=0;						//number of microseconds spent
int distance=0;					//calculation for distance



//int tone_table[8] = {29,26,23,21,19,17,15,14};
int tone_table[8] = {59,53,47,43,39,35,31,29};

void Timer0_ini()
{
	DDRD |= (1 << DDD5);
	
	//Timer0 Prescale: Divide by 256
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);
	
	//set Timer0 to CTC Mode
	//TCCR0A |= (1<<WGM01);
	TCCR0A |= (1<<WGM00);
	TCCR0A |= (1<<WGM01);
	TCCR0B |= (1<<WGM02);
	
	//Initialize OCR0A
	OCR0A = 26*2;			//1046Hz as initial value
	//OCR0B = ceil(OCR0A * 0.4);
	//toggle on compare match
	//TCCR0A |= (1<<COM0A0);
	TCCR0A |= (1<<COM0B1);
	
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



void initialize()
{
	cli();					//Disable all global interrupts
	//Port I/O settings
	DDRB |= (1 << DDB3);			//set PB4 as output (ultrasound)
	DDRB &= ~(1 << DDB0);			//set PB0 as input	(ultrasound)

	DDRD &= ~(1<<DDD3);				//set PD3 as input (button)
	DDRD |= (1 << DDD5);			//set PD5 as output (BUZZER)
	DDRB |= (1 << DDB5);			//set PB5 as output (indicator)
	
	PORTD |= (1 << PORTD6);			//Drive PD6 as low
	
	sei();	//Enable global interrupts
}

void tone_and_volumn(
)
{
	if (mode_flag == 1)
	{
		int a = distance/10;

		for(int i=2;i<=9;i++)
		{
			if (a==i)
			{
				OCR0A = tone_table[9-i];
			}
		}	
	}
	else
	{
		OCR0A = distance*0.213+8.663;
	}
	//calculate the volumn
/*	OCR0B = ceil(OCR0A * duty_cycle/100);*/
	
}

//ISR(TIMER0_COMPA_vect)
// {
// 	if (high_or_low)
// 	{
// 		tone_and_volumn();
// 		high_or_low = 0;
// 	}
// 	else
// 	{
// 		OCR0A = ceil(OCR0A * duty_cycle/100);
// 		high_or_low = 1;
// 	}
// }

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
	else if(flag==0)
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
		//calculate the tone and volumn at the same loop
		tone_and_volumn();
		//OCR0B = ceil(OCR0A * duty_cycle/100);
		
		sprintf(String, "Distance: %d cm\n",distance);
		UART_putstring(String);
	}
	TCCR1B^=(1<<ICES1);
}

ISR(ADC_vect)
{
	for(int i = 0; i <= 9; i++)
	{
		if (ADC>=i*100 && ADC<(i+1)*100))
		{
			duty_cycle = 5*(i+1);
		}
	}
	
	OCR0B = ceil(OCR0A * duty_cycle/100);
	//_delay_us(5);
	//Print ADC Values
// 	sprintf(String,"ADC = %d\n",ADC);
// 	UART_putstring(String);
}

int main(void)
{
	UART_init(BAUD_PRESCALER);
	Timer0_ini();
	Timer1_ini();
	Timer2_ini();
	ADC_ini();
	initialize();
	while (1)
	{
		if(PIND & (1<<PIND3))		//PIND3 is high
		{
			mode_flag = 1;
			PORTB &= ~(1<<PORTB5);
		}
		else
		{
			mode_flag = 0;
			PORTB |= (1<<PORTB5);
			_delay_ms(5000);
		}
		
	}
	
}
