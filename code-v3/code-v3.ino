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
byte startByte = 0x08;                                                    //on powerup - change trackInfo[1] & [8] to this
byte stopByte = 0x02;                                                     //same on powerdown
byte cartridgeInfo[] = {0x00, 0xFC, 0xFF, 0x4A, 0xFC, 0xFF};

bool reqMasterFlag = false; //set this to request master mode (and sendtext) at a proper time.

byte textHeader[] = {0xFC, 0xC6, 0x73, 0x01};
byte textRow = 2;
byte customText[4][36] = {
    {"1"},
    {"2"},
    {"3"},
    {"4"}};

//HU asks for line 3 and 4 below at startup. They can be overwritten by customText if you change textRow to 3 or 4
byte textLine[4][36] = {
    {"1"},
    {"2"},
    {"3"},
    {"4"}
};

volatile bool isCommunicating = false;

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

    byte matchIndex = 0xFF; // Default for no match found...

    bool isBusy = !READ_BUSYPIN; // Get if busy line is high or low (active low).

    while (isBusy)
    {
        if (byteIsRead)
        {
            byteIsRead = false;
            lastByte = melbus_ReceivedByte; //copy volatile byte to register variable
            receivedBytes[byteCounter] = lastByte;

            // 1-up our byte counter so we can insert the next one
            byteCounter++;
            // Serial.print(melbus_ReceivedByte, HEX);
        }

        // We should do init here
        if (!isCommunicating){

        //if (byteCounter >= MINIMUM_BYTES_RECEIVED_BEFORE_FINDING_MATCH)
        //{
            // Now we have received a minimum amount of bytes so we can start looking...

            // Lets see if we can match any commands here...
            matchIndex = findMatch(receivedBytes, byteCounter);
            if (matchIndex != 0xFF)
            {
                isCommunicating = true;
                // We found a match so lets break our while loop...
                respondToMatch(matchIndex);
                // byteCounter = 0;
                Serial.println(matchIndex);
                isCommunicating = false;
            }
            // Serial.println("No match");
            // Else no match so we should wait for a new byte...
        //}
        }

        isBusy = !READ_BUSYPIN;
    }

    // Reset
    melbus_Bitposition = 7;
    byteCounter = 0;
}

// Tries to find a match in commands list.
byte findMatch(byte receivedBytes[], byte len)
{
    // Loop through all commands.
    for (byte cmd = 0; cmd < listLen; cmd++)
    {
        // printBytes(receivedBytes, len);
        bool isMatch = false;
        byte amountOfMatchingBytes = 0;
        bool goToNext = false;

        if (len != commands[cmd][0]) {
            continue;
        }

        // Quick check for match first 3 bits
        if (receivedBytes[0] == commands[cmd][1] &&
            receivedBytes[1] == commands[cmd][2] &&
            receivedBytes[2] == commands[cmd][3])
        {
            return cmd;
        }

        // // Loop through all bytes
        // for (byte pos = 0; pos < len; pos++)
        // {
        //     // If byte matches...
        //     if (receivedBytes[pos] == commands[cmd][pos + 1])
        //     {
        //         amountOfMatchingBytes++;
        //     }
        //     else
        //     {
        //         // If there is no match, skip this and continue to next one.
        //         goToNext = true;
        //         break;
        //     }

        //     if (amountOfMatchingBytes == commands[cmd][0])
        //     {
        //         isMatch = true;
        //         break;
        //     }
        // }

        // if (goToNext)
        // {
        //     continue;
        // }

        // if (isMatch)
        // {
        //     return cmd + 1;
        // }
    }

    return 0;
}

