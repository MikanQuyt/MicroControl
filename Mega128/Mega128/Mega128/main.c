#define F_CPU 16000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#define AIN1 PC2
#define AIN2 PC3
#define BIN1 PC4
#define BIN2 PC5

#define STBY PA4

#define FTRIG_PIN PA0
#define FECHO_PIN PA1
#define LTRIG_PIN PA2
#define LECHO_PIN PA3
#define RTRIG_PIN PA5
#define RECHO_PIN PA6

#define MPU6050_ADDR        0x68
#define MPU6050_PWR_MGMT_1  0x6B
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_GYRO_ZOUT_H 0x47
#define MPU6050_GYRO_ZOUT_L 0x48

#define ENCODER_THRESHOLD 512
#define SLOTS_PER_REV     20

#define WHEEL_SPACING_CM  15
#define WHEEL_CIRCUM_CM   21

#define MOTOR_A_PERCENT 70
#define MOTOR_B_PERCENT 80

#define GYRO_DEADZONE   50
#define HEADING_GAIN     1
#define MAX_CORRECTION  30

#define OBSTACLE_CM 6

static const uint8_t speed_table[11] = {
    0, 25, 51, 76, 102, 127, 153, 178, 204, 229, 255
};

volatile char received_cmd = 0;
volatile uint8_t new_cmd_flag = 0;

static uint8_t current_speed = 0;

volatile uint16_t enc_left_count  = 0;
volatile uint16_t enc_right_count = 0;
static uint8_t enc_left_prev  = 0;
static uint8_t enc_right_prev = 0;

void SPI_SlaveInit(void) {
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
    DDRB |= (1 << PB3);
    SPCR = (1 << SPE) | (1 << SPIE);
}

ISR(SPI_STC_vect) {
    received_cmd = SPDR;
    new_cmd_flag = 1;
}

void I2C_Init(void) {
    TWSR = 0x00;
    TWBR = 72;
    TWCR = (1 << TWEN);
}

void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(10);
}

void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

