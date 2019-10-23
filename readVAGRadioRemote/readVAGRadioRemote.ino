
/*

  license: GPL

  emulator for audi radios (probably also for vw, skoda, seat)
  using ebay LCD shield(2x16) with 5 buttons on A0
  output (remote signal) is on pin 2

  more info:
  http://kovo-blog.blogspot.sk/2013/10/remote-control-audi-radio.html

  5V logic
  start bit:
  9ms LOW
  4.55ms HIGH

  logic 1:
  ~600us LOW ~1700us HIGH

  logic 0:
  ~600us LOW ~600us HIGH

  stop bit:
  ~600us LOW

  I have MFSW without telephone option, so I read codes just for radio:

  UP:       0x41E8D02F
  DOWN: 0x41E850AF
  LEFT:   0x41E840BF
  RIGHT: 0x41E8C03F
  VOL+:  0x41E8807F
  VOL-:    0x41E800FF

  code is always 0x41 0xE8 X 0xFF-X
*/

#define REMOTE_IN 2

volatile uint16_t captime = 0;
volatile uint8_t captureEnabled = 0;
volatile uint8_t capbit = 0;
volatile uint8_t capbyte[4];
volatile uint8_t capptr = 0;

void remoteInGoingHigh();
void remoteInGoingLow();
String decodeRemote(uint8_t code);

void setup() {

  TIMSK2 &= ~_BV(OCIE2A); // disable further timer 2 interrupts
  TCCR2A = TCCR2B = 0; // reset
  TCCR2A |= _BV(WGM21); //CTC mode ,WGM20 and WGM22 are cleared in prev. step
  TCCR2B |= _BV(CS21);
  TCCR2B |= _BV(CS20); // prescaler = 32 -> 1 timer clock tick is 2us long
  OCR2A = 5; //compare run each 20us (10x2us)
  TCNT2 = 0; //just to be sure

  Serial.begin(115200);
  pinMode(REMOTE_IN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REMOTE_IN), remoteInGoingLow, FALLING);
}

void loop() {
  if (capptr == 4) {
/*
    Serial.println(capbyte[0], HEX);
    Serial.println(capbyte[1], HEX);
    Serial.println(capbyte[2], HEX);
    Serial.println(capbyte[3], HEX);
*/
    if (capbyte[0] == 0x41 && capbyte[1] == 0xE8 && capbyte[2] == 0xFF - capbyte[3]) {
      Serial.println(decodeRemote(capbyte[2]));
    }
    capptr = 0;
  }
}

ISR(TIMER2_COMPA_vect)
{
  if (captureEnabled) {
    captime++;
  }

  if (captime == 600 ) { //300x20us =>  6ms high pulse,
    TIMSK2 &= ~_BV(OCIE2A); // disable further timer 1 interrupts
    captime = capptr = captureEnabled = 0;
    attachInterrupt(digitalPinToInterrupt(REMOTE_IN), remoteInGoingLow, FALLING);
  }

}

void remoteInGoingHigh() {
  //Serial.println("H");
  captime = 0; //reset timer
  if (capptr < 4) {
    captureEnabled = 1; //we capturing only when capture pointer is less then 4th byte.
    TCNT2 = 0;
    TIMSK2 |= _BV(OCIE2A); // enable timer 2 interrupts, but only if we did not already captured whole packet.
  }
  attachInterrupt(digitalPinToInterrupt(REMOTE_IN), remoteInGoingLow, FALLING);
}

void remoteInGoingLow() {
  //Serial.println("L");
  TIMSK2 &= ~_BV(OCIE2A); // disable further timer 2 interrupts
  if (captureEnabled) {
    //we have ticked in some data, lets calculate what we capture
    captureEnabled = 0; //disable future counting in timer2
    if (captime > 200) {//start bit 4.55ms
      capbit = capptr = 0; //start of packet, lets reset it all to begining
      capbyte[0] = capbyte[1] = capbyte[2] = capbyte[3] = 0;
    } else {
      if (captime > 25) { //logic 0 is 600us pulse
        capbyte[capptr] <<= 1;
      }
      if (captime > 70) {
        capbyte[capptr] |= 1;
      }
      capbit++;
    }

    if (capbit == 8) { //we got whole byte, lets
      capptr++;
      capbit = 0;
    }
  }  else { //1st drop to zero, 9ms long pulse,
    captureEnabled = 1; //do not reset any other variables, these are reseted when start condition is met
  }
  attachInterrupt(digitalPinToInterrupt(2), remoteInGoingHigh, RISING);
}

String decodeRemote(uint8_t code) {
  /*
       UP:       0x41E8D02F
    DOWN: 0x41E850AF
    LEFT:   0x41E840BF
    RIGHT: 0x41E8C03F
    VOL+:  0x41E8807F
    VOL-:    0x41E800FF
  */
  switch (code) {
    case 0xD0: return "UP";
    case 0x50: return "DOWN";
    case 0x40: return "LEFT";
    case 0xC0: return "RIGHT";
    case 0x80: return "VOL+";
    case 0x00: return "VOL-";
  }
  return "unknown";
}
