#if !defined(MELBUS_VARS_H)
#define MELBUS_VARS_H 1

#include <Arduino.h>

#define RESPONSE_ID 0xC5      //ID while responding to init requests (which will use base_id)
#define BASE_ID 0xC0          //ID when getting commands from HU
#define MASTER_ID 0xC7

#define CDC_RESPONSE_ID 0xEE  //ID while responding to init requests (which will use base_id)
#define CDC_MASTER_ID 0xEF    //ID while requesting/beeing master
#define CDC_BASE_ID 0xE8      //ID when getting commands from HU
#define CDC_ALT_ID 0xE9       //Alternative ID when getting commands from HU

const byte C1_Init_1[9] = {
  0x10, 0x00, 0xc3, 0x01,
  0x00, 0x81, 0x01, 0xff,
  0x00
};

const byte SO_C1_Init_1 = 9;

const byte C1_Init_2[11] = {
  0x10, 0x01, 0x81,
  'V', 'o', 'l', 'v', 'o', '!', ' ', ' '
};
const byte SO_C1_Init_2 = 11;


const byte C2_Init_1[] = {
  0x10, 0x01, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff
};
const byte SO_C2_Init_1 = 19;


const byte C3_Init_0[30] = {
  0x10, 0x00, 0xfe, 0xff,
  0xff, 0xdf, 0x3f, 0x29,
  0x2c, 0xf0, 0xde, 0x2f,
  0x61, 0xf4, 0xf4, 0xdf,
  0xdd, 0xbf, 0xff, 0xbe,
  0xff, 0xff, 0x03, 0x00,
  0xe0, 0x05, 0x40, 0x00,
  0x00, 0x00
} ;
const byte SO_C3_Init_0 = 30;

const byte C3_Init_1[30] = {
  0x10, 0x01, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff
};
const byte SO_C3_Init_1 = 30;

const byte C3_Init_2[30] = {
  0x10, 0x02, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff
};
const byte SO_C3_Init_2 = 30;


//Defining the commands. First byte is the length of the command.
#define MRB_1 {3, 0x00, 0x1C, 0xEC}            //Master Request Broadcast version 1
#define MRB_2 {3, 0x00, 0x1E, 0xEC}            //Master Request Broadcast version 2 (maybe this is second init seq?)
#define MI {3, 0x07, 0x1A, 0xEE}               //Main init sequence
#define SI {3, 0x00, 0x1E, 0xED}               //Secondary init sequence (turn off ignition, then on)
//changed from 00 1D ED

#define C1_1 {5, 0xC1, 0x1B, 0x7F, 0x01, 0x08} //Respond with c1_init_1
#define C1_2 {5, 0xC1, 0x1D, 0x73, 0x01, 0x81} //Respond with c1_init_2 (text)

#define C2_0 {4, 0xC2, 0x1D, 0x73, 0x00}       //Get next byte (nn) and respond with 10, 0, nn, 0,0 and 14 * 0x20 (possibly text)
#define C2_1 {4, 0xC2, 0x1D, 0x73, 0x01}       //Same as above? Answer 19 bytes (unknown)

#define C3_0 {4, 0xC3, 0x1F, 0x7C, 0x00}       //Respond with c1_init_2 (text)
#define C3_1 {4, 0xC3, 0x1F, 0x7C, 0x01}       //Respond with c1_init_2 (text)
#define C3_2 {4, 0xC3, 0x1F, 0x7C, 0x02}       //Respond with c1_init_2 (text)

#define C5_1 {3, 0xC5, 0x19, 0x73}  //C5, 19, 73, xx, yy. Answer 0x10, xx, yy + free text. End with 00 00 and pad with spaces

#define CMD_1 {3, 0xC0, 0x1B, 0x76}            //Followed by: [00, 92, FF], OR [01, 03 ,FF] OR [02, 05, FF]. Answer 0x10
#define PUP {4, 0xC0, 0x1C, 0x70, 0x02}      //Wait 2 bytes and answer 0x90?
#define CMD_3 {5, 0xC0, 0x1D, 0x76, 0x80, 0x00} //Answer: 0x10, 0x80, 0x92
#define PWR_OFF {6,0xC0, 0x1C, 0x70, 0x00, 0x80, 0x01} //answer one byte
#define PWR_SBY {6,0xC0, 0x1C, 0x70, 0x01, 0x80, 0x01} //answer one byte
#define IGN_OFF {3, 0x00, 0x18, 0x12}           //this is the last message before HU goes to Nirvana

