#include <EEPROM.h>
#include "MelbusVariables.h"

#define INT_NUM (byte)1         //Interrupt number (0/1 on ATMega 328P)
#define MELBUS_CLOCKBIT (byte)3 //Pin D3  - CLK
#define MELBUS_DATA (byte)4     //Pin D4  - Data
#define MELBUS_BUSY (byte)5     //Pin D5  - Busy

const byte MISC = 8; // MISC pin that is high when HU is on, low when off.
// Currently unused.

byte track = 0x01; //Display show HEX value, not DEC. (A-F not "allowed")
byte cd = 0x01; //1-10 is allowed (in HEX. 0A-0F and 1A-1F is not allowed)

//volatile variables used inside AND outside of ISP
volatile byte melbus_ReceivedByte = 0;
volatile byte melbus_Bitposition = 7;
volatile bool byteIsRead = false;

byte byteToSend = 0;          //global to avoid unnecessary overhead
bool reqMasterFlag = false;   //set this to request master mode (and sendtext) at a proper time.


byte textHeader[] = {0xFC, 0xC6, 0x73, 0x01};
byte textRow = 2;
byte customText[4][36] = {
  {"1"},
  {"2"},
  {"3"},
  {"4"}
};

//HU asks for line 3 and 4 below at startup. They can be overwritten by customText if you change textRow to 3 or 4
byte textLine[4][36] = {
  {"1"},           //is overwritten by init-sequence ("Volvo!")
  {"2"},           //is overwritten by customText[1][]
  {"3"},    //changes if pressing 3-button
  {"4"}     //changes if pressing 4-button
};

// some CDC (CD-CHANGER) data
byte trackInfo[] = {0x00, 0x02, 0x00, cd, 0x80, track, 0xC7, 0x0A, 0x02}; //9 bytes
byte startByte = 0x08; //on powerup - change trackInfo[1] & [8] to this
byte stopByte = 0x02; //same on powerdown
byte cartridgeInfo[] = {0x00, 0xFC, 0xFF, 0x4A, 0xFC, 0xFF};

/* SETUP */
void setup() {
  //Disable timer0 interrupt. It's is only bogging down the system. We need speed!
  TIMSK0 &= ~_BV(TOIE0);

  //All lines are idle HIGH
  pinMode(MELBUS_DATA, INPUT_PULLUP);
  pinMode(MELBUS_CLOCKBIT, INPUT_PULLUP);
  pinMode(MELBUS_BUSY, INPUT_PULLUP);

  //Initiate serial communication to debug via serial-usb (arduino)
  //Better off without it.
  //Serial printing takes a lot of time!!
  Serial.begin(115200);
  Serial.println("Calling HU");

  //Activate interrupt on clock pin
  attachInterrupt(digitalPinToInterrupt(MELBUS_CLOCKBIT), MELBUS_CLOCK_INTERRUPT, RISING);

  //Call function that tells HU that we want to register a new device
  melbusInitReq();
}