// Responds to a command.
void respondToMatch(byte matchIndex)
{
    byte cnt = 0;
    byte b1 = 0;
    byte b2 = 0;

    byte readBuffer[32];

    switch (matchIndex)
    {
    case 0: // MRB_1
        //wait for master_id and respond with same

        waitForNextBytes(readBuffer, 1);
        if (readBuffer[0] == MASTER_ID)
        {
            sendByteToMelbus(MASTER_ID);
            // I think here we can send text to display.
            SendText(textRow);
            break;
        }
        break;

    case 1: // Main init
        // Wait for base_id and respond with response_id
        waitForNextBytes(readBuffer, 1);

        if (readBuffer[0] == BASE_ID)
        {
            sendByteToMelbus(RESPONSE_ID);
        }
        else if (readBuffer[0] == CDC_BASE_ID)
        {
            sendByteToMelbus(CDC_RESPONSE_ID);
        }

        Serial.println("Main init done!");
        break;

    case 2: // Secondary init
        waitForNextBytes(readBuffer, 1);

        if (readBuffer[0] == CDC_BASE_ID)
        {
            sendByteToMelbus(CDC_RESPONSE_ID);
        }
        else if (readBuffer[0] == BASE_ID)
        {
            sendByteToMelbus(RESPONSE_ID);
        }
        break;
    case 3: // CMD_1, answer with 0x10 i guess?
        // we read 3 different tuple bytes (0x00 92), (01,3) and (02,5), response is always 0x10;
        waitForNextBytes(readBuffer, 3);
        sendByteToMelbus(0x10);
        // TODO: Maybe 3 should be 2 ???
        Serial.println("c3");
        printBytes(readBuffer, 3);

        break;

    case 4: // PUP. Power on?
        // {0xC0, 0x1C, 0x70, 0x02} we respond 0x90;
        waitForNextBytes(readBuffer, 2);
        sendByteToMelbus(0x90);
        Serial.println("c4");
        printBytes(readBuffer, 2);

        break;
    case 5: // MRB_2
        // {00 1E EC };
        //wait for master_id and respond with same
        waitForNextBytes(readBuffer, 1);

        if (readBuffer[0] == MASTER_ID)
        {
            sendByteToMelbus(MASTER_ID);
            SendText(textRow);
            break;
        }

        Serial.println("MRB_2 done");
        break;
    case 6: // CMD_3
        sendByteToMelbus(0x10);
        sendByteToMelbus(0x80);
        sendByteToMelbus(0x92);
        break;

    case 7: // C1_1
        sendBytesToMelbus(C1_Init_1, SO_C1_Init_1);
        break;

    case 8: // C1_2, 8 respond with c1_init_2 (contains text)
        sendBytesToMelbus(C1_Init_2, SO_C1_Init_2);
        break;

    case 9: // C3_0, 9 respond with c3_init_0
        sendBytesToMelbus(C3_Init_0, SO_C3_Init_0);
        break;

    case 10: // C3_1, 10 respond with c3_init_1
        sendBytesToMelbus(C3_Init_1, SO_C3_Init_1);
        break;

    case 11: // C3_2, 11 respond with c3_init_2
        sendBytesToMelbus(C3_Init_2, SO_C3_Init_2);
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
        while (!READ_BUSYPIN)
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
        while (!READ_BUSYPIN)
        {
            if (byteIsRead)
            {
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
}

void waitForNextBytes(byte buffer[], byte bufferLength)
{
    byte receivedBytes = 0;
    while (!READ_BUSYPIN) // While busy pin is active (low)
    {
        if (byteIsRead)
        {
            byteIsRead = false;
            buffer[receivedBytes] = melbus_ReceivedByte;
            receivedBytes++;

            if (receivedBytes == bufferLength)
            {
                break;
            }
        }
    }
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

// Send multiple bytes to melbus.
void sendBytesToMelbus(const byte bytesToSend[], byte length)
{
    for (byte i = 0; i < length; i++)
    {
        sendByteToMelbus(bytesToSend[i]);
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
    Serial.println("print bytes");
    for (byte b = 0; b < len; b++)
    {
        Serial.print(bytes[b], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void SendTrackInfo()
{
    sendBytesToMelbus(trackInfo, 9);
    Serial.println("Send track info");
}

void SendCartridgeInfo()
{
    sendBytesToMelbus(cartridgeInfo, 6);
    Serial.println("Send cartridge info");
}

//This method generates our own clock. Used when in master mode.
void sendByteToMelbus2(byte byteToSend)
{
    delayMicroseconds(700);
    //For each bit in the byte
    //char, since it will go negative. byte 0..255, char -128..127
    //int takes more clockcycles to update on a 8-bit CPU.
    for (char i = 7; i >= 0; i--)
    {
        delayMicroseconds(7);
        SET_CLOCKPIN_LOW;

        //If bit [i] is "1" - make datapin high
        if (byteToSend & (1 << i))
        {
            SET_DATAPIN_HIGH;
        }
        //if bit [i] is "0" - make datapin low
        else
        {
            SET_DATAPIN_LOW;
        }
        //wait for output to settle
        delayMicroseconds(5);
        SET_CLOCKPIN_HIGH;
    }
    //wait for HU to read the byte
    delayMicroseconds(20);
}

void SendText(byte rowNum)
{
    //Disable interrupt on INT_NUM quicker than: detachInterrupt(MELBUS_CLOCKBIT_INT);
    DISABLE_CLOCK_INTERRUPT;

    //Convert datapin and clockpin to output
    SET_DATAPIN_HIGH;
    SET_DATAPIN_AS_OUTPUT;
    SET_CLOCKPIN_HIGH;
    SET_CLOCKPIN_AS_OUTPUT;

    //send header
    for (byte b = 0; b < 4; b++)
    {
        sendByteToMelbus2(textHeader[b]);
    }

    //send which row to show it on
    sendByteToMelbus2(rowNum);

    //send text
    for (byte b = 0; b < 36; b++)
    {
        sendByteToMelbus2(customText[rowNum - 1][b]);
    }

    SET_CLOCKPIN_AS_INPUT;  //back to input (PULLUP since we clocked in the last bit with clk = high)
    //Reset datapin to high and return it to an input
    //pinMode(MELBUS_DATA, INPUT_PULLUP);
    SET_DATAPIN_HIGH;
    SET_DATAPIN_AS_INPUT; //this may or may not change the state, depending on the last transmitted bit
    PORTD |= 1 << MELBUS_DATA; //release the data line

    //Clear INT flag
    CLEAR_CLOCK_INTERRUPT_FLAG;
    //Enable interrupt on INT_NUM, quicker than: attachInterrupt(MELBUS_CLOCKBIT_INT, MELBUS_CLOCK_INTERRUPT, RISING);
    ENABLE_CLOCK_INTERRUPT;

    for (byte b = 0; b < 36; b++)
    {
        Serial.print(char(customText[rowNum - 1][b]));
    }
    Serial.println(" END");
}

void reqMaster()
{
    SET_DATAPIN_AS_OUTPUT;
    SET_DATAPIN_LOW;
    delayMicroseconds(700);
    delayMicroseconds(800);
    delayMicroseconds(800);
    SET_DATAPIN_HIGH;
    SET_DATAPIN_AS_INPUT;
}
