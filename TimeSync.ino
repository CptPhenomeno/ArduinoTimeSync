#include <SPI.h>
#include <SD.h>

const byte ELECT		 = 0x10;
const byte ELECTACK		 = 0x11;
const byte SYNC			 = 0x12;
const byte SYNCACK		 = 0x13;
const byte SYNCCORR		 = 0X14;
const byte DATA			 = 0X20;
const byte START_ACQUIRE = 0x21;
const byte STOP_ACQUIRE  = 0x22;
const byte START_ACQUIRE_ACK = 0x23;
const byte STOP_ACQUIRE_ACK = 0x24;

const byte SYNCPHASE = 0xA0;
const byte ACQUISITION = 0xA1;
const byte SENDDATA = 0XA2;

const byte SYNC_ATTEMPTS = 1;

const unsigned long SYNC_TIMEOUT_MICROS = 5000000;

File sdFile;

long pingBuffer[SYNC_ATTEMPTS];
long correctionBuffer[SYNC_ATTEMPTS];
volatile boolean interruptReceived = false;

uint8_t seqnumSync = 0;

boolean sent = false;
boolean master = false;
boolean waitLow = false;
boolean waitHigh = true;


long timetosend = 0;

byte indexSyncBuffer;

byte phase = 0;

long startSyncTime = 0;
long correction = 0;

void sortPings() {
	int size = 50;
	for (int i = 0; i<(size - 1); i++) {
		for (int o = 0; o<(size - (i + 1)); o++) {
			if (pingBuffer[o] > pingBuffer[o + 1]) {
				unsigned int t = pingBuffer[o];
				pingBuffer[o] = pingBuffer[o + 1];
				pingBuffer[o + 1] = t;

				long t1 = correctionBuffer[o];
				correctionBuffer[o] = correctionBuffer[o + 1];
				correctionBuffer[o + 1] = t1;
			}
		}
	}
}

void blinkDebug(int freq, int pow)
{
	for (int i = 0; i < freq; i++)
	{
		digitalWrite(LED_BUILTIN, HIGH);
		delay(pow);
		digitalWrite(LED_BUILTIN, LOW);
		delay(pow);
	}
}

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

void configureLowLatency()
{
	Serial.println("SQ,16");
	delay(100);
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

long correctionMean = 0;
long pingMean = 0;

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
	goInCommandMode();
	configureSlave();
	configureLowLatency();
	reboot();
	delay(2000);
	//attachInterrupt(0, incomingData, CHANGE);
}

void serialEvent()
{
	long receptionTime = micros();
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
			case START_ACQUIRE:
				phase = SENDDATA;
				byte startAcquireAck[3];
				startAcquireAck[0] = 'T';
				startAcquireAck[1] = 'S';
				startAcquireAck[2] = START_ACQUIRE_ACK;

				Serial.write(startAcquireAck, 3);
				break;
			case STOP_ACQUIRE:
				phase = 0;
				byte stopAcquireAck[3];
				stopAcquireAck[0] = 'T';
				stopAcquireAck[1] = 'S';
				stopAcquireAck[2] = STOP_ACQUIRE_ACK;

				Serial.write(stopAcquireAck, 3);
				break;
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
					long time;
					memcpy(&time, timeBuffer, sizeof(time));

					pingBuffer[indexSyncBuffer] = (receptionTime - startSyncTime);
					correctionBuffer[indexSyncBuffer] = (long)receptionTime - (long)time;
					indexSyncBuffer++;

					if (indexSyncBuffer >= SYNC_ATTEMPTS)
					{
						//long correctionMean = 0;
						//unsigned long pingMean = 0;

						//sortPings();

						correctionMean = 0;
						pingMean = 0;

						for (int i = 0; i < SYNC_ATTEMPTS; i++)
						{
							correctionMean += correctionBuffer[i];
							pingMean += pingBuffer[i];
						}

						correctionMean /= SYNC_ATTEMPTS;
						pingMean /= SYNC_ATTEMPTS;
						pingMean /= 2;

						long receiverCorrection = correctionMean - pingMean;

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

						phase = 0;
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

					phase = 0;
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
			startSyncTime = micros();
			Serial.write(syncpkt, 4);
			sent = true;
		}
		else
		{
			if (micros() - startSyncTime > SYNC_TIMEOUT_MICROS)
			{
				sent = false;
			}
		}
		break;
	case SENDDATA:
		//timetosend = read_from_buffer();
		timetosend = micros() + correction;
		if ((analogRead(0) >= 500 && waitHigh) || (analogRead(0) < 500 && waitLow))
		{
			byte msgData[7];
			msgData[0] = 'T';
			msgData[1] = 'S';
			msgData[2] = DATA;
			memcpy(msgData + 3, &timetosend, sizeof(timetosend));
			Serial.write(msgData, 7);

			memset(msgData, 0, 7);
			msgData[0] = 'T';
			msgData[1] = 'S';
			msgData[2] = 0x30;
			memcpy(msgData + 3, &pingMean, sizeof(pingMean));
			Serial.write(msgData, 7);

			waitHigh = !waitHigh;
			waitLow = !waitLow;
		}
		break;
	default:
		break;
	}
}

void incomingData()
{
	if (phase == SENDDATA)
		store_in_buffer(micros() + correction);
}