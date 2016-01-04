#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#include "irmp.h"

/*
 * globals
 */
IRMP_DATA EEMEM up_button_signature;                  // EEPROM memory address for signature of the up-button
IRMP_DATA EEMEM down_button_signature;                // EEPROM memory address for signature of the down-button
static IRMP_DATA up_button;
static IRMP_DATA down_button;
static void (*command)();                             // command executed after n seconds
static int number_of_interrupts;                      // number of interrupts necessary to wait n seconds

/*
 * bring MCU into hibernate
 */
static void go_asleep() {

	PORTB &= ~(_BV(PB0));                             // turn off activity indicator

	/*
	 * initialize "wake up on pin-change" on B2
	 */
	GIMSK |= _BV(INT0);                               // pin INT0
	MCUCR &= ~(_BV(ISC01) | _BV(ISC00));              // INT0 on low level
	sei();                                            // enable interrupt since this method might
	                                                  // be called within an interrupt and if so the
	                                                  // INT0-interrupt won't fire

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);              // power down - mode (<1mA current)
	sleep_mode();                                     // enter sleep mode

}

/*
 * resets time interval if given command is the command currently processed
 */
static void reset_n_seconds(float seconds, void (*cmd)()) {

	if (cmd == command) {

		number_of_interrupts = seconds * 31;

	}

}

/*
 * based on F_CPU = 8MHz and a prescaler of 1024 the timer0 has to count up to 252
 * for 31 times since one second is elapsed
 */
static void wait_n_seconds(float seconds, void (*cmd)()) {

	// store information for timer-interrupt
	command = cmd;
	number_of_interrupts = seconds * 31;

	// enable timer
	OCR0A = 252;                                      // 31 times of compare matches at 252 is 1 second
	TCNT0 = 0;                                        // start at counter 0
	TCCR0A |= _BV(WGM01);                             // compare-match mode
	TCCR0B |= _BV(CS00) | _BV(CS02);                  // prescaler 1024
	TIMSK |= _BV(OCIE0A);                             // use OCR0A for a compare-match

}

/*
 * read the current potentiometer position by reading ADC
 */
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
 * "wait_n_seconds" timer-interrupt
 */
ISR(TIMER0_COMPA_vect) {

	// decrease counter and check wether time-period has elapsed
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
 * empty ADC interrupt - needed for ADC noise reduction mode
 */
EMPTY_INTERRUPT(ADC_vect);

/*
 * INT0-interrupt - needed for "wake up on pin-change"
 */
ISR(INT0_vect) {

	/*
	 * disable all interrupts: INT0 fires repeatedly as long as INT0 is low
	 */
	cli();

	GIMSK &= ~_BV(INT0);  // disable INT0-interrupt

	/*
	 * re-enable all interrupts
	 */
	sei();

}

/*
 * Timer1 interrupt for processing IR-input
 *
 * @see https://www.mikrocontroller.net/articles/IRMP
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
 * @see https://www.mikrocontroller.net/articles/IRMP
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

/*
 * initialize activity indicator (PB0) and up(PB1)/down(PB4)-pins
 */
static void initialize_io() {

	DDRB |= _BV(PB0) | _BV(PB1) | _BV(PB4);           // configure PB0, PB1 and PB4 as out
	PORTB = 0x00;                                     // every pin is low

}

/*
 * initialize ADC for reading potentiometer-position
 */
static void initialize_adc() {

	ADMUX |= _BV(MUX0) | _BV(MUX1);                   // measure on PB3 (= ADC3)
	ADMUX &= ~(_BV(REFS0) | _BV(REFS1) | _BV(REFS2)); // AREF = AVcc
	ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS2);    // set prescaler to 128 = 8MHz / 128 = 64kHz
	ADCSRA = _BV(ADEN) | _BV(ADIE);                   // enable ADC and activate interrupt

}

/*
 * Loads the signature of the up- and down-button out of EEPROM
 */
static void read_button_signatures_from_eeprom() {

	// read up-button signature
	eeprom_read_block(&up_button, &up_button_signature, sizeof(IRMP_DATA));
	// read down-button signature
	eeprom_read_block(&down_button, &down_button_signature, sizeof(IRMP_DATA));

}

/*
 * initialize IRMP for IR-decoding
 */
static void initialize_irmp() {

	irmp_init();
	timer1_init();

}

/*
 * the pressed button should be stored
 */
static void store_command_to_eeprom(bool down, IRMP_DATA *irmp_data) {

	// down-button
	if (down) {

		down_button = *irmp_data;                     // update memory for check next button pressed
		// update eeprom if brown out occurs
		eeprom_write_block(&down_button, &down_button_signature, sizeof(IRMP_DATA));

	}
	// up-button
	else {

		up_button = *irmp_data;                       // update memory for check next button pressed
		// update eeprom if brown out occurs
		eeprom_write_block(&up_button, &up_button_signature, sizeof(IRMP_DATA));

	}

}

/*
 * compare given button-signature to the up-button signature stored in EEPROM
 */
static bool is_up_button_pressed(IRMP_DATA *irmp_data) {

	if (irmp_data->protocol != up_button.protocol) {
		return false;
	}
	if (irmp_data->address != up_button.address) {
		return false;
	}
	if (irmp_data->command != up_button.command) {
		return false;
	}

	return true;

}

/*
 * compare given button-signature to the down-button signature stored in EEPROM
 */
static bool is_down_button_pressed(IRMP_DATA *irmp_data) {

	if (irmp_data->protocol != down_button.protocol) {
		return false;
	}
	if (irmp_data->address != down_button.address) {
		return false;
	}
	if (irmp_data->command != down_button.command) {
		return false;
	}

	return true;

}

static void disable_up_and_down() {

	PORTB &= ~(_BV(PB1));                             // disable up-line
	PORTB &= ~(_BV(PB4));                             // disable down-line

}

/*
 * user pressed a button of the remote control
 */
static void process_irmp(IRMP_DATA *irmp_data) {

	int poti = get_potentiometer_position();          // read potentiometer position

	/*
	 * programming mode
	 */
	if (poti < 200) {

		PORTB |= _BV(PB0);                           // turn on activity indicator

		bool down = poti < 10;                       // minimum position means "program down"
		store_command_to_eeprom(down, irmp_data);    // store to eeprom

	}
	/*
	 * control mode
	 */
	else if (is_up_button_pressed(irmp_data)) {

		PORTB |= _BV(PB0);                           // turn on activity indicator
		PORTB |= _BV(PB1);                           // enable up-line

		wait_n_seconds(10, disable_up_and_down);

	}
	else if (is_down_button_pressed(irmp_data)) {

		PORTB |= _BV(PB0);                           // turn on activity indicator
		PORTB |= _BV(PB4);                           // enable down-line

		float seconds = 10.0 / 1024 * poti;
		wait_n_seconds(seconds, disable_up_and_down);

	}
	else {

		// ignore unknown buttons

	}

}

/*
 * main routine, called after booting
 */
int main() {

	/*
	 * initialize device
	 */
	initialize_io();
	initialize_adc();
	initialize_irmp();
	read_button_signatures_from_eeprom();
	sei();                                            // enable interrupts

	/*
	 * main loop
	 */
	IRMP_DATA irmp_data;
	while (1) {

		/*
		 * IR-command received?
		 */
		if (irmp_get_data(&irmp_data)) {

			process_irmp(&irmp_data);

			// reset "go asleep" timer
			reset_n_seconds(2, go_asleep);

		}
		/*
		 * if no IR-command received than go asleep after 2 second
		 */
		else if (command == NULL) {

			wait_n_seconds(2, go_asleep);

		}

	}

}

