#include <avr/io.h>
#include <avr/interrupt.h>



void Initialize()
{
	//disable global interrupts
	cli();
	
	//GPIO pins setup
	DDRD |= (1<<DDD6);
	PORTD &=~(1<<PORTD6);
	
	//prescale the timer0 to 1/256 
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);

	//Set the normal mode
	TCCR0A &= ~(1<<WGM00);    
	TCCR0A &= ~(1<<WGM01);    
	TCCR0A &= ~(1<<WGM02);    
	OCR0A=70;            //counter clears timer to 0 when timer reaches 70
	
	TIMSK0|=(1<<OCIE0A);   //Set the interrupt request

	sei();                 //Enable interrupt
}

ISR(TIMER0_COMPA_vect)
{
	PORTD ^= (1<<(PORTD6));
}


int main(void)
{
	Initialize();
	
	while (1);
}