uint8_t I2C_ReadACK(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

uint8_t I2C_ReadNACK(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

void MPU6050_WriteReg(uint8_t reg, uint8_t value) {
    I2C_Start();
    I2C_Write(MPU6050_ADDR << 1);
    I2C_Write(reg);
    I2C_Write(value);
    I2C_Stop();
}

uint8_t MPU6050_ReadReg(uint8_t reg) {
    I2C_Start();
    I2C_Write(MPU6050_ADDR << 1);
    I2C_Write(reg);
    I2C_Start();
    I2C_Write((MPU6050_ADDR << 1) | 1);
    uint8_t data = I2C_ReadNACK();
    I2C_Stop();
    return data;
}

void MPU6050_Init(void) {
    _delay_ms(100);
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
    _delay_ms(50);
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x00);
}

int16_t MPU6050_ReadGyroZ(void) {
    uint8_t high = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
    uint8_t low  = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
    return (int16_t)((high << 8) | low);
}

void ADC_Init(void) {
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_Read(uint8_t channel) {
    ADMUX = (1 << REFS0) | (channel & 0x07);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void Encoder_Update(void) {
    uint16_t adc_left = ADC_Read(0);
    uint8_t left_now = (adc_left > ENCODER_THRESHOLD) ? 1 : 0;
    if (left_now && !enc_left_prev) {
        enc_left_count++;
    }
    enc_left_prev = left_now;

    uint16_t adc_right = ADC_Read(1);
    uint8_t right_now = (adc_right > ENCODER_THRESHOLD) ? 1 : 0;
    if (right_now && !enc_right_prev) {
        enc_right_count++;
    }
    enc_right_prev = right_now;
}

void Encoder_Reset(void) {
    enc_left_count  = 0;
    enc_right_count = 0;
}

void PWM_Init(void) {
    DDRE |= (1 << PE3) | (1 << PE4);
    TCCR3A = (1 << COM3A1) | (1 << COM3B1) | (1 << WGM30);
    TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
    OCR3A = 0;
    OCR3B = 0;
}

void Motor_Init(void) {
    DDRA |= (1 << STBY);
    DDRC |= (1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2);
    PORTC &= ~((1 << AIN1) | (1 << AIN2) | (1 << BIN1) | (1 << BIN2));
    PORTA &= ~(1 << STBY);
}

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

uint16_t Pulses_For_Angle(uint16_t angle_deg) {
    uint32_t arc_x100 = (uint32_t)angle_deg * 314UL * WHEEL_SPACING_CM / 360UL;
    uint16_t pulses = (uint16_t)((arc_x100 * SLOTS_PER_REV) / (100UL * WHEEL_CIRCUM_CM));
    if (pulses == 0) pulses = 1;
    return pulses;
}

void Motor_TurnLeftAngle(uint16_t angle_deg) {
    uint16_t target = Pulses_For_Angle(angle_deg);
    Encoder_Reset();
    Motor_TurnLeft();
    while (enc_right_count < target) {
        Encoder_Update();
    }
    Motor_Stop();
}

void Motor_TurnRightAngle(uint16_t angle_deg) {
    uint16_t target = Pulses_For_Angle(angle_deg);
    Encoder_Reset();
    Motor_TurnRight();
    while (enc_left_count < target) {
        Encoder_Update();
    }
    Motor_Stop();
}

void Motor_ForwardStraight(uint16_t duration_ms) {
    PORTC |= (1 << AIN2) | (1 << BIN2);
    PORTC &= ~((1 << AIN1) | (1 << BIN1));

    uint16_t base_a = ((uint16_t)current_speed * MOTOR_A_PERCENT) / 100;
    uint16_t base_b = ((uint16_t)current_speed * MOTOR_B_PERCENT) / 100;

    uint16_t elapsed = 0;
    while (elapsed < duration_ms) {
        int16_t gz = MPU6050_ReadGyroZ();

        int16_t correction = 0;
        if (gz > GYRO_DEADZONE) {
            correction = (gz - GYRO_DEADZONE) * HEADING_GAIN / 131;
        } else if (gz < -GYRO_DEADZONE) {
            correction = (gz + GYRO_DEADZONE) * HEADING_GAIN / 131;
        }

        if (correction > MAX_CORRECTION)  correction = MAX_CORRECTION;
        if (correction < -MAX_CORRECTION) correction = -MAX_CORRECTION;

        int16_t pwm_a = (int16_t)base_a + correction;
        int16_t pwm_b = (int16_t)base_b - correction;

        if (pwm_a < 0)   pwm_a = 0;
        if (pwm_a > 255) pwm_a = 255;
        if (pwm_b < 0)   pwm_b = 0;
        if (pwm_b > 255) pwm_b = 255;

        OCR3A = (uint8_t)pwm_a;
        OCR3B = (uint8_t)pwm_b;

        Encoder_Update();
        _delay_ms(10);
        elapsed += 10;
    }
    Motor_Stop();
}

void Ultrasonic_Init(void) {
    DDRA |= (1 << FTRIG_PIN) | (1 << LTRIG_PIN) | (1 << RTRIG_PIN);
    PORTA &= ~((1 << FTRIG_PIN) | (1 << LTRIG_PIN) | (1 << RTRIG_PIN));
    DDRA &= ~((1 << FECHO_PIN) | (1 << LECHO_PIN) | (1 << RECHO_PIN));
    PORTA &= ~((1 << FECHO_PIN) | (1 << LECHO_PIN) | (1 << RECHO_PIN));
}

uint16_t Ultrasonic_ReadCm(uint8_t trig_pin, uint8_t echo_pin) {
    PORTA |= (1 << trig_pin);
    _delay_us(10);
    PORTA &= ~(1 << trig_pin);

    uint16_t timeout = 60000;
    while (!(PINA & (1 << echo_pin))) {
        if (--timeout == 0) return 0;
    }

    uint16_t count = 0;
    while (PINA & (1 << echo_pin)) {
        _delay_us(1);
        count++;
        if (count > 25000) return 0;
    }

    return count / 58;
}

uint16_t Front_Distance(void) { return Ultrasonic_ReadCm(FTRIG_PIN, FECHO_PIN); }
uint16_t Left_Distance(void)  { return Ultrasonic_ReadCm(LTRIG_PIN, LECHO_PIN); }
uint16_t Right_Distance(void) { return Ultrasonic_ReadCm(RTRIG_PIN, RECHO_PIN); }

void Bell_Init(void) {
    DDRB |= (1 << PB7);
    PORTB &= ~(1 << PB7);
}

void Bell_Ring(void) {
    PORTB |= (1 << PB7);
    _delay_ms(200);
    PORTB &= ~(1 << PB7);
}

void Auto_Drive(void) {
    uint16_t f = Front_Distance();
    uint16_t l = Left_Distance();
    uint16_t r = Right_Distance();

    uint8_t f_blocked = (f > 0 && f < OBSTACLE_CM) || (f == 0);
    uint8_t l_blocked = (l > 0 && l < OBSTACLE_CM) || (l == 0);
    uint8_t r_blocked = (r > 0 && r < OBSTACLE_CM) || (r == 0);

    if (!f_blocked) {
        Motor_ForwardStraight(200);
    } else if (!l_blocked) {
        Motor_TurnLeftAngle(90);
    } else if (!r_blocked) {
        Motor_TurnRightAngle(90);
    } else {
        Motor_Backward();
        _delay_ms(400);
        Motor_Stop();
        _delay_ms(100);
        Motor_TurnLeftAngle(90);
    }
}

int main(void) {
    SPI_SlaveInit();
    I2C_Init();
    MPU6050_Init();
    ADC_Init();
    PWM_Init();
    Motor_Init();
    Ultrasonic_Init();
    Bell_Init();

    sei();

    uint8_t is_active = 0;

    while (1) {
        Encoder_Update();

        if (new_cmd_flag) {
            new_cmd_flag = 0;
            char cmd = received_cmd;

            if (cmd == 'C') {
                PORTA |= (1 << STBY);
                Bell_Ring();
                is_active = 1;

            } else if (cmd == 'P') {
                if (is_active == 1) {
                    is_active = 2;
                } else {
                    PORTA &= ~(1 << STBY);
                    Motor_Stop();
                    is_active = 0;
                }

            } else if (is_active == 1) {

                if (cmd >= '0' && cmd <= '9') {
                    current_speed = speed_table[cmd - '0'];
                    Motor_SetSpeed();
                } else if (cmd == 'M') {
                    current_speed = speed_table[10];
                    Motor_SetSpeed();

                } else {
                    switch (cmd) {
                        case 'W': Motor_Forward();   Motor_Stop(); break;
                        case 'A': Motor_TurnLeft();  Motor_Stop(); break;
                        case 'S': Motor_Backward();  Motor_Stop(); break;
                        case 'D': Motor_TurnRight(); Motor_Stop(); break;
                        case 'X': Motor_Stop();  break;
                    }
                }
            }
        }

        if (is_active == 2) {
            Auto_Drive();
            _delay_ms(50);
        }
    }
}
