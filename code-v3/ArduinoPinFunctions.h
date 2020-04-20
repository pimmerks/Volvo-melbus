#if !defined(ARDUINO_PIN_FUNC)
#define ARDUINO_PIN_FUNC 1

#include <Arduino.h>

// Interrupts
#define DISABLE_TIMER0_INTERRUPT   (TIMSK0 &= ~_BV(TOIE0)) // Disables timer0 interrupt.
#define DISABLE_CLOCK_INTERRUPT    (EIMSK &= ~(1 << INT_NUM)) // Disable interrupt on INT_NUM quicker than: detachInterrupt(INT_NUM);
#define ENABLE_CLOCK_INTERRUPT     (EIMSK |= (1 << INT_NUM)) // Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
#define CLEAR_CLOCK_INTERRUPT_FLAG (EIFR |= 1 << INT_NUM)


// Data
#define SET_DATAPIN_AS_OUTPUT (DDRD |= (1 << MELBUS_DATA)) // Convert datapin to output = pinMode(MELBUS_DATA, OUTPUT);
#define SET_DATAPIN_AS_INPUT  (DDRD &= ~(1 << MELBUS_DATA)) // Convert datapin to input = pinMode(MELBUS_DATA, INPUT_PULLUP);
#define READ_DATAPIN          (PIND & (1 << MELBUS_DATA)) // Read status of Datapin
#define SET_DATAPIN_HIGH      (PORTD |= (1 << MELBUS_DATA))
#define SET_DATAPIN_LOW       (PORTD &= ~(1 << MELBUS_DATA))

// Clock
#define SET_CLOCKPIN_AS_OUTPUT (DDRD |= (1 << MELBUS_CLOCK)) // Convert datapin to output = pinMode(MELBUS_DATA, OUTPUT);
#define SET_CLOCKPIN_AS_INPUT  (DDRD &= ~(1 << MELBUS_CLOCK)) // Convert datapin to input = pinMode(MELBUS_DATA, INPUT_PULLUP);
#define READ_CLOCKPIN         (PIND & (1 << MELBUS_CLOCK))
#define SET_CLOCKPIN_HIGH     (PORTD |= (1 << MELBUS_CLOCK))
#define SET_CLOCKPIN_LOW      (PORTD &= ~(1 << MELBUS_CLOCK))

// BUSY
#define SET_BUSYPIN_AS_OUTPUT (DDRD |= (1 << MELBUS_BUSY)) // Convert datapin to output = pinMode(MELBUS_BUSY, OUTPUT);
#define SET_BUSYPIN_AS_INPUT  (DDRD &= ~(1 << MELBUS_BUSY)) // Convert datapin to input = pinMode(MELBUS_BUSY, INPUT_PULLUP);
#define READ_BUSYPIN          (PIND & (1 << MELBUS_BUSY)) // Read status of busy pin (active low)
#define SET_BUSYPIN_HIGH      (PORTD |= (1 << MELBUS_BUSY))
#define SET_BUSYPIN_LOW       (PORTD &= ~(1 << MELBUS_BUSY))

#endif