/* SPI Slave Receiver (Mega128) & Master Sender (ESP Cam)
 * Author: mikan
 *
 * TB6612FNG Motor Driver wiring (Port A):
 *   PA0 = AIN1  (Motor A direction)
 *   PA1 = AIN2  (Motor A direction)
 *   PA2 = BIN1  (Motor B direction)
 *   PA3 = BIN2  (Motor B direction)
 *   PA4 = STBY  (Standby: HIGH = active, LOW = standby/off)
 *
 * PWM Speed (Port B, Timer1):
 *   PB5 = OC1A -> PWMA (Motor A speed)
 *   PB6 = OC1B -> PWMB (Motor B speed)
 *
 * TB6612FNG truth table (per channel):
 *   IN1=H, IN2=L, PWM -> Forward  (CW)
 *   IN1=L, IN2=H, PWM -> Backward (CCW)
 *   IN1=L, IN2=L       -> Coast   (stop)
 *   IN1=H, IN2=H       -> Brake   (short brake)
 */

#define F_CPU 16000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*--- TB6612FNG Pin Definitions (Port A) ---*/
#define AIN1  PA0
#define AIN2  PA1
#define BIN1  PA2
#define BIN2  PA3
#define STBY  PA4

/*--- PWM Speed Pins (Port B, Timer1) ---*/
#define PWMA  PB5  // OC1A
#define PWMB  PB6  // OC1B

/*--- Speed lookup: level 0-10 -> OCR value (0-255) ---*/
static const uint8_t speed_table[11] = {
  // 0    1    2    3    4    5    6    7    8    9   10(M)
     0,  25,  51,  76, 102, 127, 153, 178, 204, 229, 255
};

volatile char received_cmd = 0;
volatile uint8_t new_cmd_flag = 0;

static uint8_t current_speed = 0; // Current PWM duty (0-255)

// Initialize SPI in Slave Mode
void SPI_SlaveInit(void) {
  /* Set data direction for SPI pins on Port B:
   * PB0 (SS)   : Input
   * PB1 (SCK)  : Input
   * PB2 (MOSI) : Input Using when Mega128-V2 is just a receiver (slave)
   * PB3 (MISO) : Output
   */
  DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
  DDRB |= (1 << PB3);

  /* Enable SPI + SPI Interrupt, Slave Mode (MSTR=0), Mode 0 */
  SPCR = (1 << SPE) | (1 << SPIE);
}

// Initialize Timer1 for Fast PWM 8-bit on OC1A (PB5) and OC1B (PB6)
// Prescaler = 64 -> 16MHz / 64 / 256 = ~976 Hz PWM frequency
void PWM_Init(void) {
  DDRB |= (1 << PWMA) | (1 << PWMB); // Set PWM pins as output

  // Fast PWM 8-bit (WGM = 0101), Non-inverting on OC1A & OC1B
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
  TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // Prescaler = 64

  OCR1A = 0; // Motor A speed = 0
  OCR1B = 0; // Motor B speed = 0
}

// Initialize TB6612FNG direction + standby pins (Port A)
void Motor_Init(void) {
  DDRA |= (1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2) | (1 << STBY);
  PORTA &= ~((1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2)); // All LOW = coast
  PORTA &= ~(1 << STBY); // STBY LOW = motors disabled at startup
}

// Set both motors PWM to current_speed
void Motor_SetSpeed(void) {
  OCR1A = current_speed;
  OCR1B = current_speed;
}

// Stop both motors (coast)
void Motor_Stop(void) {
  PORTA &= ~((1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2));
  OCR1A = 0;
  OCR1B = 0;
}

// Forward: both motors CW
void Motor_Forward(void) {
  PORTA |=  (1 << AIN1) | (1 << BIN1);
  PORTA &= ~((1 << AIN2) | (1 << BIN2));
  Motor_SetSpeed();
}

// Backward: both motors CCW
void Motor_Backward(void) {
  PORTA &= ~((1 << AIN1) | (1 << BIN1));
  PORTA |=  (1 << AIN2) | (1 << BIN2);
  Motor_SetSpeed();
}

// Turn left: Motor A backward, Motor B forward (pivot)
void Motor_TurnLeft(void) {
  PORTA &= ~(1 << AIN1);
  PORTA |=  (1 << AIN2);  // Motor A: CCW
  PORTA |=  (1 << BIN1);
  PORTA &= ~(1 << BIN2);  // Motor B: CW
  _delay_ms(500);
  Motor_SetSpeed();
}

// Turn right: Motor A forward, Motor B backward (pivot)
void Motor_TurnRight(void) {
  PORTA |=  (1 << AIN1);
  PORTA &= ~(1 << AIN2);  // Motor A: CW
  PORTA &= ~(1 << BIN1);
  PORTA |=  (1 << BIN2);  // Motor B: CCW
  _delay_ms(500);
  Motor_SetSpeed();
}

// Interrupt Service Routine: Runs automatically when 1 byte is received from
// ESP32-CAM
ISR(SPI_STC_vect) {
  received_cmd = SPDR; // Read command char from SPI Data Register
                       // ('W','A','S','D','L','R','0'-'9','M','C','P','X')
  new_cmd_flag = 1;    // Set flag to notify main loop
}

int main(void) {
  // Initialize PE2 and PE3 as Outputs for LEDs
  DDRE |= (1 << PE2) | (1 << PE3);

  // Ensure both LEDs are OFF at startup (LOW level)
  PORTE &= ~((1 << PE2) | (1 << PE3));

  // Initialize peripherals
  // Initialize SPI
  SPI_SlaveInit();
  PWM_Init();
  Motor_Init();

  // Enable Global Interrupts
  sei();

  while (1) {
    // Check if ESP-CAM has sent a new command
    if (new_cmd_flag) {
      new_cmd_flag = 0; // Clear flag to prepare for the next command

      // Alternating LED sequence
      PORTE |= (1 << PE2);
      PORTE &= ~(1 << PE3);
      _delay_ms(100);

      PORTE &= ~(1 << PE2);
      PORTE |= (1 << PE3);
      _delay_ms(100);

      PORTE &= ~((1 << PE2) | (1 << PE3));

      char cmd = received_cmd;

      // Handle speed commands: '0'-'9' and 'M' (grouped)
      if (cmd >= '0' && cmd <= '9') {
        current_speed = speed_table[cmd - '0'];
        Motor_SetSpeed(); // Apply new speed immediately
      }
      else if (cmd == 'M') {
        current_speed = speed_table[10]; // Max speed = 255
        Motor_SetSpeed();
      }
      else {
        // Handle other commands
        switch (cmd) {
        // Connect / Pause
        case 'C': // Connect: enable TB6612FNG by pulling STBY HIGH
          PORTA |= (1 << STBY);
          break;

        case 'P': // Pause: disable TB6612FNG by pulling STBY LOW
          PORTA &= ~(1 << STBY);
          Motor_Stop();
          break;

        // Car movement
        case 'W': // Forward
          Motor_Forward();
          break;

        case 'A': // Turn left
          Motor_TurnLeft();
          break;

        case 'S': // Backward
          Motor_Backward();
          break;

        case 'D': // Turn right
          Motor_TurnRight();
          break;

        // Camera control
        case 'L': // Camera turn left
          break;

        case 'R': // Camera turn right
          break;

        // Stop (button released)
        case 'X':
          Motor_Stop();
          break;
        }
      }
    }
  }
}