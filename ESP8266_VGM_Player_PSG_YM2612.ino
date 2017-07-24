//YM2612 & SN76489
#include "FS.h"
#include "music.h"

//Control bits (on control shift register)
// SN_WE = QA (bit 0, pin 15)
// YM_IC = QB (bit 1, pin 1)
// YM_CS = QC (bit 2, pin 2)
// YM_WR = QD (bit 3, pin 3)
// YM_RD = QE (bit 4, pin 4)
// YM_A0 = QF (bit 5, pin 5)
// YM_A1 = QG (bit 6, pin 6)
// NO CONNECT = QH (bit 7, pin 7)

//PSG DATA - Shift Register 
const int psgLatch = D0;
const int psgClock = D1;
const int psgData = D2;

//Control Shift Register
const int controlLatch = D3; 
const int controlClock = D4; 
const int controlData = D5;

//YM DATA - Shift Register
const int ymLatch = D6;
const int ymClock = D7;
const int ymData = D8; 

//Timing Variables
float singleSampleWait = 0;
const int sampleRate = 44100; //44100
const float WAIT60TH = ((1000.0 / (sampleRate/(float)735))*1000);  
const float WAIT50TH = ((1000.0 / (sampleRate/(float)882))*1000);  
uint32_t waitSamples = 0;
unsigned long preCalced8nDelays[16];
unsigned long preCalced7nDelays[16];
unsigned long lastWaitData61 = 0;

unsigned long cachedWaitTime61 = 0;

unsigned long pauseTime = 0;
unsigned long startTime = 0;

//Song Data Variables
#define MAX_PCM_BUFFER_SIZE 30000 //In bytes
uint8_t pcmBuffer[MAX_PCM_BUFFER_SIZE];
uint32_t pcmBufferPosition = 0;
uint8_t cmd;
uint32_t loopOffset = 0;

//File Stream
File vgm;
const unsigned int MAX_CMD_BUFFER = 3000;
char cmdBuffer[MAX_CMD_BUFFER];
uint32_t bufferPos = 0;

//SONG INFO
const int NUMBER_OF_FILES = 6; //How many VGM files do you have stored in flash? (Files should be named (1.vgm, 2.vgm, 3.vgm, etc);
int currentTrack = 1;


void setup() 
{  
  pinMode(SS, OUTPUT);
  for(int i = 0; i < 10; i++)
  {
    digitalWrite(SS, HIGH);
    delay(150);
    digitalWrite(SS, LOW);
    delay(150);
  }
  
  //Setup SN DATA 595
  pinMode(psgLatch, OUTPUT);
  pinMode(psgClock, OUTPUT);
  pinMode(psgData, OUTPUT);

  //Setup CONTROL 595
  pinMode(controlLatch, OUTPUT);
  pinMode(controlClock, OUTPUT);
  pinMode(controlData, OUTPUT);

  //Setup YM DATA 595
  pinMode(ymLatch, OUTPUT);
  pinMode(ymClock, OUTPUT);
  pinMode(ymData, OUTPUT);
  
  SPIFFS.begin();
  Serial.begin(115200);
  StartupSequence();
}

void StartupSequence()
{
  waitSamples = 0;
  loopOffset = 0;
  lastWaitData61 = 0;
  cachedWaitTime61 = 0;
  pauseTime = 0;
  startTime = 0;
  cmd = 0;
  ClearBuffers();
  String track = "/"+String(currentTrack)+".vgm";
  Serial.print("Opening track: ");
  Serial.println(track);
  vgm = SPIFFS.open(track, "r");
    if(!vgm)
      Serial.print("File open failed");
      
  FillBuffer();
  for(int i = bufferPos; i<0x17; i++) GetByte(); //Ignore the unimportant VGM header data
  
  for ( int i = bufferPos; i < 0x1B; i++ ) //0x18->0x1B : Get wait Samples count
  {
    waitSamples += uint32_t(GetByte()) << ( 8 * i );
  }

  for ( int i = bufferPos; i < 0x1F; i++ ) //0x1C->0x1F : Get loop offset Postition
  {
    loopOffset += uint32_t(GetByte()) << ( 8 * i );
  }
  for ( int i = bufferPos; i < 0x40; i++ ) GetByte(); //Go to VGM data start
  singleSampleWait = ((1000.0 / (sampleRate/(float)1))*1000);  

  for(int i = 0; i<16; i++)
  {
    if(i == 0)
    {
      preCalced8nDelays[i] = 0;
      preCalced7nDelays[i] = 1;
    }
    else
    {  
      preCalced8nDelays[i] = ((1000.0 / (sampleRate/(float)i))*1000);  
      preCalced7nDelays[i] = ((1000.0 / (sampleRate/(float)i+1))*1000);  
    }
  }

  ResetRegisters();

  SN_WE(HIGH);
  SendControlReg();
  
  delay(500);
  SilenceAllChannels();
  YM_A0(LOW);
  YM_A1(LOW);
  YM_CS(HIGH);
  YM_WR(HIGH);
  YM_RD(HIGH);
  YM_IC(HIGH);
  SendControlReg();
  delay(10);
  YM_IC(LOW);
  SendControlReg();
  delay(10);
  YM_IC(HIGH);
  SendControlReg();
  delay(500);
}

