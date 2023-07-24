#define switchtime 75 //Time to apply current to motor, enough time to switch, but also not too much so it will get hot
#define debounce 20
#define POSPIN (PIND & (1 << PIND2)
typedef enum {
  load = 1,
  tokamak = 2,
  between = 3,
  error = 0,
  } position_t;
void pos(position_t pos);
position_t getpos(void);
void setupIO(void);
void set_position_output();
volatile position_t current_pos = 0;
volatile position_t command_pos_bounced = 0;
volatile position_t command_pos = 0;
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
  attachInterrupt(digitalPinToInterrupt(trigger_input),input_change,CHANGE); //Attach interrupt to enable immediate reaction to command signals
  pinMode(position_output, OUTPUT); 
  pinMode(interlock_pin, INPUT);
  pinMode(13,OUTPUT);
}

void setup() {
  setupIO();
  setupTimer();
}

void loop() {
  if(POSPIN == command_pos_bounced){
    //If command signal is high, deliver power to tokamak
    if(command_pos_bounced){
      if(current_pos == load || current_pos == between)
        pos(tokamak);
    }
    //If signal is low go to safe position where power is delivered to load
    else if(!command_pos_bounced){
      if(current_pos == tokamak || current_pos == between)
        pos(load);
    }
  }
  else{
    command_pos = POSPIN;
    delay(50);
    if((PIND & (1 << PIND2)) == command_pos)
      command_pos_bounced = command_pos;
  }
}

//Called when interrupt caused by external trigger signal change
void input_change(){
 command_pos_bounced = POSPIN; //Set new commanded position
}
//Sets the pins for the relays to move waveguide
/*
PD7 - Relay 1
PD6 - Relay 2
PD5 - Relay 3
PD4 - Relay 4
*/
void pos(position_t pos)
{
  if(pos == load) {
    if(getpos() != load){ //Only react if it's not already there
      PORTD &= ~0xC0; //off R1 and R2
      PORTD |= 0x30;  //on R3 and R4
      while(getpos() != 1); //Wait for movement to complete
      delay(switchtime); //Time after switch happens to still apply current, helps minimize bouncing of the contact
      PORTD &= ~0xF0; //off everything
    }
  }

  else if(pos == tokamak) {
    if(getpos() != tokamak){ //Only react if it's not already there
      PORTD &= ~0x30; //off R3 and R4
      PORTD |= 0xC0;  //on R1 and R2
      while(getpos() != 2); //Wait for movement to complete (TODO: Add routine to stop if it takes too long)
      delay(switchtime); //Time after switch happens to still apply current, helps minimize bouncing of the contact
      PORTD &= ~0xF0; //off everything
    }
  }
}
//Get the position of the waveguide, can either be position 1, 2 or in between
position_t getpos(void){
  //Port C0 and Port C1
  uint8_t reading = (PINC & ((1 << PINC0) | (1 << PINC1))); //Make reading and store in memory
  if(! (reading & 0x3) ) //rotor is between the 2 positions
    return between;
  if(reading & 1)
    return load;
  if(reading & 2)
    return between;
  else
    return error;
}

//TODO: Use 2 pins to also show when waveguide switch is between 2 posistions
void set_position_output(){
  current_pos = getpos();
  //for feedback
  if(current_pos == tokamak) // Set when in tokamak position
    PORTB |= (1 << PORTB0);
  else if(current_pos == load) // Clear when in safe position
    PORTB &= ~(1 << PORTB0);
}
ISR(TIMER1_COMPA_vect){
  TCNT1  = 0;                  //First, set the timer back to 0 so it resets for next interrupt
  PORTB ^= 0x20; //toggle LED to let user know program is running
  set_position_output();
}