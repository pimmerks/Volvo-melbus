#pragma region Pin definition
#define INT_NUM (byte)1      // Interrupt number - seems like this should be 1 on nano with clockpin == 3.
#define MELBUS_CLOCK (byte)3 // Pin D3 - CLK
#define MELBUS_DATA (byte)4  // Pin D4 - Data
#define MELBUS_BUSY (byte)5  // Pin D5 - Busy
#pragma endregion

#include <stdio.h>
#include "MelbusVariables.h"
#include "ArduinoPinFunctions.h"

volatile byte melbus_ReceivedByte = 0;
volatile byte melbus_Bitposition = 7;
volatile bool byteIsRead = false;

#define MINIMUM_BYTES_RECEIVED_BEFORE_FINDING_MATCH 3

byte cd = 1;
byte track = 1;

byte trackInfo[] = {0x00, 0x02, 0x00, cd, 0x80, track, 0xC7, 0x0A, 0x02}; //9 bytes
byte startByte = 0x08; //on powerup - change trackInfo[1] & [8] to this
byte stopByte = 0x02; //same on powerdown
byte cartridgeInfo[] = {0x00, 0xFC, 0xFF, 0x4A, 0xFC, 0xFF};

void setup()
{
    //Disable timer0 interrupt. It's is only bogging down the system. We need speed!
    DISABLE_TIMER0_INTERRUPT;

    //All lines are idle HIGH
    pinMode(MELBUS_DATA, INPUT_PULLUP);
    pinMode(MELBUS_CLOCK, INPUT_PULLUP);
    pinMode(MELBUS_BUSY, INPUT_PULLUP);

    //Initiate serial communication to debug via serial-usb (arduino)
    //Better off without it.
    //Serial printing takes a lot of time!!
    Serial.begin(115200);
    Serial.println("Calling HU");

    //Activate interrupt on clock pin
    attachInterrupt(digitalPinToInterrupt(MELBUS_CLOCK), MELBUS_CLOCK_INTERRUPT, RISING);

    //Call function that tells HU that we want to register a new device
    melbusInitReq();
}

void loop()
{
    // Variables that arent reset every loop.
    static byte lastByte = 0; //used to copy volatile byte to register variable. See below

    // Variables that are reset every loop.
    byte byteCounter = 0;   //keep track of how many bytes is sent in current command
    byte receivedBytes[99]; // Keep track of received bytes.

    byte matchIndex = -1;

    bool isBusy = !READ_BUSYPIN; // Get if busy line is high or low (active low).

    while (isBusy)
    {

        if (byteIsRead)
        {
            byteIsRead = false;
            lastByte = melbus_ReceivedByte; //copy volatile byte to register variable
            receivedBytes[byteCounter] = lastByte;

            if (byteCounter >= MINIMUM_BYTES_RECEIVED_BEFORE_FINDING_MATCH - 1)
            {
                // Now we have received a minimum amount of bytes so we can start looking...

                // Lets see if we can match any commands here...
                matchIndex = findMatch(receivedBytes, byteCounter);
                if (matchIndex != -1)
                {
                    // We found a match so lets break our while loop...
                    respondToMatch(matchIndex);
                    break;
                }
                // Else no match so we should wait for a new byte...
            }

            // 1-up our byte counter
            byteCounter++;
        } // Else do nothing now. We are waiting for byteIsRead to be true, which is updated in our interrupt.

        isBusy = !READ_BUSYPIN;
    }

    // Reset
    melbus_Bitposition = 7;

    Serial.print("Received bytes: ");
    printBytes(receivedBytes, byteCounter);
}

// Tries to find a match in commands list.
byte findMatch(byte receivedBytes[], byte len)
{
    // Loop through all commands.
    for (byte cmd = 0; cmd < listLen; cmd++)
    {
        bool isMatch = false;
        byte amountOfMatchingBytes = 0;
        bool goToNext = false;

        // Loop through all bytes
        for (byte pos = 0; pos < len; pos++)
        {
            // If byte matches...
            if (receivedBytes[pos] == commands[cmd][pos + 1])
            {
                amountOfMatchingBytes++;
            }
            else
            {
                // If there is no match, skip this and continue to next one.
                goToNext = true;
                break;
            }

            if (amountOfMatchingBytes == commands[cmd][0])
            {
                isMatch = true;
                break;
            }
        }

        if (goToNext)
        {
            continue;
        }

        if (isMatch)
        {
            return cmd;
        }
    }

    return -1;
}

