#include <Arduino.h>
#include "SoundChips.h"
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#define SAMPLE_OK 13

//File Stream
File vgm;
const unsigned int MAX_CMD_BUFFER = 64;
char cmdBuffer[MAX_CMD_BUFFER];
uint32_t bufferPos = 0;

//Timing Variables
float singleSampleWait = 0;
const int sampleRate = 44100; //44100 standard
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
#define MAX_PCM_BUFFER_SIZE 40000 //In bytes
uint8_t pcmBuffer[MAX_PCM_BUFFER_SIZE];
uint32_t pcmBufferPosition = 0;
uint8_t cmd;
uint32_t loopOffset = 0;
uint16_t loopCount = 0;
uint16_t nextSongAfterXLoops = 3;
bool play = true;

//SONG INFO
const int NUMBER_OF_FILES = 8; //How many VGM files do you have stored in flash? (Files should be named (1.vgm, 2.vgm, 3.vgm, etc);
int currentTrack = 1;

void FillBuffer()
{
    vgm.readBytes(cmdBuffer, MAX_CMD_BUFFER);
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

void SendSNByte(byte b)
{
  digitalWrite(SN_WE, HIGH);
  digitalWrite(SN_RCLK, LOW);
  shiftOut(SN_SER, SN_SRCLK, MSBFIRST, b);
  digitalWrite(SN_RCLK, HIGH);
  digitalWrite(SN_WE, LOW);
  delayMicroseconds(25);
  digitalWrite(SN_WE, HIGH);
}

void SendYMByte(byte b)
{
  digitalWrite(YM_RCLK, LOW);
  shiftOut(YM_SER, YM_SRCLK, MSBFIRST, b);
  digitalWrite(YM_RCLK, HIGH);
}

void ResetRegisters()
{
   digitalWrite(SN_RCLK, LOW);
   shiftOut(SN_SER, SN_SRCLK, MSBFIRST, 0x00);
   digitalWrite(SN_RCLK, HIGH);

   digitalWrite(YM_RCLK, LOW);
   shiftOut(YM_SER, YM_SRCLK, MSBFIRST, 0x00);
   digitalWrite(YM_RCLK, HIGH);
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

  digitalWrite(YM_A0, LOW);
  digitalWrite(YM_A1, LOW);
  digitalWrite(YM_CS, HIGH);
  digitalWrite(YM_WR, HIGH);
  digitalWrite(YM_RD, HIGH);
  digitalWrite(YM_IC, HIGH);
  delay(1);
  digitalWrite(YM_IC, LOW);
  delay(1);
  digitalWrite(YM_IC, HIGH);
}

void StartupSequence()
{
  waitSamples = 0;
  loopOffset = 0;
  lastWaitData61 = 0;
  cachedWaitTime61 = 0;
  pauseTime = 0;
  startTime = 0;
  loopCount = 0;
  cmd = 0;
  ClearBuffers();
  vgm = SD.open("/test.vgm", "r");
  if(!vgm)
    Serial.println("File open failed!");
  else
    Serial.println("Opened successfully...");
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
        preCalced7nDelays[i] = ((1000.0 / (sampleRate/(float)i+1))*1000);  //+1 is spec-accurate; however, on this hardware, it sounds better without it.
      }
    }

    ResetRegisters();
    SilenceAllChannels();
    digitalWrite(SN_WE, HIGH);
    delay(500);
}

void setup()
{
  pinMode(SAMPLE_OK, OUTPUT);
  //Setup YM DATA 595
  pinMode(YM_RCLK, OUTPUT);
  pinMode(YM_SRCLK, OUTPUT);
  pinMode(YM_SER, OUTPUT);

  //Setup SN DATA 595
  pinMode(SN_RCLK, OUTPUT);
  pinMode(SN_SRCLK, OUTPUT);
  pinMode(SN_SER, OUTPUT);

  //Sound chip control pins
  pinMode(SN_WE, OUTPUT);
  pinMode(YM_IC, OUTPUT);
  pinMode(YM_CS, OUTPUT);
  pinMode(YM_WR, OUTPUT);
  pinMode(YM_RD, OUTPUT);
  pinMode(YM_A0, OUTPUT);
  pinMode(YM_A1, OUTPUT);
  Serial.begin(115200);
  SilenceAllChannels();
  if(!SD.begin())
  {
      Serial.println("Card Mount Failed");
      return;
  }
  StartupSequence();
}

void loop()
{
  digitalWrite(SAMPLE_OK, HIGH);
  if(!play)
    return;
  unsigned long timeInMicros = micros();
  if( timeInMicros - startTime <= pauseTime)
  {
    return;
  }
  cmd = GetByte();

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
      byte address = GetByte();
      byte data = GetByte();
      digitalWrite(YM_A1, LOW);
      digitalWrite(YM_A0, LOW);
      digitalWrite(YM_CS, LOW);
      //Areas like this may require 1 microsecond delays.
      SendYMByte(address);
      digitalWrite(YM_WR, LOW);
      delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      delayMicroseconds(1);
      digitalWrite(YM_A0, HIGH);
      digitalWrite(YM_CS, LOW);
      SendYMByte(data);
      digitalWrite(YM_WR, LOW);
      delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      }
      startTime = timeInMicros;
      pauseTime = singleSampleWait;
      //delay(singleSampleWait);
      break;

      case 0x53:
      {
      byte address = GetByte();
      byte data = GetByte();
      digitalWrite(YM_A1, HIGH);
      digitalWrite(YM_A0, LOW);
      digitalWrite(YM_CS, LOW);
      SendYMByte(address);
      digitalWrite(YM_WR, LOW);
      delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      delayMicroseconds(1);
      digitalWrite(YM_A0, HIGH);
      digitalWrite(YM_CS, LOW);
      SendYMByte(data);
      digitalWrite(YM_WR, LOW);
      delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
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
        byte address = 0x2A;
        byte data = pcmBuffer[pcmBufferPosition++];
        //pcmBufferPosition++;
        digitalWrite(YM_A1, LOW);
        digitalWrite(YM_A0, LOW);
        digitalWrite(YM_CS, LOW);
        //ShiftControlFast(B00011010);
        SendYMByte(address);
        digitalWrite(YM_WR, LOW);
        delayMicroseconds(1);
        digitalWrite(YM_WR, HIGH);
        digitalWrite(YM_CS, HIGH);
        delayMicroseconds(1);
        digitalWrite(YM_A0, HIGH);
        digitalWrite(YM_CS, LOW);
        SendYMByte(data);
        digitalWrite(YM_WR, LOW);
        delayMicroseconds(1);
        digitalWrite(YM_WR, HIGH);
        digitalWrite(YM_CS, HIGH);
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
      loopCount++;
      vgm.seek(loopOffset, SeekSet);
      FillBuffer();
      bufferPos = 0;
      break;
      default:
      break;
    }
      digitalWrite(SAMPLE_OK, LOW);
}
