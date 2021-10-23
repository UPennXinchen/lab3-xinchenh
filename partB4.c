#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL



void Initialize()
{
	cli();
	
	//GPIO pins setup
	DDRD |= (1<<DDD6);
	PORTD |= (1 << PORTD6);		
	
	//prescale the timer0 to 1/256
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS02);

	//Set the Phase Correct PWM mode(TOP = OCRA)
	TCCR0B |= (1<<WGM02);
	TCCR0A &= ~(1<<WGM01);
	TCCR0A |= (1<<WGM00);
	
	//OCR0A sets freq to be440Hz
	OCR0A=71;          
	
	// toggle OC0A on compare match
	TCCR0A |= (1<<COM0A0);
	
	sei();

}

int main(void)
{
	Initialize();
	while (1);
}