/* MAIN LOOP */
void loop() {
  static byte lastByte = 0;     //used to copy volatile byte to register variable. See below
  static long runOnce = 300000;     //counts down on every received message from HU. Triggers when it is passing 1.
  static long runPeriodically = 1000;// 100000; //same as runOnce but resets after each countdown.
  static bool powerOn = true;
  static long HWTicks = 0;      //age since last BUSY switch
  static long ComTicks = 0;     //age since last received byte
  static long ConnTicks = 0;    //age since last message to SIRIUS SAT
  static long timeout = 1000000; //should be around 10-20 secs
  static byte matching[listLen];     //Keep track of every matching byte in the commands array

  //these variables are reset every loop
  byte byteCounter = 1;  //keep track of how many bytes is sent in current command
  byte melbus_log[99];  //main init sequence is 61 bytes long...
  bool BUSY = PIND & (1 << MELBUS_BUSY); // Get if busy line is high or low.

  HWTicks++;
  if (powerOn) {
    ComTicks++;
    ConnTicks++;
  } else {
    ComTicks = 1;
    ConnTicks = 1;
    //to avoid a lot of serial.prints when its 0
  }

  //check BUSY line active (active low)
  while (!BUSY) {
    HWTicks = 0;  //reset age

    //Transmission handling here...
    if (byteIsRead) {
      byteIsRead = false;
      lastByte = melbus_ReceivedByte; //copy volatile byte to register variable
      ComTicks = 0; //reset age
      melbus_log[byteCounter - 1] = lastByte;
      //Loop though every command in the array and check for matches. (No, don't go looking for matches now)
      for (byte cmd = 0; cmd < listLen; cmd++) {
        //check if this byte is matching
        if (lastByte == commands[cmd][byteCounter]) {
          matching[cmd]++;
          //check if a complete command is received, and take appropriate action
          if ((matching[cmd] == commands[cmd][0]) && (byteCounter == commands[cmd][0])) {
            byte cnt = 0;
            byte b1 = 0, b2 = 0;
            ConnTicks = 0;  //reset age
            // Serial.println(cmd);
            switch (cmd) {

              //0, MRB_1
              case 0:
                //wait for master_id and respond with same
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == MASTER_ID) {
                      byteToSend = MASTER_ID;
                      SendByteToMelbus();
                      SendText(textRow, "MRB1");
                      //Serial.println("MRB1");
                      break;
                    }
                  }
                }
                //Serial.println("MRB 1");
                break;

              //1, MAIN INIT
              case 1:
                //wait for base_id and respond with response_id
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == BASE_ID) {
                      byteToSend = RESPONSE_ID;
                      SendByteToMelbus();
                    }
                    if (melbus_ReceivedByte == CDC_BASE_ID) {
                      byteToSend = CDC_RESPONSE_ID;
                      SendByteToMelbus();
                    }
                  }
                }
                //Serial.println("main init");
                break;

              //2, Secondary INIT
              case 2:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == CDC_BASE_ID) {
                      byteToSend = CDC_RESPONSE_ID;
                      SendByteToMelbus();
                    }
                    if (melbus_ReceivedByte == BASE_ID) {
                      byteToSend = RESPONSE_ID;
                      SendByteToMelbus();
                    }
                  }
                }
                //Serial.println("2nd init");
                break;

              //CMD_1. answer 0x10
              case 3:
                // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    cnt++;
                  }
                  if (cnt == 2) {
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    break;
                  }
                }
                powerOn = true;
                //Serial.println("Cmd 1");
                break;


              //PUP. power on?
              case 4:
                // {0xC0, 0x1C, 0x70, 0x02} we respond 0x90;
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    cnt++;
                  }
                  if (cnt == 2) {
                    byteToSend = 0x90;
                    SendByteToMelbus();
                    break;
                  }
                }
                //Serial.println("power on");
                digitalWrite(MISC, HIGH);
                break;

              //MRB_2
              case 5:
                // {00 1E EC };
                //wait for master_id and respond with same
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    if (melbus_ReceivedByte == MASTER_ID) {
                      byteToSend = MASTER_ID;
                      SendByteToMelbus();
                      SendText(textRow, "MRB2");
                      //Serial.println("MRB2");
                      break;
                    }
                  }
                }
                Serial.println("MRB 2");
                break;

              // CMD_3,  // 6 unknown. Answer: 0x10, 0x80, 0x92
              case 6:
                byteToSend = 0x10;
                SendByteToMelbus();
                byteToSend = 0x80;
                SendByteToMelbus();
                byteToSend = 0x92;
                SendByteToMelbus();
                //Serial.println("cmd 3");
                break;

              //C1_1,    7 respond with c1_init_1
              case 7:
                for (byte i = 0; i < SO_C1_Init_1; i++) {
                  byteToSend = C1_Init_1[i];
                  SendByteToMelbus();
                }
                //Serial.println("C1 1");
                break;

              //C1_2,   8 respond with c1_init_2 (contains text)
              case 8:
                for (byte i = 0; i < SO_C1_Init_2; i++) {
                  byteToSend = C1_Init_2[i];
                  SendByteToMelbus();
                }
                //Serial.println("C1_2");
                break;

              //  C3_0,    9 respond with c3_init_0
              case 9:
                for (byte i = 0; i < SO_C3_Init_0; i++) {
                  byteToSend = C3_Init_0[i];
                  SendByteToMelbus();
                }
                //Serial.println("C3 init 0");
                break;

              //C3_1,    10 respond with c3_init_1
              case 10:
                for (byte i = 0; i < SO_C3_Init_1; i++) {
                  byteToSend = C3_Init_1[i];
                  SendByteToMelbus();
                }
                //Serial.println("C3 init 1");
                break;

              //C3_2,   11 respond with c3_init_2
              case 11:
                for (byte i = 0; i < SO_C3_Init_2; i++) {
                  byteToSend = C3_Init_2[i];
                  SendByteToMelbus();
                }
                //Serial.println("C3 init 2");
                break;

              //   C2_0,    12 get next byte (nn) and respond with 10, 0, nn, 0,0 and (14 times 0x20)
              // possibly a text field?
              case 12:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = melbus_ReceivedByte;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x20;
                    for (byte b = 0; b < 14; b++) {
                      SendByteToMelbus();
                    }
                    break;
                  }
                }
                //Serial.print("C2_0");
                //Serial.println(melbus_ReceivedByte, HEX);
                break;

              //C2_1,    13 respond as 12
              case 13:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    byteToSend = 0x10;
                    SendByteToMelbus();
                    byteToSend = 0x01;
                    SendByteToMelbus();
                    byteToSend = melbus_ReceivedByte;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x00;
                    SendByteToMelbus();
                    byteToSend = 0x20;
                    for (byte b = 0; b < 14; b++) {
                      SendByteToMelbus();
                    }
                    break;
                  }
                }
                //Serial.print("C2_1");
                //Serial.println(melbus_ReceivedByte, HEX);
                break;

              //C5_1
              case 14:
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b1 = melbus_ReceivedByte;
                    break;
                  }
                }
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b2 = melbus_ReceivedByte;
                    break;
                  }
                }
                byteToSend = 0x10;
                SendByteToMelbus();
                byteToSend = b1;
                SendByteToMelbus();
                byteToSend = b2;
                SendByteToMelbus();
                for (byte b = 0; b < 36; b++) {
                  byteToSend = textLine[b2 - 1][b];
                  SendByteToMelbus();
                }
                //Serial.print("C5_1");
                break;

              //BTN
              case 15:
                //wait for next byte to get CD number
                while (!(PIND & (1 << MELBUS_BUSY))) {
                  if (byteIsRead) {
                    byteIsRead = false;
                    b1 = melbus_ReceivedByte;
                    break;
                  }
                }
                byteToSend = 255; //0x00;  //no idea what to answer
                //but HU is displaying "no artist" for a short while when sending zeroes.
                //Guessing there should be a number here indicating that text will be sent soon.
                SendByteToMelbus();
                byteToSend = 255; //0x00;  //no idea what to answer
                SendByteToMelbus();
                byteToSend = 255; //0x00;  //no idea what to answer
                SendByteToMelbus();

                Serial.print("Pressed button: ");
                Serial.println(b1, HEX);

                switch (b1) {
                  //0x1 to 0x6 corresponds to cd buttons 1 to 6 on the HU (650) (SAT 1)
                  //7-13 on SAT 2, and 14-20 on SAT 3
                  //button 1 is always sent (once) when switching to SAT1.
                  case 0x1:
                    break;
                  case 0x2: //unfortunately the steering wheel button equals btn #2
                    break;
                  case 0x3:
                    //change LED color
                    break;
                  case 0x4:
                    //change LED color
                    break;
                  case 0x5:
                    //not used for the moment
                    break;
                  case 0x6:   //unfortunately the steering wheel button equals btn #6
                    break;
                }
                break;

              //NXT
              case 16:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                Serial.println("NXT");
                break;

              //PRV
              case 17:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                Serial.println("PRV");
                break;

              //SCN
              case 18:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                Serial.println("SCN");
                break;

              //PWR OFF
              case 19:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                Serial.println("Power down");
                powerOn = false;
                digitalWrite(MISC, LOW);
                break;

              //PWR SBY
              case 20:
                byteToSend = 0x00;  //no idea what to answer
                SendByteToMelbus();
                Serial.println("Power stby");
                powerOn = false;
                break;

              //IGN_OFF
              case 21:
                Serial.println("Ignition OFF");
                powerOn = false;
                break;

              //
              case 22:
                SendCartridgeInfo();
                break;

              //
              case 23:
                SendTrackInfo();
                break;

              //
              case 24:
                track++;
                fixTrack();
                trackInfo[5] = track;
                Serial.println("Track++");
                break;

              //
              case 25:
                track--;
                fixTrack();
                trackInfo[5] = track;
                Serial.println("Track--");
                break;

              //
              case 26:
                changeCD();
                break;

              //CDC_PUP
              case 27:
                byteToSend = 0x00;
                SendByteToMelbus();
                trackInfo[1] = startByte;
                trackInfo[8] = startByte;
                digitalWrite(MISC, HIGH);
                Serial.println("CDC_PUP");
                break;

              //CDC_PDN
              case 28:
                byteToSend = 0x00;
                SendByteToMelbus();
                trackInfo[1] = stopByte;
                trackInfo[8] = stopByte;
                digitalWrite(MISC, LOW);
                Serial.println("CDC_PDN");
                break;

              //CDC_FFW
              case 29:
                byteToSend = 0x00;
                SendByteToMelbus();
                Serial.println("CDC_FFW");
                break;

              //CDC_FRW
              case 30:
                byteToSend = 0x00;
                SendByteToMelbus();
                Serial.println("CDC_FRW");
                break;

              //CDC_SCN
              case 31:
                byteToSend = 0x00;
                SendByteToMelbus();
                Serial.println("CDC_SCN");
                break;

              //CDC_RND
              case 32:
                byteToSend = 0x00;
                SendByteToMelbus();
                Serial.println("CDC_RND");
                break;

              //CDC_NU
              case 33:
                Serial.println("CDC_NU");
                break;
            } //end switch

            Serial.println(cmd);
            break;    //bail for loop. (Not meaningful to search more commands if one is already found)

          } //end if command found

        } //end if lastbyte matches

      }  //end for cmd loop
      byteCounter++;

    }  //end if byteisread
    
    //Update status of BUSY line, so we don't end up in an infinite while-loop.
    BUSY = PIND & (1 << MELBUS_BUSY);
  }

  //Do other stuff here if you want. MELBUS lines are free now. BUSY = IDLE (HIGH)
  //Don't take too much time though, since BUSY might go active anytime, and then we'd better be ready to receive.

  //Printing transmission log (from HU, excluding our answer and things after it)
  //if (ComTicks == 0) {                    //print all messages
  if (ComTicks == 0 && ConnTicks != 0) {    //print unmatched messages (unknown)
    Serial.print("Log of unmatched bytes: ");
    for (byte b = 0; b < byteCounter - 1; b++) {
      Serial.print(melbus_log[b], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  //runOnce is counting down to zero and stays there
  //after that, runPeriodically is counting down over and over...
  if (runOnce >= 1) {
    runOnce--;
  } else if (runPeriodically > 0) runPeriodically--;


  //check if BUSY-line is alive
  if (HWTicks > timeout) {
    Serial.println("BUSY line problem");
    HWTicks = 0;
    ComTicks = 0;     //to avoid several init requests at once
    ConnTicks = 0;    //to avoid several init requests at once
    //while (1);      //maybe do a reset here, after a delay.
    melbusInitReq();  //worth a try...
  }

  //check if we are receiving any data
  if (ComTicks > timeout) {
    Serial.println("COM failure (check CLK line)");
    ComTicks = 0;
    ConnTicks = 0;    //to avoid several init requests at once
    melbusInitReq();  //what else to do...
  }

  //check if HU is talking to us specifically, otherwise force it.
  if (ConnTicks > timeout) {
    Serial.println("Lost connection. Re-initializing");
    ConnTicks = 0;
    melbusInitReq();
  }


  //Reset stuff
  melbus_Bitposition = 7;
  for (byte i = 0; i < listLen; i++) {
    matching[i] = 0;
  }

  //initiate MRB2 to send text (from one of the four textlines in customText[textRow][])
  if ((runOnce == 1) || reqMasterFlag) {
    delayMicroseconds(200); 
    //cycleRight() did not get reqMaster() to initiate a MRB2 to happen.
    //All other functions that was identical worked.
    //That's super weird, but now it works thanks to the delay above.
    reqMaster();
    reqMasterFlag = false;
  }

 if (runPeriodically == 0) {
  Serial.println("run period");
   // String message = "BAT: V" + '\0';
   runPeriodically = 100000;
   
   // textRow = 2;
   // message.getBytes(customText[textRow - 1], 36);
   // SendText(0);
   // SendText(1);
   // SendText(2);
   // SendText(3);
   reqMaster();
   SendText(0, "Testing...");
 }
}


/* END LOOP */

//Notify HU that we want to trigger the first initiate procedure to add a new device
//(CD-CHGR/SAT etc) by pulling BUSY line low for 1s
void melbusInitReq() {
  //Serial.println("conn");
  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);

  // Wait until Busy-line goes high (not busy) before we pull BUSY low to request init
  while (digitalRead(MELBUS_BUSY) == LOW) {}
  delayMicroseconds(20);

  pinMode(MELBUS_BUSY, OUTPUT);
  digitalWrite(MELBUS_BUSY, LOW);
  //timer0 is off so we have to do a trick here
  for (unsigned int i = 0; i < 12000; i++) delayMicroseconds(100);

  digitalWrite(MELBUS_BUSY, HIGH);
  pinMode(MELBUS_BUSY, INPUT_PULLUP);
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);
}

