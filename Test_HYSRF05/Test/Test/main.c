/*
 * main.c
 *
 * Created: 18-Apr-26 2:56:17 PM
 *  Author: mikan
 */ 



#define F_CPU 16000000UL
#include <xc.h>
#include <avr/io.h>
#include <util/delay.h>
/*
#define F_CPU 16000000UL // Gi? ??nh th?ch anh 16MHz, ch?nh l?i n?u b?n dùng t?n s? khác
#include <avr/io.h>
#include <util/delay.h>

void hc_sr05_init() {
	// C?u hình PA0 (Trig) là Output, PA3 (LED) là Output
	DDRA |= (1 << PA0) | (1 << PA3);
	// C?u hình PA1 (Echo) là Input
	DDRA &= ~(1 << PA1);
	// T?t LED ban ??u
	PORTA &= ~(1 << PA3);
}

uint16_t get_distance() {
	uint16_t count = 0;

	// 1. T?o xung Trig (ít nh?t 10us)
	PORTA &= ~(1 << PA0);
	_delay_us(2);
	PORTA |= (1 << PA0);
	_delay_us(10);
	PORTA &= ~(1 << PA0);

	// 2. Ch? Echo lên m?c cao (High)
	// Thêm ki?m tra timeout ?? tránh treo chip n?u không nh?n ???c ph?n h?i
	uint32_t timeout = 100000;
	while (!(PINA & (1 << PA1)) && timeout--);
	if (timeout == 0) return 999; // L?i c?m bi?n

	// 3. ??m th?i gian Echo ? m?c cao
	// M?i vòng l?p này x?p x? vài micro giây tùy vào F_CPU
	while ((PINA & (1 << PA1))) {
		count++;
		_delay_us(1);
		if (count > 20000) break; // Quá xa (ngoài ph?m vi 3m)
	}

	// 4. Tính toán kho?ng cách (cm)
	// Công th?c th?c t? ph? thu?c vào t?c ?? vòng l?p,
	// V?i _delay_us(1), h? s? th?c nghi?m th??ng chia kho?ng 58-60
	return (count / 58);
}

int main(void) {
	hc_sr05_init();
	uint16_t distance = 0;

	while (1) {
		distance = get_distance();

		// Ki?m tra ?i?u ki?n: 0 < kho?ng cách < 5 cm
		if (distance > 0 && distance < 5) {
			PORTA |= (1 << PA3);  // B?t LED
			} else {
			PORTA &= ~(1 << PA3); // T?t LED
		}

		_delay_ms(60); // Ch? gi?a các l?n ?o ?? tránh nhi?u sóng âm
	}
}
*/


//Define led pin
#define Y_LED PA4
#define G_LED PA5
#define R_LED PE7

//Define status for led
//#define ON 1
//#define OFF 0

//Define connect pin for the first sensor
#define F_TRIG PE2
#define F_ECHO PE3
#define F_DDR DDRE

#define F_PORT PORTE
#define F_PIN PINE

//Define connect pin for the second sensor and the third sensor
#define S_TRIG PA0
#define S_ECHO PA1
#define T_TRIG PA2
#define T_ECHO PA3

#define S_DDR DDRA
#define S_PORT PORTA
#define S_PIN PINA

//Function to control led to light
void control_led(uint8_t led_name){
	PORTE &= ~(1 << R_LED);
	PORTA &= ~((1 << Y_LED)|(1 << G_LED));
	if(led_name == R_LED){
		PORTE |= (1 << led_name);
	}
	if(led_name == Y_LED || led_name == G_LED){
		PORTA |= (1 << led_name);
	}
}


//Function get distance from sensors
uint16_t get_distance(volatile uint8_t *trig_port, uint8_t trig_pin, volatile uint8_t *echo_pin_reg, uint8_t echo_pin){
	
	//Declare counting variable
	uint32_t distance = 0;
	
	//Ensure Timer1 in normal mode and reset divider
	TCCR1A = 0;
	TCCR1B = 0;
	
	//Sending active pulse (Trigger)
	*trig_port &= ~(1 << trig_pin);
	_delay_us(2);
	*trig_port |= (1 << trig_pin);
	_delay_us(20);
	*trig_port &= ~(1 << trig_pin);
	
	//Waiting Echo = 1, add limit not to get lag (timeout)
	uint16_t timeout = 100000;
	while (!(*echo_pin_reg & (1 << echo_pin)) && timeout--);
	if(timeout == 0) return 0;
	
	//Start to count (Prescaler 8)
	TCNT1 = 0;
	TCCR1B |= (1 << CS11);
	while((*echo_pin_reg & (1 << echo_pin)) && (TCNT1 < 50000));
	
	TCCR1B = 0; //Stop timer
	distance = (uint16_t)(TCNT1 * 0.0085);
	
	//Get cm
	return distance;
}


uint16_t read_stable_distance(volatile uint8_t *trig_port, uint8_t trig_pin, volatile uint8_t *echo_pin_reg, uint8_t echo_pin) {
	uint32_t sum = 0;
	uint8_t valid_sample = 0;

	for(uint8_t i = 0; i < 3; i++) {
		uint16_t d = get_distance(trig_port, trig_pin, echo_pin_reg, echo_pin);
		if(d > 0) {
			sum += d;
			valid_sample++;
		}
		_delay_ms(35);
	}
	
	if(valid_sample == 0) return 0;
	return (uint16_t)(sum / valid_sample);
}


int main(void)
{
	uint8_t temp = MCUCR | (1 << JTD);
	MCUCR = temp;
	MCUCR = temp;
	
	XMCRA  = 0;
	XMCRB = 0;
	
	//Specs of ouput for LED
	DDRE |= (1 << R_LED);
	DDRA |= (1 << Y_LED)|(1 << G_LED);
	
	//Specs for the first sensor
	DDRE |= (1 << F_TRIG);
	DDRE &= ~(1 << F_ECHO);
	PORTE &= ~(1 << F_TRIG);
	PINE &= ~(1 << F_ECHO);
	
	//Specs for another sensors
	DDRA |= (1 << S_TRIG)|(1 << T_TRIG);
	DDRA &= ~((1 << S_ECHO)|(1 << T_ECHO));
	PORTA &= ~((1 << S_TRIG)|(1 << T_TRIG));
	PINA &= ~((1 << S_ECHO)|(1 << T_ECHO));
	
	//Declare distance for each sensor
	uint16_t d1, d2, d3;
	
    while(1)
    {
        //TODO:: Please write your application code
		d1 = read_stable_distance(&PORTE, F_TRIG, &PINE, F_ECHO);
		_delay_ms(50);
		d2 = read_stable_distance(&PORTA, S_TRIG, &PINA, S_ECHO);
		_delay_ms(50);
		d3 = read_stable_distance(&PORTA, T_TRIG, &PINA, T_ECHO);
		_delay_ms(50);
		
		//logic control led
		if(d1 > 0 && d1 < 15) PORTE |= (1 << R_LED);
		else PORTE &= ~(1 << R_LED);
		
		if(d2 > 0 && d2 < 15) PORTA |= (1 << Y_LED);
		else PORTA &= ~(1 << Y_LED);
		
		if(d3 > 0 && d3 < 15) PORTA |= (1 << G_LED);
		else PORTA &= ~(1 << G_LED);
    }
}
