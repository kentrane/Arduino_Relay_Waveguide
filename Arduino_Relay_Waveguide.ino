#define switchtime 75 //Time to apply current to motor, enough time to switch, but also not too much so it will get hot
#define debounce 20
void pos(int pos);
uint8_t getpos(void);
void setupIO(void);
void set_position_output();
volatile uint8_t current_pos = 0;
volatile uint8_t command_pos_bounced = 0;
volatile uint8_t command_pos = 0;
uint8_t last_pos = 0;
uint8_t relaypin[4] = {4, 5, 6, 7}; //PD4, PD5, PD6, PD7
uint8_t pos_pins[2] = {A0,A1}; // PC0 and PC1
uint8_t trigger_input = 2; //D2 / PD2
uint8_t position_output = 8; //D8 / PB0
uint8_t interlock_pin = 9; //D9 / PB1
bool LED_STATE = true;
/*
PD7 - Relay 1
PD6 - Relay 2
PD5 - Relay 3
PD4 - Relay 4
*/
void setupTimer(void) {
  cli();
  /*1. First we reset the control register to amke sure we start with everything disabled.*/
  TCCR1A = 0;                 // Reset entire TCCR1A to 0 
  TCCR1B = 0;                 // Reset entire TCCR1B to 0
  /*2. We set the prescalar to the desired value by changing the CS10 CS12 and CS12 bits. */  
  TCCR1B |= B00000100;        //Set CS12 to 1 so we get prescalar 256  
  /*3. We enable compare match mode on register A*/
  TIMSK1 |= B00000010;        //Set OCIE1A to 1 so we enable compare match A 
  //OCR1A = 4167;             //Finally we set compare register A to this value  
  OCR1A = 4167;             //Finally we set compare register A to this value  
  sei();                     //Enable back the interrupts
}

void setupIO(void) {
  pinMode(relaypin[0],OUTPUT);
  pinMode(relaypin[1],OUTPUT);
  pinMode(relaypin[2],OUTPUT);
  pinMode(relaypin[3],OUTPUT);
  pinMode(pos_pins[0],INPUT); //pos 1 / "Safe" pos
  pinMode(pos_pins[1],INPUT); //pos 2 / tokamak pos
  pinMode(trigger_input, INPUT); 
  attachInterrupt(digitalPinToInterrupt(trigger_input),input_change,CHANGE);
  pinMode(position_output, OUTPUT); 
  pinMode(interlock_pin, INPUT);
  pinMode(13,OUTPUT);
}

void setup() {
  setupIO();
  setupTimer();
}

void loop() {
  //set_position_output(); //Sets the output pin to reflect the current measured position
  if((PIND & (1 << PIND2)) == command_pos_bounced){
    //If command signal is low, deliver power to tokamak
    if(command_pos_bounced){
      if(current_pos == 1 || current_pos == 0)
        pos(2);
    }

    //If signal is high go to safe position where power is delivered to load
    else if(!command_pos_bounced){
      if(current_pos == 2 || current_pos == 0)
        pos(1);
    }
  }
  else{
    command_pos = (PIND & (1 << PIND2));
    delay(50);
    if((PIND & (1 << PIND2)) == command_pos)
      command_pos_bounced = command_pos;
  }
}
void input_change(){
  command_pos = (PIND & (1 << PIND2));
  delay(50);
  if((PIND & (1 << PIND2)) == command_pos)
    command_pos_bounced = command_pos;
}
//Sets the pins for the relays to move waveguide
/*
PD7 - Relay 1
PD6 - Relay 2
PD5 - Relay 3
PD4 - Relay 4
*/
void pos(int pos)
{
  if(pos == 1) {
    PORTD &= ~0xC0; //off R1 and R2
    PORTD |= 0x30;  //on R3 and R4
    while(getpos() != 1); //Wait for movement to complete
    delay(75);
    PORTD &= ~0xF0; //off everything
  }

  else if(pos == 2) {
    PORTD &= ~0x30; //off R3 and R4
    PORTD |= 0xC0;  //on R1 and R2
    while(getpos() != 2); //Wait for movement to complete (TODO: Add routine to stop if it takes too long)
    delay(75);
    PORTD &= ~0xF0; //off everything
  }
}
//Get the position of the waveguide, can either be position 1, 2 or in between
uint8_t getpos(void){
  //Port C0 and Port C1
  uint8_t reading = (PINC & ((1 << PINC0) | (1 << PINC1))); //Make reading and store in memory

  if(! (reading & 0x3) ) //rotor is between the 2 positions
    return 0;
  if(reading & 1)
    return 1;
  if(reading & 2)
    return 2;
  else
    return 3;
}
void set_position_output(){
  current_pos = getpos();
  //for feedback
  if(current_pos == 2) // Set when in tokamak position
    PORTB |= (1 << PORTB0);
  else if(current_pos == 1) // Clear when in safe position
    PORTB &= ~(1 << PORTB0);
}
ISR(TIMER1_COMPA_vect){
  TCNT1  = 0;                  //First, set the timer back to 0 so it resets for next interrupt
  PORTB ^= 0x20; //toggle LED to let user know program is running
  set_position_output();
}