//This is a function that sends a byte to the HU - (not using interrupts)
//SET byteToSend variable before calling this!!
void SendByteToMelbus() {
  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);

  //Convert datapin to output
  //pinMode(MELBUS_DATA, OUTPUT); //To slow, use DDRD instead:
  DDRD |= (1 << MELBUS_DATA);

  //For each bit in the byte
  for (char i = 7; i >= 0; i--)
  {
    while (PIND & (1 << MELBUS_CLOCKBIT)) {} //wait for low clock
    //If bit [i] is "1" - make datapin high
    if (byteToSend & (1 << i)) {
      PORTD |= (1 << MELBUS_DATA);
    }
    //if bit [i] is "0" - make datapin low
    else {
      PORTD &= ~(1 << MELBUS_DATA);
    }
    while (!(PIND & (1 << MELBUS_CLOCKBIT))) {}  //wait for high clock
  }
  //Let the value be read by the HU
  delayMicroseconds(20);
  //Reset datapin to high and return it to an input
  //pinMode(MELBUS_DATA, INPUT_PULLUP);
  PORTD |= 1 << MELBUS_DATA;
  DDRD &= ~(1 << MELBUS_DATA);

  //We have triggered the interrupt but we don't want to read any bits, so clear the flag
  EIFR |= 1 << INT_NUM;
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);
}

