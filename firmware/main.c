#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#include "irmp.h"

/*
 * forward declarations
 */
int get_potentiometer_position();
static void timer1_init(void);

/*
 * Timer1 interrupt for processing IR-input
 */
#ifdef TIM1_COMPA_vect                                // ATtiny84
#define COMPA_VECT  TIM1_COMPA_vect
#else
#define COMPA_VECT  TIMER1_COMPA_vect                 // ATmega
#endif
ISR(COMPA_VECT) {

	(void) irmp_ISR();                                // call IRMP ISR

}

/*
 * Main routine, called on booting
 */
int main() {

	/*
	 * initialize activity indicator
	 */
	DDRB |= _BV(PB0);                                 // LED pin to output
	PORTB &= ~(_BV(PB0));                             // switch LED off (active low)

	/*
	 * initialize ADC for potentiometer
	 */
	ADMUX &= ~(_BV(REFS0) | _BV(REFS1) | _BV(REFS2)); // AREF = AVcc
	ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS2);    // set prescaler to 128 = 8MHz / 128 = 64kHz
	ADCSRA = _BV(ADEN);                               // enable ADC

	/*
	 * initialize IRMP for IR-decoding
	 */
	IRMP_DATA irmp_data;
	irmp_init();
	timer1_init();

	/*
	 * enable interrupts
	 */
	sei();

	while (1) {

		if (irmp_get_data(&irmp_data)) {

			/*
			 * switch activity light on and off
			 */
			int poti = get_potentiometer_position();
			if (poti > 950) {

				PORTB |= _BV(PB0);
				_delay_ms(100);
				PORTB &= ~(_BV(PB0));
				_delay_ms(100);

			}

		}

	}

}

int get_potentiometer_position() {

	// measure on PB2
	ADMUX |= _BV(MUX0);

	// start single convertion
	// write ’1′ to ADSC
	ADCSRA |= _BV(ADSC);

	// wait for conversion to complete
	// ADSC becomes ’0′ again
	// till then, run loop continuously
	while (ADCSRA & _BV(ADSC));

	// read low and high byte as result
	return ADCL | (ADCH << 8);

}

/*
 * @see
 */
static void timer1_init(void) {
#if defined (__AVR_ATtiny45__) || defined (__AVR_ATtiny85__)                // ATtiny45 / ATtiny85:

#if F_CPU >= 16000000L
	OCR1C = (F_CPU / F_INTERRUPTS / 8) - 1; // compare value: 1/15000 of CPU frequency, presc = 8
	TCCR1 = (1 << CTC1) | (1 << CS12);// switch CTC Mode on, set prescaler to 8
#else
	OCR1C = (F_CPU / F_INTERRUPTS / 4) - 1; // compare value: 1/15000 of CPU frequency, presc = 4
	TCCR1 = (1 << CTC1) | (1 << CS11) | (1 << CS10);// switch CTC Mode on, set prescaler to 4
#endif

#else                                                                       // ATmegaXX:
	OCR1A = (F_CPU / F_INTERRUPTS) - 1; // compare value: 1/15000 of CPU frequency
	TCCR1B = (1 << WGM12) | (1 << CS10); // switch CTC Mode on, set prescaler to 1
#endif

#ifdef TIMSK1
	TIMSK1 = 1 << OCIE1A;                  // OCIE1A: Interrupt by timer compare
#else
	TIMSK = 1 << OCIE1A;                   // OCIE1A: Interrupt by timer compare
#endif
}
