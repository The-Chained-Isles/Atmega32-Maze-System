#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
uint8_t cycle=0,Door_Num=0,TimerOverflow;
bool CheckForPlayer=true;
bool QMemory[16];
char* questions[16][2]={ //16 questions chosen randomly by cycle
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
	{"How Many fingers?","is Hassan raising"}
};
char* answers[16][2] = {
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
void ADC_init();
uint16_t ADC_Read(uint8_t pin);
void CheckSensors();
void Timer0_Init();
void Timer1_Init();
void BeMode(uint8_t cmd);
void BeMessage(char* str);
bool CheckTemperature();
ISR(TIMER0_OVF_vect) {
	cycle = (cycle + 1) % 16;
}
bool CheckAnswer(uint8_t Door_Num);
bool winner (void);
void LCD_Init(void);
void setB(bool door,bool set);
void setC(uint8_t door,bool set);
void open(uint8_t Door);
ISR(TIMER1_OVF_vect){
	TimerOverflow++;	/* Increment Timer Overflow count */
}
double ultra(void);
int main(void){
	ADC_init();
	DDRA=0b01111110;//will use PA0 for temp sensor,PA7 for force sensor, the rest are for the LCD
	DDRB=0b11000000; // will use and PB(0-4) for door sensors input, and PB(6-7) for 2 doors
	DDRD=1; //will use PD(2-5) for Keypad, PD0 for trig, PD6 for Echo
	DDRC=0xFF; // set Pins PC5 for buzzer, PC(0-2) for blu/GRN/red leds, other pins for 4 doors
	PORTB=0,PORTC=1,PORTD=0x40,PORTA=0;_delay_ms(20); //Reset Ports
	open(7);
	Timer1_Init(),Timer0_Init();
	LCD_Init();BeMessage("HALLO");
	_delay_ms(50);
	while (1)
	{
		uint8_t FSR_Voltage = ADC_Read(7);
		uint8_t Players_First_Gate= (Players_First_Gate>300) ? 2 : (FSR_Voltage!=0);
		while(!(Players_First_Gate&1)){
			FSR_Voltage = ADC_Read(7);
			Players_First_Gate= (Players_First_Gate>300) ? 2 : (FSR_Voltage!=0);
			LCD_Init();
			if (Players_First_Gate&2)
			BeMessage("1 PLAYER ONLY");
			else
			BeMessage("NO PLAYER FOUND");
			_delay_ms(1000);
		}
		if (winner())
		LCD_Init(),
		BeMessage("Congratulations!"),
		_delay_ms(1500),
		BeMode(0xC0),
		BeMessage("    YOU WIN"),// new line
		_delay_ms(1500);
		while (ultra()<5)
		LCD_Init(),
		BeMessage("GET OUT"),
		_delay_ms(1000);
	}
}
void Timer0_Init() {
	TCCR0 |= (1 << CS00); // no prescaler
	TIMSK = (1 << TOIE0);
	sei(); // Enable global interrupts
}
bool CheckTemperature(){
	uint16_t Current_NHC_Volt = ADC_Read(0);
	return ((Current_NHC_Volt<250)&&Current_NHC_Volt); //Alert if NHC is heated (voltage drop over 250 counts);
}
bool CheckAnswer(uint8_t Door_Num) {
	uint8_t q = cycle;
	while(QMemory[q]) q=(q+1)%16; //get unused question
	QMemory[q]=true;
	// Disable Timer1 Compare Match A interrupt
	
	while (true) {
		LCD_Init(),BeMessage(questions[q][0]),_delay_ms(5);
		BeMode(0xC0),BeMessage(questions[q][1]),_delay_ms(5);
		LCD_Init(),BeMessage(answers[q][0]),_delay_ms(5);
		BeMode(0xC0),BeMessage(answers[q][1]),_delay_ms(5); // new line

		for (uint8_t i = 2; i <= 5; i++) {
			if (PIND & (1 << i)) {
				PORTC |= 1 << 5;
				_delay_ms(50);
				PORTC &= ~(1 << 5); // Buzzer sound
				_delay_ms(10);
				return ((i-2)==correct_answers[q]);
			}
		}
		_delay_ms(5);
	}
}
bool winner (void)
{
	memset(QMemory,0,sizeof(QMemory)); //reset questions
	open(6); //reset doors
	uint8_t Tries=0;char Door_str[1];
	while (Tries<3&&Door_Num!=6){
		while (CheckTemperature())LCD_Init(),BeMessage("TEMP ALERT"),_delay_ms(1500);
		PORTC= (PORTC&0xF8)|(Door_Num+1),
		sprintf(Door_str,"%d",Door_Num + 1),
		LCD_Init(),BeMessage("Door "),BeMessage(Door_str),_delay_ms(5); //display door number
		sprintf(Door_str,"%d",3-Tries),
		strcat(Door_str, " Tries Left"),
		BeMode(0xC0),BeMessage(Door_str),_delay_ms(5); //display tries left
		if (CheckAnswer(Door_Num)) {
			(Door_Num==6)? open(6) : open(++Door_Num-1), // open the door // open the door
			Tries=0, //reset Tries to zero
			PORTC= (PORTC&0xF8) | (1+Door_Num); //Coloured LEDs in Binary
		}
		else {
			Tries++, //increment Tries
			LCD_Init(),
			BeMessage("Wrong"),
			_delay_ms(20);
		}
	}
	Door_Num=0;
	return (Tries<3);
}
void BeMode(uint8_t cmd) {
	// Send higher nibble
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
void setB(bool door,bool set){
	for (int i=0;i<50;i++)
	PORTB|=(1<<(door+6)),
	(set) ? _delay_ms(1) : _delay_ms(2),
	PORTB &= ~(1<<(door+6)),
	(set) ? _delay_ms(19) : _delay_ms(18);
}
void setC(uint8_t door,bool set){
	door+= (door<4) ? 1 : 2;
	for (int i=0;i<50;i++)
	PORTD|=(1<<door),
	(set) ? _delay_ms(1) : _delay_ms(2),
	PORTD &= ~(1<<door),
	(set) ? _delay_ms(19) : _delay_ms(18);
}
void open(uint8_t Door){
	if (Door<2) setB(Door,true);
	else if (Door<6) setC(Door,true);
	else{
		setB(false,false),
		setB(true,false);
		for (uint8_t i =2;i<6;i++)
		setC(i,false);
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
		_delay_ms(15);
	}
}
void LCD_Init(void) {
	_delay_ms(20); // LCD power on delay
	BeMode(0x02); // Initialize LCD in 4-bit mode
	BeMode(0x28); // 2 line, 5x7 matrix
	BeMode(0x0C); // Display on, cursor off
	BeMode(0x06); // Increment cursor (shift cursor to right)
	BeMode(0x01); // Clear display
	_delay_ms(20);
}
void ADC_init(){
	ADMUX|=1<<REFS0;
	ADCSRA|=(1<<ADEN)|(1<<ADPS0)|(1<<ADPS1)|(1<<ADPS2);
}
uint16_t ADC_Read(uint8_t pin){
	ADMUX = (ADMUX&0xF8)|(pin&0x07);
	ADCSRA|=1<<ADSC;
	while(ADCSRA&(1<<ADIF));
	return ADC;
}
void Timer1_Init(){
	TIMSK = (1 << TOIE1);
	TCCR1A = 0;
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