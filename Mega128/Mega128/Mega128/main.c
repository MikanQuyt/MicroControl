#define F_CPU 16000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#define AIN1 PC0
#define AIN2 PC1
#define BIN1 PC2
#define BIN2 PC3

#define STBY PA4

static const uint8_t speed_table[11] = {0,   25,  51,  76,  102, 127,
                                        153, 178, 204, 229, 255};

volatile char received_cmd = 0;
volatile uint8_t new_cmd_flag = 0;

static uint8_t current_speed = 0;

#define SERVO_MIN 250
#define SERVO_MID 375
#define SERVO_MAX 500
#define SERVO_STEP 12 

static uint16_t servo_pos = 375; 

void SPI_SlaveInit(void) {
  DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
  DDRB |= (1 << PB3);

  SPCR = (1 << SPE) | (1 << SPIE);
}

void PWM_Init(void) {
  DDRE |= (1 << PE3) | (1 << PE4);

  TCCR3A = (1 << COM3A1) | (1 << COM3B1) | (1 << WGM30);
  TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);

  OCR3A = 0;
  OCR3B = 0;
}

void Servo_Init(void) {
  DDRB |= (1 << PA6); 

  TCCR1A = (1 << COM1C1) | (1 << WGM11);
  TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10);

  ICR1 = 4999;       
  OCR1C = SERVO_MID; 
}

void Motor_Init(void) {
  DDRA |= (1 << STBY);
  DDRC |= (1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2);

  PORTC &= ~((1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2));
  PORTA &= ~(1 << STBY);
}

#define MOTOR_A_PERCENT 82  
#define MOTOR_B_PERCENT 100 

void Motor_SetSpeed(void) {
  OCR3A = ((uint16_t)current_speed * MOTOR_A_PERCENT) / 100;
  OCR3B = ((uint16_t)current_speed * MOTOR_B_PERCENT) / 100;
}

void Motor_Stop(void) {
  PORTC &= ~((1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2));
  OCR3A = 0;
  OCR3B = 0;
}

void Motor_Forward(void) {
  PORTC |= (1 << AIN2) | (1 << BIN2);
  PORTC &= ~((1 << AIN1) | (1 << BIN1));
  Motor_SetSpeed();
}

void Motor_Backward(void) {
  PORTC &= ~((1 << AIN2) | (1 << BIN2));
  PORTC |= (1 << AIN1) | (1 << BIN1);
  Motor_SetSpeed();
}

void Motor_TurnLeft(void) {
  PORTC &= ~(1 << AIN2);
  PORTC |= (1 << AIN1);
  PORTC |= (1 << BIN2);
  PORTC &= ~(1 << BIN1);
  Motor_SetSpeed();
}

void Motor_TurnRight(void) {
  PORTC |= (1 << AIN2);
  PORTC &= ~(1 << AIN1);
  PORTC &= ~(1 << BIN2);
  PORTC |= (1 << BIN1);
  Motor_SetSpeed();
}

void Servo_Left(void) {
  if (servo_pos > SERVO_MIN + SERVO_STEP) {
    servo_pos -= SERVO_STEP;
  } else {
    servo_pos = SERVO_MIN;
  }
  OCR1C = servo_pos;
}

void Servo_Right(void) {
  if (servo_pos < SERVO_MAX - SERVO_STEP) {
    servo_pos += SERVO_STEP;
  } else {
    servo_pos = SERVO_MAX;
  }
  OCR1C = servo_pos;
}

ISR(SPI_STC_vect) {
  received_cmd = SPDR;
  new_cmd_flag = 1;
}

int main(void) {
  SPI_SlaveInit();
  PWM_Init();
  Servo_Init();
  Motor_Init();

  sei();

  while (1) {
    if (new_cmd_flag) {
      new_cmd_flag = 0;
      char cmd = received_cmd;

      if (cmd >= '0' && cmd <= '9') {
        current_speed = speed_table[cmd - '0'];
        Motor_SetSpeed();
      } else if (cmd == 'M') {
        current_speed = speed_table[10];
        Motor_SetSpeed();
      } else {
        switch (cmd) {
        case 'C':
          PORTA |= (1 << STBY);
          break;

        case 'P':
          PORTA &= ~(1 << STBY);
          Motor_Stop();
          break;

        case 'W':
          Motor_Forward();
          _delay_ms(500);
          Motor_Stop();
          break;

        case 'A':
          Motor_TurnLeft();
          _delay_ms(50);
          Motor_Stop();
          break;

        case 'S':
          Motor_Backward();
          _delay_ms(500);
          Motor_Stop();
          break;

        case 'D':
          Motor_TurnRight();
          _delay_ms(50);
          Motor_Stop();
          break;

        case 'L':
          Servo_Left();
          break;

        case 'R':
          Servo_Right();
          break;

        case 'X':
          Motor_Stop();
          break;
        }
      }
    }
  }
}
