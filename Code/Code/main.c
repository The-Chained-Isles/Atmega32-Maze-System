//---------LIBRARIES---------
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h> //sprintf
#include <avr/interrupt.h>
#include <stdbool.h> //to use booleans
#include <string.h>//to use strcat
//---------UINT---------
//there are 3 types of unsigned(u) int used in this code
//uint8_t: for small data 8 bits, ranges from 0-255
//uint16_t: for big data 16 bits, ranges from 0-65535
//uint32_t: for bigger data 32 bits, ranges from 0-4 billions (only used in ultrasonic)
uint8_t cycle=0, //used for choosing questions randomly based on timer0
//each time timer 0 overflows; cycle increments, making the questions vary according to time
Door_Num=0, //used for setting which door are you standing on
TimerOverflow; //used for ultrasonic time counting
//-------------QUESTIONS--------- There are 16 questions:
bool QMemory[16]; //sets on 1 if the question got used, so that it doesn't get chosen again
char* questions[16][2]={ //16 questions chosen randomly by cycle
	//How Many fingers
	//is Hasan raising
	{"3 + 2 = ?"},
	{"8 - 3 = ?"},
	{"5 * 2 = ?"},
	{"9 / 3 = ?"},
	{"7 + 4 = ?"},
	{"6 - 2 = ?"},
	{"4 * 3 = ?"},
	{"10 / 5 = ?"},
	{"5 + 6 = ?"},
	{"12 - 5 = ?"},
	{"3 * 3 = ?"},
	{"15 / 3 = ?"},
	{"y=3x, dy/dx=?"},
	{"lim ((e^x-1)/x)","x->0"},
	{"Ahmed plays","ahmed is ?"},
	{"How Many fingers?","is Hasan raising"}
};
char* answers[16][2] = {
	//   0     1       2     3 (indices in correct_answers array)
	{"1) 4  2) 5", "3) 6  4) 7"},
	{"1) 4  2) 5", "3) 6  4) 7"},
	{"1) 9  2) 12", "3) 10  4) 15"},
	{"1) 2  2) 3", "3) 4  4) 5"},
	{"1) 10  2) 11", "3) 12  4) 13"},
	{"1) 3  2) 4", "3) 5  4) 6"},
	{"1) 10  2) 11", "3) 12  4) 13"},
	{"1) 1  2) 2", "3) 3  4) 4"},
	{"1) 10  2) 11", "3) 12  4) 13"},
	{"1) 6  2) 7", "3) 8  4) 9"},
	{"1) 6  2) 7", "3) 8  4) 9"},
	{"1) 4  2) 7", "3) 6  4) 5"},
	{"1) 4  2) 0", "3) 6  4) 3"},
	{"1) 1  2) 0", "3) e  4) 1/e"},
	{"1) noun  2) adj", "3) verb  4)adverb"},
	{"1) 0  2) 2", "3) 1  4) 10"}
};
uint8_t correct_answers[16] = {1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 3, 3, 3, 0, 0, 2};
//---------FUNCTION INITIALISATION---------
void ADC_init();
uint16_t ADC_Read(uint8_t pin);
void CheckSensors();
void Timer0_Init();
void Timer1_Init();
void BeMode(uint8_t cmd);
void BeMessage(char* str);
bool CheckTemperature();
ISR(TIMER0_OVF_vect) { //%: remainder
	cycle = (cycle + 1) % 16; //there are 16 questions, so it is not approprite to increment beyond 15
	//this makes it that cycle is only in range (0-15)
	//when it reaches 16 it gets reset back to 0 using the %
	//because 16%16 =0
}
bool CheckAnswer(uint8_t Door_Num);
bool winner (void);
void LCD_Init(void);
void setB(uint8_t door,bool set);
void open(uint8_t Door);
ISR(TIMER1_OVF_vect){
	TimerOverflow++;	/* Increment Timer Overflow count */
}
double ultra(void);
//---------MAIN----------
int main(void){
	ADC_init();
	DDRA=0b01111110;//will use PA0 for temp sensor,PA7 for force sensor, the rest are for the LCD
	DDRB=0; // PB(0-5) for 6 doors
	DDRD=1; //will use PD(2-5) for Keypad, PD0 for trig, PD6 for Echo
	DDRC=0xFF; // set Pins PC5 for buzzer, PC(0-2) for blu/GRN/red leds
	PORTB=0,PORTC=1,PORTD=0b01111100,PORTA=0;_delay_ms(20); //Reset Ports
	open(7);
	Timer1_Init(),Timer0_Init();
	LCD_Init();BeMessage("HALLO");
	_delay_ms(50);
	
	while (1)
	{
		uint8_t FSR_Voltage = ADC_Read(7); //read voltage from FSR
		uint8_t Players_First_Gate= (Players_First_Gate>300) ? 2 : (FSR_Voltage>20); //voltage>300: 2 players,>20: 1player,>=0 player
		while(Players_First_Gate!=1){ //loop until number of players on start is 1
			FSR_Voltage = ADC_Read(7);
			Players_First_Gate= (Players_First_Gate>300) ? 2 : (FSR_Voltage>20);
			LCD_Init();
			if (Players_First_Gate&2)
			BeMessage("1 PLAYER ONLY");
			else
			BeMessage("NO PLAYER FOUND");
			_delay_ms(1000);
		}
		if (winner())
		BeMode(1),//ehfaz da, resets the screen
		//THIS IS MY
		BeMessage("PERFECT VICTORY!"),
		_delay_ms(1500),
		BeMode(0xC0),// new line, ehfaz da
		//THAT'S RIGHT
		BeMessage("    I WIN"),
		_delay_ms(1500);
		BeMode(1);
		while (ultra()<5) //if distance between player and ultra is less than 5cm (final door)
		BeMode(1),LCD_Init(),
		BeMessage("GET OUT"), //loop and ask the player to get out
		_delay_ms(1000);BeMode(1);
	}
}
//-------FUNCTIONS-------
void Timer0_Init() {
	TCCR0 |= (1 << CS00); // no prescaler
	TIMSK = (1 << TOIE0); // Toggle overflow interrupt enabled TIMER0
	sei(); // Enable global interrupts
}
bool CheckTemperature(){ //NTC IS OUR TEMP SENSOR, it works differently than LM, it has ADC output
	uint16_t Current_NTC_Volt;
	do{
		Current_NTC_Volt = ADC_Read(0); //read count (count varies from (0-1023)
		//if you happened to use an analog sensor, then you shall need to use an ADC (Analog to digital)
		//PORTA is the only port that contains ADC's
		//so you will have to use a PORTA pin
		//in our boards (FARES) pins from PA1 to PA6 are used for LCD display
		//so only pins A0 and A7 are free for ADC
	}while(Current_NTC_Volt==0);//if NTC reads 0 then NTC is not working correctly
	return((Current_NTC_Volt<150)&&(Current_NTC_Volt!=0));//Alert if NTC is heated (voltage drop over 150 counts);
}
bool CheckAnswer(uint8_t Door_Num) {
	uint8_t q = cycle;
	while(QMemory[q]==1) q=(q+1)%16; //get unused question
	QMemory[q]=1;
	while (1) { //infinite loop until broken using return
		LCD_Init(),//reset the screen, can use Bemode(1) for same purpose
		BeMessage(questions[q][0]),_delay_ms(5);
		BeMode(0xC0),//new line
		BeMessage(questions[q][1]),_delay_ms(5);
		LCD_Init(),BeMessage(answers[q][0]),_delay_ms(5);
		BeMode(0xC0),BeMessage(answers[q][1]),_delay_ms(5); // new line
		for (uint8_t i = 2; i <= 5; i++) {
			if (!(PIND & (1 << i))) {
				PORTC |= 1 << 5;
				_delay_ms(50);
				PORTC &= ~(1 << 5); // Buzzer sound
				_delay_ms(10);
				return ((i-2)==correct_answers[q]); //break out of function
			}
		}
		_delay_ms(5);
	}
}
bool winner (void)
{
	LCD_Init();
	memset(QMemory,0,sizeof(QMemory)); //reset questions
	open(6); //reset doors
	uint8_t Tries=0;char Door_str[1];
	while (Tries<3&&Door_Num!=6){
		while (CheckTemperature())LCD_Init(),BeMessage("TEMP ALERT"),_delay_ms(1500);
		//there are 7 states of the maze
		//1st: you are facing door 1
		//2nd: you are facing door 2
		//etc
		//7th you finished the maze
		//so we can say that state is equal to DoorNum+1
		PORTC&=0x11111000; //you are removing the values the first 3 pins (the led)
		PORTC|= (1+Door_Num); //Coloured LEDs in Binary using state
		//you added the new number of the state to PORTC
		//I am not flexing my programming skills
		//the coloured led part is essential for the project since the servos are malfunctioning
		//and we need any hardware indication of which door are we standing on
		sprintf(Door_str,"%d",Door_Num + 1), //"%d" means that i will place an intger into this string
		//sprintf is used for copying intgers into strings for printing
		LCD_Init(),BeMessage("Door "),BeMessage(Door_str),_delay_ms(5); //display door number
		sprintf(Door_str,"%d",3-Tries),
		strcat(Door_str, " Tries Left"), //string concatenate
		BeMode(0xC0),BeMessage(Door_str),_delay_ms(5); //display tries left
		if (CheckAnswer(Door_Num)) {
			if(Door_Num==6) open(6);//close all doors
			else open(++Door_Num-1);//open the door you are facing
			Tries=0, //reset Tries to zero
			PORTC= (PORTC&0xF8) | (1+Door_Num); //Coloured LEDs in Binary zay ely fou2
		}
		else {
			Tries++, //increment Tries
			LCD_Init(),
			BeMessage("Wrong"),
			_delay_ms(20);
		}
	}
	Door_Num=0;
	DDRA=0b01111110,PORTA=0; //reinitalisation because the LCD Gets stuck here
	return (Tries<3);
}
void BeMode(uint8_t cmd) {
	// Send higher nibble on Data transfer pins (PA3-6)
	PORTA = (PORTA & 0x87) | ((cmd >> 1) & 0x78); // Set higher nibble on PA3-6
	PORTA &= ~(1 << 1); // RS = 0 for command
	PORTA |= (1 << 2); // Enable pulse
	_delay_us(20);
	PORTA &= ~(1 << 2); // Disable pulse
	_delay_us(20);

	// Send lower nibble
	PORTA = (PORTA & 0x87) | ((cmd << 3) & 0x78); // Set lower nibble on PA3-6
	PORTA |= (1 << 2); // Enable pulse
	_delay_us(20);
	PORTA &= ~(1 << 2); // Disable pulse
	_delay_ms(20);
}
void setB(uint8_t door,bool set){
	for (int i=0;i<50;i++)
	PORTB|=(1<<(door)), //1ms is 0 deg, 2ms is 90 deg
	(set) ? _delay_ms(1) : _delay_ms(2),
	PORTB &= ~(1<<(door)), //cycle is 20ms in total
	(set) ? _delay_ms(19) : _delay_ms(18);
}
void open(uint8_t Door){
	if (Door<2) setB(Door,true);//open one of the doors in PORTB
	else{//close all doors
		for (uint8_t i =0;i<6;i++)
		setB(i,false);
	}
}
void BeMessage(char* str) {
	for (uint8_t i = 0; str[i] != 0; i++) {
		// Send higher nibble
		PORTA = (PORTA & 0x87) | ((str[i] >> 1) & 0x78); // Set higher nibble on PA3-6
		PORTA |= (1 << 1); // RS = 1 for data
		PORTA |= (1 << 2); // Enable pulse
		_delay_us(15);
		PORTA &= ~(1 << 2); // Disable pulse
		_delay_us(15);

		// Send lower nibble
		PORTA = (PORTA & 0x87) | ((str[i] << 3) & 0x78); // Set lower nibble on PA3-6
		PORTA |= (1 << 2); // Enable pulse
		_delay_us(15);
		PORTA &= ~(1 << 2); // Disable pulse
		_delay_ms(50);
	}
	_delay_ms(1500);
}
void LCD_Init(void) {
	_delay_ms(20); // LCD power on delay
	BeMode(0x02); // Initialize LCD in 4-bit (nibbles) mode
	BeMode(0x28); // 2 line, 5x7 matrix
	BeMode(0x0C); // Display on, cursor off
	BeMode(0x06); // Increment cursor (shift cursor to right)
	BeMode(0x01); // Clear display
	_delay_ms(20);
}
void ADC_init(){
	// Set reference voltage to AVcc with external capacitor at AREF pin
	ADMUX |= 1 << REFS0;
	// Enable ADC and set prescaler to 128 (16 MHz / 128 = 125 KHz)
	ADCSRA |= (1 << ADEN) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);
}
uint16_t ADC_Read(uint8_t pin){
	// Select ADC channel with safety mask
	ADMUX = (ADMUX & 0xF8) | (pin & 0x07);
	// Start single conversion
	ADCSRA |= 1 << ADSC;
	// Wait for conversion to complete
	while (ADCSRA & (1 << ADIF));
	// Return the ADC value
	return ADC;
}
void Timer1_Init(){
	TIMSK = (1 << TOIE1); //enable timer overflow interrupt
	TCCR1A = 0; //no prescaler
}
double ultra(void){
	/* Give 10us trigger pulse on trig. pin to HC-SR04 */
	PORTD |= 1;
	_delay_us(10);
	PORTD &= ~1;
	TCNT1 = 0;	/* Clear Timer counter */
	TCCR1B = 0x41;	/* Capture on rising edge, No prescaler*/
	TIFR = 1<<ICF1;	/* Clear ICP flag (Input Capture flag) */
	TIFR = 1<<TOV1;	/* Clear Timer Overflow flag */
	/*Calculate width of Echo by Input Capture (ICP) */
	while ((TIFR & (1 << ICF1)) == 0);/* Wait for rising edge */
	TCNT1 = 0;	/* Clear Timer counter */
	TCCR1B = 0x01;	/* Capture on falling edge, No prescaler */
	TIFR = 1<<ICF1;	/* Clear ICP flag (Input Capture flag) */
	TIFR = 1<<TOV1;	/* Clear Timer Overflow flag */
	TimerOverflow = 0;/* Clear Timer overflow count */
	while ((TIFR & (1 << ICF1)) == 0);/* Wait for falling edge */
	uint32_t count = ICR1 + (65535 * TimerOverflow);	/* Take count */
	/* 16MHz Timer freq, sound speed =343 m/s */
	double distance = (double)count / 932.94;
	return distance;
}