#include <SPI.h>
#include <SD.h>

const byte ELECT		= 0x10;
const byte ELECTACK		= 0x11;
const byte SYNC			= 0x12;
const byte SYNCACK		= 0x13;
const byte SYNCCORR		= 0X14;
const byte DATA			= 0X20;

const byte SYNCPHASE = 0xA0;
const byte ACQUISITION = 0xA1;
const byte SENDDATA = 0XA2;

const byte SYNC_ATTEMPTS = 10;

const unsigned long SYNC_TIMEOUT_MILLIS = 5000000;

File sdFile;

unsigned int pingBuffer[20];
unsigned long correctionBuffer[20];

uint8_t seqnumSync = 0;
size_t size = 0;

boolean sent = false;
boolean master = false;

byte indexSyncBuffer;

byte phase;

unsigned long starttime = 0;
unsigned long timestamp = 0;
unsigned long endtime = 0;
unsigned long startAcquisition = 0;

unsigned long startSyncTime = 0;
unsigned long endSyncTime = 0;
unsigned long correction = 0;

//Bluetooth command utility
void clearSerial()
{
	while (Serial.available() > 0)
		Serial.read();
}

void goInCommandMode()
{
	Serial.print("$$$");
	delay(100);
}

void exitFromCommandMode()
{
	Serial.println("---");
	delay(100);
}

void reboot()
{
	Serial.println("R,1");
	delay(1000);
}

void configureSlave()
{	
	Serial.println("SM,0");
	delay(100);
}

void configureMaster()
{
	Serial.println("SM,1");
	delay(100);
}

void connectTo(String address)
{
	Serial.println("C,"+address);
	delay(100);
}

void killConnection()
{
	Serial.println("K,");
	delay(100);
}
//End bluetooth command

void incomingData()
{
	//store_in_buffer(micros());
	Serial.println(micros() + correction);
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
	goInCommandMode();
	configureSlave();
	exitFromCommandMode();
	attachInterrupt(1, incomingData, CHANGE);
	
}

void serialEvent()
{
	if (Serial.available() > 0)
	{
		unsigned long receptionTime = micros();
		char headerChar;
		Serial.readBytes(&headerChar, 1);
		if (headerChar == 'T')
		{
			Serial.readBytes(&headerChar, 1);
			if (headerChar == 'S')
			{
				byte msgType;
				byte seqnum;
				Serial.readBytes(&msgType, 1);
				switch (msgType)
				{
				case ELECT:
					Serial.readBytes(&seqnum, 1);
					byte ack[4];
					ack[0] = 'T';
					ack[1] = 'S';
					ack[2] = ELECTACK;
					ack[3] = seqnum;
					Serial.write(ack, 4);
					delay(500);
					//TODO
					goInCommandMode();
					killConnection();
					configureMaster();
					reboot();					
					goInCommandMode();
					clearSerial();

					connectTo("0006667286D5");
					exitFromCommandMode();
					phase = SYNCPHASE;
					indexSyncBuffer = 0;
					break;
				case SYNC:
					digitalWrite(LED_BUILTIN, HIGH);
					delay(200);
					digitalWrite(LED_BUILTIN, LOW);
					Serial.readBytes(&seqnum, 1);
					seqnumSync = seqnum;
					byte syncackpkt[8];
					syncackpkt[0] = 'T';
					syncackpkt[1] = 'S';
					syncackpkt[2] = SYNCACK;
					syncackpkt[3] = seqnum;
					memcpy(syncackpkt + 4, &receptionTime, sizeof(receptionTime));
					Serial.write(syncackpkt, 8);				
					break;
				case SYNCACK:
					Serial.readBytes(&seqnum, 1);
					if (seqnum == seqnumSync)
					{
						byte timeBuffer[4];
						Serial.readBytes(timeBuffer, 4);
						unsigned long time;
						memcpy(&time, timeBuffer, sizeof(time));

						pingBuffer[indexSyncBuffer] = (receptionTime - startSyncTime) / 2;
						correctionBuffer[indexSyncBuffer] = time - receptionTime;
						indexSyncBuffer++;

						if (indexSyncBuffer >= SYNC_ATTEMPTS)
						{
							unsigned long correctionMean = 0;
							unsigned long pingMean = 0;

							for (int i = 0; i < SYNC_ATTEMPTS; i++)
							{
								correctionMean += correctionBuffer[i];
								pingMean += pingBuffer[i];
							}

							correctionMean /= SYNC_ATTEMPTS;
							pingMean /= SYNC_ATTEMPTS;
								

							unsigned long receiverCorrection = correctionMean + pingMean;

							memset(timeBuffer, 0, 4);
							memcpy(timeBuffer, &receiverCorrection, sizeof(receiverCorrection));

							byte msgCorrection[8];
							msgCorrection[0] = 'T';
							msgCorrection[1] = 'S';
							msgCorrection[2] = SYNCCORR;
							msgCorrection[3] = seqnum;
							memcpy(msgCorrection + 4, &receiverCorrection, sizeof(receiverCorrection));

							Serial.write(msgCorrection, 8);

							delay(1000);

							goInCommandMode();
							killConnection();
							configureSlave();
							exitFromCommandMode();

							phase = SENDDATA;
							digitalWrite(LED_BUILTIN, HIGH);
						}
						else
						{
							sent = false;
						}
					}
					break;
				case SYNCCORR:
					Serial.readBytes(&seqnum, 1);
					if (seqnum == seqnumSync)
					{
						byte timeBuffer[4];
						Serial.readBytes(timeBuffer, 4);
						memcpy(&correction, timeBuffer, sizeof(correction));
						
						phase = SENDDATA;
						digitalWrite(LED_BUILTIN, HIGH);
					}
					break;
				default:
					break;
				}
			}
			else
				clearSerial();
		}
	}
}

void loop(){
	switch (phase)
	{
	case SYNCPHASE:
		if (!sent && indexSyncBuffer < SYNC_ATTEMPTS)
		{
			seqnumSync = (byte)random();
			byte syncpkt[4];
			syncpkt[0] = 'T';
			syncpkt[1] = 'S';
			syncpkt[2] = SYNC;
			syncpkt[3] = seqnumSync;
			Serial.write(syncpkt, 4);
			startSyncTime = micros();
			sent = true;
		}
		else
		{
			if (micros() - startSyncTime > SYNC_TIMEOUT_MILLIS)
			{
				sent = false;
			}
		}
		break;
	case SENDDATA:
		byte msgData[6];
		msgData[0] = 'T';
		msgData[1] = 'S';
		timestamp = micros() - correction;
		memcpy(msgData + 2, &timestamp, sizeof(timestamp));
		Serial.println(timestamp);
		delay(2000);
		break;
	default:
		break;
	}
	/*if (phase == ACQUISITION)
	{
		Serial.println("Acquisition");
		if (millis() - startAcquisition > 3000)
		{
			sdFile.close();
			phase = 0;
		}
		else
		{
			sdFile.println("Prova");
		}
		
	}
	*/
}