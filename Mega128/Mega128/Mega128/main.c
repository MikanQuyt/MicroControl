/* SPI Slave Receiver (Mega128) & Master Sender (ESP Cam)
 * Author: mikan
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile char received_cmd = 0;
volatile uint8_t new_cmd_flag = 0;

//Initialize SPI in Slave Mode
void SPI_SlaveInit(void) {
    /* Set data direction for SPI pins on Port B:
     * PB0 (SS)   : Input
     * PB1 (SCK)  : Input
     * PB2 (MOSI) : Input Using when Mega128-V2 is just a receiver (slave)
     * PB3 (MISO) : Output
     */
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
    DDRB |= (1 << PB3);

    //Enable SPI (SPE = 1) and enable SPI Interrupt (SPIE = 1)
    //MSTR bit is 0 by default, so the MCU operates in Slave Mode
    SPCR = (1 << SPE) | (1 << SPIE);
}

//Interrupt Service Routine: Runs automatically when 1 byte is received from ESP32-CAM
ISR(SPI_STC_vect) {
    received_cmd = SPDR; //Read the character ('W', 'A', 'S', 'D', '0', '1', 'C', 'P', 'X') from SPI Data Register
    new_cmd_flag = 1;    //Set flag to notify main loop that a new command is available
}

int main(void) {
    //Initialize PE2 and PE3 as Outputs for LEDs
    DDRE |= (1 << PE2) | (1 << PE3);
    
    //Ensure both LEDs are OFF at startup (LOW level)
    PORTE &= ~((1 << PE2) | (1 << PE3)); 

    //Initialize SPI
    SPI_SlaveInit();

    //Enable Global Interrupts
    sei();

    while (1) {
        //Check if ESP-CAM has sent a new command
        if (new_cmd_flag) {
            new_cmd_flag = 0; //Clear flag to prepare for the next command

            //Alternating LED sequence
            
            //PE2 ON, PE3 OFF
            PORTE |= (1 << PE2);       
            PORTE &= ~(1 << PE3);      
            _delay_ms(100); //Wait for 100ms

            //PE2 OFF, PE3 ON
            PORTE &= ~(1 << PE2);      
            PORTE |= (1 << PE3);       
            _delay_ms(100); // Wait for 100ms

            //Turn OFF both LEDs
            PORTE &= ~((1 << PE2) | (1 << PE3)); 
            
            //Motor control logic can be handled here based on the received character.
            switch (received_cmd) {
			   //Case to connect car
			   case 'C': //Code for connect to car
			   break;
			   
			   case 'P': //Code for stop connecting to car
			   break;
			   
			   
			   //Case to control car move
			   case 'W': //Code for Forward
               break;
			   
			   case 'A': //Code for turn left
			   break;
			   
			   case 'S': //Code for Backward
			   break;
			   
			   case 'D': //Code for turn right
			   break;
			   
			   //Case to control camera
			   case 'L':
			   break;
			   
			   case 'R':
			   break;
			   
			   //Case to control the speed of car
			   case '0': //Code for control camera Turn Right
			   break;
			   
			   case '1': //Code for control camera Turn Left
			   break;
			   
			   //Case to stop the command avoid to run always code
               case 'X': // Code for Stop
               break;
            }
        }
    }
}