#define BTN {4, 0xC0, 0x1D, 0x77, 0x81}        //Read next byte which is the button #. Respond with 3 bytes
#define NXT {5, 0xC0, 0x1B, 0x71, 0x80, 0x00}  //Answer 1 byte
#define PRV {5, 0xC0, 0x1B, 0x71, 0x00, 0x00}  //Answer 1 byte
#define SCN {4, 0xC0, 0x1A, 0x74, 0x2A}        //Answer 1 byte

#define CDC_CIR {3, CDC_BASE_ID, 0x1E, 0xEF}             //Cartridge info request. Respond with 6 bytes
#define CDC_TIR {5, CDC_ALT_ID, 0x1B, 0xE0, 0x01, 0x08}  //track info req. resp 9 bytes
#define CDC_NXT {5, CDC_BASE_ID, 0x1B, 0x2D, 0x40, 0x01} //next track.
#define CDC_PRV {5, CDC_BASE_ID, 0x1B, 0x2D, 0x00, 0x01} //prev track
#define CDC_CHG {3, CDC_BASE_ID, 0x1A, 0x50}             //change cd
#define CDC_PUP {3, CDC_BASE_ID, 0x19, 0x2F}             //power up. resp ack (0x00).
#define CDC_PDN {3, CDC_BASE_ID, 0x19, 0x22}             //power down. ack (0x00)
#define CDC_FFW {3, CDC_BASE_ID, 0x19, 0x29}             //FFW. ack
#define CDC_FRW {3, CDC_BASE_ID, 0x19, 0x26}             //FRW. ack
#define CDC_SCN {3, CDC_BASE_ID, 0x19, 0x2E}             //scan mode. ack
#define CDC_RND {3, CDC_BASE_ID, 0x19, 0x52}             //random mode. ack
#define CDC_NU {3, CDC_BASE_ID, 0x1A, 0x50}              //not used
//#define CDC_MI {0x07, 0x1A, 0xEE},         //main init seq. wait for BASE_ID and respond with RESPONSE_ID.
//#define CDC_SI {0x00, 0x1C, 0xED},         //secondary init req. wait for BASE_ID and respond with RESPONSE_ID.
//#define CDC_MRB {0x00, 0x1C, 0xEC}         //master req broadcast. wait for MASTER_ID and respond with MASTER_ID.

//This list can be quite long. We have approx 700 us between the received bytes.
const byte commands[][7] = {
  MRB_1,  // 0 now we are master and can send stuff (like text) to the display!
  MI,     // 1 main init
  SI,     // 2 sec init (00 1E ED respond 0xC5 !!)
  CMD_1,  // 3 follows: [0, 92, FF], OR [1,3 ,FF] OR [2, 5 FF]
  PUP,  // 4 wait 2 bytes and answer 0x90?
  MRB_2,  // 5 alternative master req bc
  CMD_3,  // 6 unknown. Answer: 0x10, 0x80, 0x92
  C1_1,   // 7 respond with c1_init_1
  C1_2,   // 8 respond with c1_init_2 (contains text)
  C3_0,   // 9 respond with c3_init_0
  C3_1,   // 10 respond with c3_init_1
  C3_2,   // 11 respond with c3_init_2
  C2_0,   // 12 get next byte (nn) and respond with 10, 0, nn, 0,0 and 14 of 0x20
  C2_1,   // 13
  C5_1,   // 14
  BTN,    // 15
  NXT,    // 16
  PRV,    // 17
  SCN,    // 18
  PWR_OFF,// 19
  PWR_SBY,// 20
  IGN_OFF, // 21
  CDC_CIR, // 22
  CDC_TIR, // 23
  CDC_NXT, // 24
  CDC_PRV, // 25
  CDC_CHG, // 26
  CDC_PUP, // 27
  CDC_PDN, // 28
  CDC_FFW, // 29
  CDC_FRW, // 30
  CDC_SCN, // 31
  CDC_RND, // 32
  CDC_NU   // 33
};

const byte listLen = 34; //how many rows in the above array

#endif