byte GetByte()
{
  if(bufferPos == MAX_CMD_BUFFER)
  {
    bufferPos = 0;
    FillBuffer();
  }
  return cmdBuffer[bufferPos++];
}

void FillBuffer()
{
    vgm.readBytes(cmdBuffer, MAX_CMD_BUFFER);
}

void ClearBuffers()
{
  pcmBufferPosition = 0;
  bufferPos = 0;
  for(int i = 0; i < MAX_CMD_BUFFER; i++)
    cmdBuffer[i] = 0;
  for(int i = 0; i < MAX_PCM_BUFFER_SIZE; i++)
    pcmBuffer[i] = 0;
}

void SilenceAllChannels()
{
  SendSNByte(0x9f);
  SendSNByte(0xbf);
  SendSNByte(0xdf);
  SendSNByte(0xff);
}

void SN_WE(bool state)
{
  SetControlReg(0, state);
  SendControlReg();
}

void YM_IC(bool state)
{
  SetControlReg(1, state);
}

void YM_CS(bool state)
{
  SetControlReg(2, state);
}

void YM_WR(bool state)
{
  SetControlReg(3, state);
}

void YM_RD(bool state)
{
  SetControlReg(4, state);
}

void YM_A0(bool state)
{
  SetControlReg(5, state);
}

void YM_A1(bool state)
{
  SetControlReg(6, state);
}

void ResetRegisters()
{
   digitalWrite(controlLatch, LOW);
   shiftOut(controlData, controlClock, MSBFIRST, 0x00);
   digitalWrite(controlLatch, LOW); 
   
   digitalWrite(psgLatch, LOW);
   shiftOut(psgData, psgClock, MSBFIRST, 0x00);   
   digitalWrite(psgLatch, HIGH);

   digitalWrite(ymLatch, LOW);
   shiftOut(ymData, ymClock, MSBFIRST, 0x00);
   digitalWrite(ymLatch, HIGH);   
}

uint8_t controlRegister = 0x00;
void SendControlReg()
{
  digitalWrite(controlLatch, LOW);
  shiftOut(controlData, controlClock, MSBFIRST, controlRegister);
  digitalWrite(controlLatch, HIGH);
}

void SetControlReg(byte bitLocation, bool state)
{
    state == HIGH ? controlRegister |= 1 << bitLocation : controlRegister &= ~(1 << bitLocation);
}

void SendSNByte(byte b) //Send 1-byte of data to PSG
{
  SN_WE(HIGH);
  digitalWrite(psgLatch, LOW);
  shiftOut(psgData, psgClock, MSBFIRST, b);   
  digitalWrite(psgLatch, HIGH);
  SN_WE(LOW);
  delayMicroseconds(1);
  SN_WE(HIGH);
}

void SendYMByte(byte b)
{
  digitalWrite(ymLatch, LOW);
  shiftOut(ymData, ymClock, MSBFIRST, b);
  digitalWrite(ymLatch, HIGH);
}

void ShiftControlFast(byte b)
{
  digitalWrite(controlLatch, LOW);
  shiftOut(controlData, controlClock, MSBFIRST, b);
  digitalWrite(controlLatch, HIGH);
}