// Responds to a command.
void respondToMatch(byte matchIndex)
{
    byte cnt = 0;
    byte b1 = 0;
    byte b2 = 0;

    switch (matchIndex)
    {
    case 0: // MRB_1
        //wait for master_id and respond with same
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                if (melbus_ReceivedByte == MASTER_ID)
                {
                    sendByteToMelbus(MASTER_ID);
                    // I think here we can send text to display.
                    break;
                }
            }
        }
        break;

    case 1: // Main init
        // Wait for base_id and respond with response_id
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                if (melbus_ReceivedByte == BASE_ID)
                {
                    sendByteToMelbus(RESPONSE_ID);
                }
                else if (melbus_ReceivedByte == CDC_BASE_ID)
                {
                    sendByteToMelbus(CDC_RESPONSE_ID);
                }
            }
        }
        break;

    case 2: // Secondary init
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                if (melbus_ReceivedByte == CDC_BASE_ID)
                {
                    sendByteToMelbus(CDC_RESPONSE_ID);
                }
                else if (melbus_ReceivedByte == BASE_ID)
                {
                    sendByteToMelbus(RESPONSE_ID);
                }
            }
        }
        break;
    case 3: // CMD_1, answer with 0x10 i guess?
        
        while (!READ_BUSYPIN)
        {
            // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
            if (byteIsRead)
            {
                byteIsRead = false;
                cnt++;
            }
            if (cnt == 2)
            {
                sendByteToMelbus(0x10);
                break;
            }
        }
        break;
    case 4: // PUP. Power on?
        // {0xC0, 0x1C, 0x70, 0x02} we respond 0x90;
        while (!READ_BUSYPIN)
        {
            // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
            if (byteIsRead)
            {
                byteIsRead = false;
                cnt++;
            }
            if (cnt == 2)
            {
                sendByteToMelbus(0x90);
                break;
            }
        }
        break;
    case 5: // MRB_2
        // {00 1E EC };
        //wait for master_id and respond with same
        while (!READ_BUSYPIN)
        {
            // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
            if (byteIsRead)
            {
                byteIsRead = false;
                if (melbus_ReceivedByte == MASTER_ID)
                {
                    sendByteToMelbus(MASTER_ID);
                    break;
                }
            }
        }
        break;
    case 6: // CMD_3
        sendByteToMelbus(0x10);
        sendByteToMelbus(0x80);
        sendByteToMelbus(0x92);
        break;
    case 7: // C1_1
        for (byte i = 0; i < SO_C1_Init_1; i++)
        {
            sendByteToMelbus(C1_Init_1[i]);
        }
        break;

    case 8: // C1_2, 8 respond with c1_init_2 (contains text)
        for (byte i = 0; i < SO_C1_Init_2; i++)
        {
            sendByteToMelbus(C1_Init_2[i]);
        }
        break;

    case 9: // C3_0, 9 respond with c3_init_0
        for (byte i = 0; i < SO_C3_Init_0; i++)
        {
            sendByteToMelbus(C3_Init_0[i]);
        }
        break;

    case 10: // C3_1, 10 respond with c3_init_1
        for (byte i = 0; i < SO_C3_Init_1; i++)
        {
            sendByteToMelbus(C3_Init_1[i]);
        }
        break;

    case 11: // C3_2, 11 respond with c3_init_2
        for (byte i = 0; i < SO_C3_Init_2; i++)
        {
            sendByteToMelbus(C3_Init_2[i]);
        }
        break;

    case 12: // C2_0, 12 get next byte (nn) and respond with 10, 0, nn, 0,0 and (14 times 0x20) possibly a text field?
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                sendByteToMelbus(0x10);
                sendByteToMelbus(0x00);
                sendByteToMelbus(melbus_ReceivedByte);
                sendByteToMelbus(0x00);
                sendByteToMelbus(0x00);
                for (byte b = 0; b < 14; b++)
                {
                    sendByteToMelbus(0x20);
                }
                break;
            }
        }
        break;

    case 13: //C2_1, 13 respond the same as 12
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                sendByteToMelbus(0x10);
                sendByteToMelbus(0x01);
                sendByteToMelbus(melbus_ReceivedByte);
                sendByteToMelbus(0x00);
                sendByteToMelbus(0x00);
                for (byte b = 0; b < 14; b++)
                {
                    sendByteToMelbus(0x20);
                }
                break;
            }
        }
        //Serial.print("C2_1");
        //Serial.println(melbus_ReceivedByte, HEX);
        break;

    //C5_1
    case 14:
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                b1 = melbus_ReceivedByte;
                break;
            }
        }
        while (!(PIND & (1 << MELBUS_BUSY)))
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                b2 = melbus_ReceivedByte;
                break;
            }
        }
        sendByteToMelbus(0x10);
        sendByteToMelbus(b1);
        sendByteToMelbus(b2);
        for (byte b = 0; b < 36; b++)
        {
            // Can we send text here? dunno yet...
            sendByteToMelbus('a');
        }

        char o[32];
        sprintf(o, "C5_1 %2x %2x", b1, b2);
        Serial.println(o);
        break;
    case 15: // BTN, here we receive a button press :D
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
                byteIsRead = false;
                b1 = melbus_ReceivedByte;
                break;
            }
        }

        //but HU is displaying "no artist" for a short while when sending zeroes.
        //Guessing there should be a number here indicating that text will be sent soon.
        sendByteToMelbus(0x00); //no idea what to answer
        sendByteToMelbus(0x00); //no idea what to answer
        sendByteToMelbus(0x00); //no idea what to answer
        
        Serial.print("Pressed button: ");
        Serial.println(b1, HEX);

        break;

    // CDC_CIR - Requesting cartridge info...
    case 22:
        SendCartridgeInfo();
        break;

    // CDC_TIR - Requesting track info, this happens around once every second...
    case 23:
        SendTrackInfo();
        break;

    case 24: // CDC_NXT Next Track
        Serial.println("Track++");
        break;

    case 25: // CDC_PRV Prv Track
        Serial.println("Track--");
        break;
    
    case 26: // Change CD
        byte b = 0x00;
        while (!READ_BUSYPIN) {
            if (byteIsRead) {
                byteIsRead = false;
                b = melbus_ReceivedByte;
            }
        }

        // We can do something here?
        Serial.print("Change cd:");
        Serial.println(b, HEX);
        break;
    case 27: // CDC_PUP, Respond ACK 0x00
        sendByteToMelbus(0x00);
        trackInfo[1] = startByte;
        trackInfo[8] = startByte;
        break;
    case 28: // CDC_PDN, Respond ACK 0x00
        sendByteToMelbus(0x00);
        trackInfo[1] = stopByte;
        trackInfo[8] = stopByte;
        break;
    default:
        sendByteToMelbus(0x00);
        Serial.println("def: 0x00");
        break;
    }

    Serial.print("Responded to match: ");
    Serial.println(matchIndex, DEC);
}