//This method generates our own clock. Used when in master mode.
void SendByteToMelbus2() {
  delayMicroseconds(700);
  //For each bit in the byte
  //char, since it will go negative. byte 0..255, char -128..127
  //int takes more clockcycles to update on a 8-bit CPU.
  for (char i = 7; i >= 0; i--)
  {
    delayMicroseconds(7);
    PORTD &= ~(1 << MELBUS_CLOCKBIT);  //clock -> low
    //If bit [i] is "1" - make datapin high
    if (byteToSend & (1 << i)) {
      PORTD |= (1 << MELBUS_DATA);
    }
    //if bit [i] is "0" - make datapin low
    else {
      PORTD &= ~(1 << MELBUS_DATA);
    }
    //wait for output to settle
    delayMicroseconds(5);
    PORTD |= (1 << MELBUS_CLOCKBIT);   //clock -> high
    //wait for HU to read the bit
  }
  delayMicroseconds(20);
}

//Global external interrupt that triggers when clock pin goes high after it has been low for a short time => time to read datapin
void MELBUS_CLOCK_INTERRUPT() {
  //Read status of Datapin and set status of current bit in recv_byte
  //if (digitalRead(MELBUS_DATA) == HIGH) {
  if ((PIND & (1 << MELBUS_DATA))) {
    melbus_ReceivedByte |= (1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "1"
  }
  else {
    melbus_ReceivedByte &= ~(1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "0"
  }

  //if all the bits in the byte are read:
  if (melbus_Bitposition == 0) {
    //set bool to true to evaluate the bytes in main loop
    byteIsRead = true;

    //Reset bitcount to first bit in byte
    melbus_Bitposition = 7;
  } else {
    //set bitnumber to address of next bit in byte
    melbus_Bitposition--;
  }
}

void SendText(byte rowNum, String text) {
  byte bytesToSend[36];
  text.getBytes(bytesToSend, 36);

  //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
  EIMSK &= ~(1 << INT_NUM);

  //Convert datapin and clockpin to output
  //pinMode(MELBUS_DATA, OUTPUT); //To slow, use DDRD instead:
  PORTD |= 1 << MELBUS_DATA;      //set DATA input_pullup => HIGH when output (idle)
  DDRD |= (1 << MELBUS_DATA);     //set DATA as output
  PORTD |= 1 << MELBUS_CLOCKBIT;  //set CLK input_pullup => HIGH when output (idle)
  DDRD |= (1 << MELBUS_CLOCKBIT); //set CLK as output

  //send header
  for (byte b = 0; b < 4; b++) {
    byteToSend = textHeader[b];
    SendByteToMelbus2();
  }

  //send which row to show it on
  byteToSend = rowNum;
  SendByteToMelbus2();

  //send text
  for (byte b = 0; b < 36; b++) {
    byteToSend = bytesToSend[b];
    SendByteToMelbus2();
  }

  DDRD &= ~(1 << MELBUS_CLOCKBIT);//back to input (PULLUP since we clocked in the last bit with clk = high)
  //Reset datapin to high and return it to an input
  //pinMode(MELBUS_DATA, INPUT_PULLUP);
  PORTD |= 1 << MELBUS_DATA;  //this may or may not change the state, depending on the last transmitted bit
  DDRD &= ~(1 << MELBUS_DATA);//release the data line

  //Clear INT flag
  EIFR |= 1 << INT_NUM;
  //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
  EIMSK |= (1 << INT_NUM);

  for (byte b = 0; b < 36; b++) {
    Serial.print(char(bytesToSend[b]));
  }
  Serial.println("  END");
}

void reqMaster() {
  DDRD |= (1 << MELBUS_DATA); //output
  PORTD &= ~(1 << MELBUS_DATA);//low
  delayMicroseconds(700);
  delayMicroseconds(800);
  delayMicroseconds(800);
  PORTD |= (1 << MELBUS_DATA);//high
  DDRD &= ~(1 << MELBUS_DATA); //back to input_pullup
}

void fixTrack() {
  //cut out A-F in each nibble, and skip "00"
  byte hn = track >> 4;
  byte ln = track & 0xF;
  if (ln == 0xA) {
    ln = 0;
    hn += 1;
  }
  if (ln == 0xF) {
    ln = 9;
  }
  if (hn == 0xA) {
    hn = 0;
    ln = 1;
  }
  if ((hn == 0) && (ln == 0)) {
    ln = 0x9;
    hn = 0x9;
  }
  track = (hn << 4) + ln;
}

void changeCD() {
  byte b = 0x00;
  while (!(PIND & (1 << MELBUS_BUSY))) {
    if (byteIsRead) {
      byteIsRead = false;
      b=melbus_ReceivedByte;
      switch (b) {
        //0x81 to 0x86 corresponds to cd buttons 1 to 6 on the HU (650)
        case 0x81:
          cd = 1;
          track = 1;
          break;
        case 0x82:
          cd = 2;
          track = 1;
          break;
        case 0x83:
          cd = 3;
          track = 1;
          break;
        case 0x84:
          cd = 4;
          track = 1;
          break;
        case 0x85:
          cd = 5;
          track = 1;
          break;
        case 0x86:
          cd = 6;
          track = 1;
          break;
        case 0x41:  //next cd
          cd++;
          track = 1;
          break;
        case 0x01:  //prev cd
          cd--;
          track = 1;
          break;
        default:
          track = 1;
          break;
      }
      Serial.print("Change cd:");
      Serial.println(b, HEX);
    }
  }
  
  cd = 1;
  track = 1;
  trackInfo[3] = cd;
  trackInfo[5] = track;
}

void SendTrackInfo() {
  for (byte i = 0; i < 9; i++) {
    byteToSend = trackInfo[i];
    SendByteToMelbus();
    delayMicroseconds(10);
  }
}

void SendCartridgeInfo() {
  for (byte i = 0; i < 6; i++) {
    byteToSend = cartridgeInfo[i];
    SendByteToMelbus();
    delayMicroseconds(10);
  }
}

//Happy listening AND READING, hacker!
