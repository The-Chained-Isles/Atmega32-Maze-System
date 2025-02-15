#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
uint8_t cycle=0,Door_Num=0;
char PlayAlert[16];
bool PrintT=false,PrintP=false;
bool SensorsReadings[6];
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
void Timer1_Init(int denominator);
void BeMode(uint8_t cmd);
void BeMessage(char* str);
void BeM(char* str);
void CheckTemperature();
void CheckPlayers();
ISR(TIMER1_COMPA_vect) {
	//CheckTemperature(),
	CheckSensors(),
	CheckPlayers(),
	cycle = (cycle + 1) % 16,
	Timer1_Init(2); // Reinitialize the timer with the desired denominator
}
bool CheckAnswer(uint8_t Door_Num);
bool winner (void);
void LCD_Init(void);
void setB(bool door,bool set);
void setD(uint8_t door,bool set);
void open(uint8_t Door);
int main(void){
	ADC_init();
	DDRA=0b01111110;//will use PA0 for temp sensor,PA7 for force sensor, the rest are for the LCD
	DDRB=0b11000000; // will use and PB(0-4) for door sensors input, and PB(6-7) for 2 doors
	DDRD=0b11000011; //will use PD(2-5) for Keypad, and other PD pins for 4 doors
	DDRC=0xFF; // set Pins PC5 for buzzer, PC(0-2) for blu/GRN/red leds,
	PORTB=0,PORTC=1,PORTD=0,PORTA=0;_delay_ms(20); //Reset Ports
	memset(SensorsReadings,0,sizeof(SensorsReadings));
	open(7);
	LCD_Init();BeMessage("HALLO");
	_delay_ms(50);
	while(1){
		CheckSensors();
	}
	while (1)
	{
		if (winner())
		{
			LCD_Init(); BeMessage("Congratulations!");_delay_ms(15);
			BeMode(0xC0);BeMessage("    YOU WIN");// new line
			_delay_ms(100);
		}
		CheckForPlayer=0; //check for empty maze
	}
}
void CheckSensors(){
	uint16_t FSR_Voltage = ADC_Read(7);
	SensorsReadings[0]= (FSR_Voltage>705) ? 2 : (FSR_Voltage>630);
	for(int i=0;i<5;i++)
	SensorsReadings[i+1]= (PINB&(1<<i));
	char a[16],b[16],c[16],d[16],e[16],f[16];
	sprintf(a,"%d",SensorsReadings[0]);
	sprintf(b,"%d",SensorsReadings[1]);
	sprintf(c,"%d",SensorsReadings[2]);
	sprintf(d,"%d",SensorsReadings[3]);
	sprintf(e,"%d",SensorsReadings[4]);
	sprintf(f,"%d",SensorsReadings[5]);
	LCD_Init();
	BeMessage("a: "),BeM(a),_delay_ms(1000);
	LCD_Init();
	BeMessage("b: "),BeM(b),_delay_ms(100);
	LCD_Init();
	BeMessage("c: "),BeM(c),_delay_ms(100);
	LCD_Init();
	BeMessage("d: "),BeM(d),_delay_ms(100);
	LCD_Init();
	BeMessage("e: "),BeM(e),_delay_ms(100);
	LCD_Init();
	BeMessage("f: "),BeM(f),_delay_ms(100);
}
void Timer1_Init(int denominator) {
	TCCR1B |= (1 << WGM12); // Set CTC mode
	TCCR1B |= (1 << CS11) | (1 << CS10); // Set Prescaler to 64
	uint16_t compare_match_value = 124999 / denominator;
	OCR1A = compare_match_value; // Set Compare Match value for 1-second/denominator delay
	TIMSK |= (1 << OCIE1A); // Enable Timer1 Compare Match A interrupt
	sei(); // Enable global interrupts
}
void CheckTemperature(){
	uint16_t Current_NHC_Volt = ADC_Read(0);
	if (Current_NHC_Volt)
	PrintT = ((Current_NHC_Volt<250)); //Alert if NHC is heated (voltage drop over 250 counts);
}
void CheckPlayers()
{
	uint8_t sum=0;
	const char NPF[16]="NO PLAYER FOUND",OPO[16]="1 PLAYER ONLY",GBS[16]="Go to start";
	const char GBC[16]="GO BACK, Cheat!",FRWRD[16]="Forward!";
	for (int i=0;i<5;i++) sum+=(SensorsReadings[i]); //sum the number of players, Then choose alert to print if fault
	Door_Num=(Door_Num%6);
	if(CheckForPlayer) {
		if (sum==1) {
			
			if (SensorsReadings[Door_Num]) PrintP=false;
			else
			{
				PrintP=true;
				for (uint8_t i = 0;i<6;i++){
					if (SensorsReadings[i])
					{
						if (i<Door_Num&& Door_Num!=6) strcpy(PlayAlert,FRWRD);
						else strcpy(PlayAlert,GBC);
						break;
					}
				}
			}
		}
		else{
			if (!sum) strcpy(PlayAlert,NPF);
			else strcpy(PlayAlert,OPO);
			PrintP=true;
		}
	}
	else {
		if (sum) strcpy(PlayAlert,GBS),PrintP=true;
		else PrintP=false,CheckForPlayer=true;
	}
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
		PORTC= (PORTC&0xF8)|(Door_Num+1);
		sprintf(Door_str,"%d",Door_Num + 1);
		LCD_Init(),BeMessage("Door "),BeMessage(Door_str),_delay_ms(5); //display door number
		sprintf(Door_str,"%d",3-Tries);
		strcat(Door_str, " Tries Left");
		BeMode(0xC0),BeMessage(Door_str),_delay_ms(5); //display tries left
		if (CheckAnswer(Door_Num)) {
			(Door_Num==6)? open(6) : open(++Door_Num-1); // open the door // open the door
			Tries=0; //reset Tries to zero
			PORTC= (PORTC&0xF8) | (1+Door_Num); //Coloured LEDs in Binary
		}
		else {
			Tries++; //increment Tries
			LCD_Init();
			BeMessage("Wrong");
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
	//for (int i=0;i<50;i++)
	PORTB|=(1<<(door+6)),
	(set) ? _delay_ms(1) : _delay_ms(2),
	PORTB &= ~(1<<(door+6)),
	(set) ? _delay_ms(19) : _delay_ms(18);
}
void setD(uint8_t door,bool set){
	door+= (door<4) ? -2 : 2;
	//for (int i=0;i<50;i++)
	PORTD|=(1<<door),
	(set) ? _delay_ms(1) : _delay_ms(2),
	PORTD &= ~(1<<door),
	(set) ? _delay_ms(19) : _delay_ms(18);
}
void open(uint8_t Door){
	if (Door<2) setB(Door,true);
	else if (Door<6) setD(Door,true);
	else{
		setB(false,false),
		setB(true,false);
		for (uint8_t i =2;i<6;i++)
		setD(i,false);
	}
	Timer1_Init(20);
}
void BeM(char* str) {
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
void BeMessage(char* str) {
	for(int i=0;i<5;i++) if (PrintT) BeM("TEMP ALERT!!"),_delay_ms(15),LCD_Init(); //alert if any fault occurred
	while (PrintP) BeM(PlayAlert),_delay_ms(15),LCD_Init();
	BeM(str);
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