//Notify HU that we want to trigger the first initiate procedure to add a new device by pulling BUSY line low for 1s
void melbusInitReq()
{
    //Serial.println("conn");
    DISABLE_CLOCK_INTERRUPT;

    // Wait until Busy-line goes high (not busy) before we pull BUSY low to request init
    while (!READ_DATAPIN)
    {
    }
    delayMicroseconds(20);

    SET_BUSYPIN_AS_OUTPUT;
    SET_BUSYPIN_LOW;

    //timer0 is off so we have to do a trick here
    for (unsigned int i = 0; i < 12000; i++)
    {
        delayMicroseconds(100);
    }

    SET_BUSYPIN_HIGH;
    SET_BUSYPIN_AS_INPUT;

    //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
    ENABLE_CLOCK_INTERRUPT;
}

//Global external interrupt that triggers when clock pin goes high after it has been low => time to read datapin
void MELBUS_CLOCK_INTERRUPT()
{
    //Read status of Datapin and set status of current bit in recv_byte
    if (READ_DATAPIN)
    {
        melbus_ReceivedByte |= (1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "1"
    }
    else
    {
        melbus_ReceivedByte &= ~(1 << melbus_Bitposition); //set bit nr [melbus_Bitposition] to "0"
    }

    // If all the bits in the byte are read:
    if (melbus_Bitposition == 0)
    {
        //set bool to true to evaluate the bytes in main loop
        byteIsRead = true;

        //Reset bitcount to first bit in byte
        melbus_Bitposition = 7;
    }
    else
    {
        //set bitnumber to address of next bit in byte
        melbus_Bitposition--;
    }
}

//This is a function that sends a byte to the HU - (not using interrupts)
void sendByteToMelbus(byte byteToSend)
{
    DISABLE_CLOCK_INTERRUPT;
    SET_DATAPIN_AS_OUTPUT;

    //For each bit in the byte
    for (char i = 7; i >= 0; i--)
    {
        while (READ_CLOCKPIN)
        {
        }

        if (byteToSend & (1 << i))
        {
            SET_DATAPIN_HIGH;
        }
        else
        {
            SET_DATAPIN_LOW;
        }

        while (!(READ_CLOCKPIN))
        {
        } // Wait for high clock
    }
    //Let the value be read by the HU
    delayMicroseconds(20);
    //Reset datapin to high and return it to an input
    //pinMode(MELBUS_DATA, INPUT_PULLUP);
    SET_DATAPIN_HIGH;
    SET_DATAPIN_AS_INPUT;

    //We have triggered the interrupt but we don't want to read any bits, so clear the flag
    CLEAR_CLOCK_INTERRUPT_FLAG;

    //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
    ENABLE_CLOCK_INTERRUPT;
}

void printBytes(byte bytes[], byte len)
{
    for (byte b = 0; b < len; b++)
    {
        Serial.print(bytes[b], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void SendTrackInfo() {
  for (byte i = 0; i < 9; i++) {
    sendByteToMelbus(trackInfo[i]);
    // delayMicroseconds(10);
  }
}

void SendCartridgeInfo() {
  for (byte i = 0; i < 6; i++) {
    sendByteToMelbus(cartridgeInfo[i]);
    // delayMicroseconds(10);
  }
}