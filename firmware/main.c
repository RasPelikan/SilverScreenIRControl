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
static int get_potentiometer_position();
static void timer1_init(void);
static void go_asleep();
static void wait_n_seconds(float seconds, void (*cmd)());

static void (*command)();                             // command executed after n seconds
static int number_of_interrupts;                      // number of interrupts necessary to wait n seconds

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
 * "wait_n_seconds" timer
 */
ISR(TIMER0_COMPA_vect) {

	// decrease counter
	if (--number_of_interrupts == 0) {

		// disable timer
		TIMSK &= ~(_BV(TOIE0));

		void (*cmd)() = command;
		command = NULL;

		// run command if time elapsed
		(*cmd)();

	}

}

/*
 * Needed for ADC noise reduction mode
 */
EMPTY_INTERRUPT(ADC_vect);

/*
 * Needed for "wake up on pin-change"
 */
ISR(INT0_vect) {

	cli();

	GIMSK &= ~_BV(INT0);

	sei();

}

/*
 * Main routine, called on booting
 */
int main() {

	/*
	 * initialize activity indicator and up/down-pins
	 */
	DDRB |= _BV(PB0) | _BV(PB1) | _BV(PB4);
	PORTB = 0x00;                                     // every pin is low

	/*
	 * initialize ADC for potentiometer
	 */
	ADMUX |= _BV(MUX0) | _BV(MUX1);                   // measure on PB3 (= ADC3)
	ADMUX &= ~(_BV(REFS0) | _BV(REFS1) | _BV(REFS2)); // AREF = AVcc
	ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS2);    // set prescaler to 128 = 8MHz / 128 = 64kHz
	ADCSRA = _BV(ADEN) | _BV(ADIE);                   // enable ADC and activate interrupt

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

		/*
		 * IR-command received?
		 */
		if (irmp_get_data(&irmp_data)) {

			/*
			 * switch on potentiometer-line (Q1)
			 * and sign activity
			 */
			PORTB |= _BV(PB0);
			_delay_ms(10);

			/*
			 * switch activity light on and off
			 */
			int poti = get_potentiometer_position();

			/*
			 * programming mode
			 */
			if (poti < 200) {

				if (poti < 10) {

					PORTB |= _BV(PB1);
					_delay_ms(200);
					PORTB &= ~(_BV(PB1));

				} else {

					PORTB |= _BV(PB4);
					_delay_ms(200);
					PORTB &= ~(_BV(PB4));

				}

			}
			/*
			 * control mode
			 */
			else {


			}

			/*
			 * switch off potentiometer-line (Q1)
			 * and sign activity
			 */
			PORTB &= ~(_BV(PB0));

		}
		/*
		 * if no IR-command received than go asleep after 1 second
		 */
		else if (command == NULL) {

			wait_n_seconds(5, go_asleep);

		}

	}

}

static void go_asleep() {

	/*
	 * initialize "wake up on pin-change" on B2
	 */
	GIMSK |= _BV(INT0);
	MCUCR &= ~(_BV(ISC01) | _BV(ISC00));              // INT0 on low level

	sei();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);              // power down
	sleep_mode();                                     // enter sleep mode

}

static void wait_n_seconds(float seconds, void (*cmd)()) {

	// store information for timer-interrupt
	command = cmd;
	number_of_interrupts = seconds * 31;

	// enable timer
	OCR0A = 252; // 31 times of compare matches at 252 (at prescaler 1024) is 1 second
	TCNT0 = 0;  // start at counter 0
	TCCR0A |= _BV(WGM01); // compare-match
	TCCR0B |= _BV(CS00) | _BV(CS02); // prescaler 1024
	TIMSK |= _BV(OCIE0A);

}

static void store_command(bool down, IRMP_DATA *comand) {



}

int get_potentiometer_position() {

	// start single conversion
	set_sleep_mode(SLEEP_MODE_ADC);     // noise reduction
	sleep_enable();						// enable sleep
	sleep_cpu();						// enter sleep mode
	sleep_disable();					// first thing to do is disable sleep

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