void ICACHE_FLASH_ATTR loop(void) 
{
  while(Serial.available() > 0)
  {
    if(Serial.readString() == "+")
    {
      if(currentTrack + 1 > NUMBER_OF_FILES)
        currentTrack = 1;
      else
        currentTrack++; 
      vgm.close();
      StartupSequence(); 
    }
  }
         
  
  unsigned long timeInMicros = micros();
  if( timeInMicros - startTime <= pauseTime)
  {
    return;
  }

  cmd = GetByte();
  //Serial.println(cmd, HEX);
  
  switch(cmd) //Use this switch statement to parse VGM commands
  {
    case 0x50:
    SendSNByte(GetByte());
    startTime = timeInMicros;
    pauseTime = singleSampleWait;
    //delay(singleSampleWait);
    break;
    
    case 0x52:
    {
    uint8 address = GetByte();
    uint8 data = GetByte();
    YM_A1(LOW);
    YM_A0(LOW);
    YM_CS(LOW);
    SendControlReg();
    //ShiftControlFast(B00011010);
    SendYMByte(address);
    YM_WR(LOW);
    SendControlReg();
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    SendControlReg();
    //delayMicroseconds(1);
    YM_A0(HIGH);
    YM_CS(LOW);
    SendControlReg();
    SendYMByte(data);
    YM_WR(LOW);
    SendControlReg();
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    SendControlReg();
    }
    startTime = timeInMicros;
    pauseTime = singleSampleWait;
    //delay(singleSampleWait);
    break;
    
    case 0x53:
    {
    uint8 address = GetByte();
    uint8 data = GetByte();
    YM_A1(HIGH);
    YM_A0(LOW);
    YM_CS(LOW);
    SendControlReg();
    SendYMByte(address);
    YM_WR(LOW);
    SendControlReg();
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    SendControlReg();
    //delayMicroseconds(1);
    YM_A0(HIGH);
    YM_CS(LOW);
    SendControlReg();
    SendYMByte(data);
    YM_WR(LOW);
    SendControlReg();
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    SendControlReg();
    }
    startTime = timeInMicros;
    pauseTime = singleSampleWait;
    //delay(singleSampleWait);
    break;

    
    case 0x61: 
    {
      //Serial.print("0x61 WAIT: at location: ");
      //Serial.print(parseLocation);
      //Serial.print("  -- WAIT TIME: ");
    uint32_t wait = 0;
    for ( int i = 0; i < 2; i++ ) 
    {
      wait += ( uint32_t( GetByte() ) << ( 8 * i ));
    }
    if(lastWaitData61 != wait) //Avoid doing lots of unnecessary division.
    {
      lastWaitData61 = wait;
      if(wait == 0)
        break;
      cachedWaitTime61 = ((1000.0 / (sampleRate/(float)wait))*1000);  
    }
    //Serial.println(cachedWaitTime61);
    
    startTime = timeInMicros;
    pauseTime = cachedWaitTime61;
    //delay(cachedWaitTime61);
    break;
    }
    case 0x62:
    startTime = timeInMicros;
    pauseTime = WAIT60TH;
    //delay(WAIT60TH); //Actual time is 16.67ms (1/60 of a second)
    break;
    case 0x63:
    startTime = timeInMicros;
    pauseTime = WAIT50TH;
    //delay(WAIT50TH); //Actual time is 20ms (1/50 of a second)
    break;
    case 0x67:
    {
      //Serial.print("DATA BLOCK 0x67.  PCM Data Size: ");
      GetByte(); 
      GetByte(); //Skip 0x66 and data type
      pcmBufferPosition = bufferPos;
      uint32_t PCMdataSize = 0;
      for ( int i = 0; i < 4; i++ ) 
      {
        PCMdataSize += ( uint32_t( GetByte() ) << ( 8 * i ));
      }
      //Serial.println(PCMdataSize);
      
      for ( int i = 0; i < PCMdataSize; i++ ) 
      {
         if(PCMdataSize <= MAX_PCM_BUFFER_SIZE)
            pcmBuffer[ i ] = (uint8_t)GetByte(); 
      }
      //Serial.println("Finished buffering PCM");
      break;
    }
    
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F: 
    {
      //Serial.println("0x7n WAIT");
      uint32_t wait = cmd & 0x0F;
      //Serial.print("Wait value: ");
      //Serial.println(wait);
      startTime = timeInMicros;
      pauseTime = preCalced7nDelays[wait];
      //delay(preCalced7nDelays[wait]);
    break;
    }
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
      {
      uint32_t wait = cmd & 0x0F;
      uint8 address = 0x2A;
      uint8 data = pcmBuffer[pcmBufferPosition++];
      //pcmBufferPosition++;
      YM_A1(LOW);
      YM_A0(LOW);
      YM_CS(LOW);
      SendControlReg();
      //ShiftControlFast(B00011010);
      SendYMByte(address);
      YM_WR(LOW);
      SendControlReg();
      //delayMicroseconds(1);
      YM_WR(HIGH);
      YM_CS(HIGH);
      SendControlReg();
      //delayMicroseconds(1);
      YM_A0(HIGH);
      YM_CS(LOW);
      SendControlReg();
      SendYMByte(data);
      YM_WR(LOW);
      SendControlReg();
      //delayMicroseconds(1);
      YM_WR(HIGH);
      YM_CS(HIGH);
      SendControlReg();
      startTime = timeInMicros;
      pauseTime = preCalced8nDelays[wait];
      //delayMicroseconds(23*wait); //This is a temporary solution for a bigger delay problem.
      }      
      break;
    case 0xE0:
    {
      //Serial.print("LOCATION: ");
      //Serial.print(parseLocation, HEX);
      //Serial.print(" - PCM SEEK 0xE0. NEW POSITION: ");
      
      pcmBufferPosition = 0;
      for ( int i = 0; i < 4; i++ ) 
      {      
        pcmBufferPosition += ( uint32_t( GetByte() ) << ( 8 * i ));
      }
    }
      //Serial.println(pcmBufferPosition);    
    break;
    case 0x66:
    if(loopOffset == 0)
      loopOffset = 64;
    vgm.seek(loopOffset, SeekSet);
    FillBuffer();
    bufferPos = 0;
    break;
    default:
    break;
  }
}
