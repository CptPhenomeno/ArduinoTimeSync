#include <SPI.h>
#include <SD.h>

const byte ELECT		= 0x10;
const byte ELECTACK		= 0x11;
const byte SYNC			= 0x12;
const byte SYNCACK		= 0x13;

const byte SYNCPHASE = 0xA0;
const byte ACQUISITION = 0xA1;

const unsigned long SYNC_TIMEOUT_MILLIS = 5000000;

File sdFile;

uint8_t sendBuffer[20];
uint8_t receiveBuffer[20];
uint8_t seqnumSync = 0;
size_t size = 0;

boolean sent = false;
boolean master = false;

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

void findDevice()
{
	Serial.println("IS10");
	delay(10500);
	String test = Serial.readString();
	Serial.print("Ho trovato ");
	Serial.println(test);
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

/*
void buildPacket(uint8_t * buffer, size_t * size){
	*buffer = 'T';
	*(buffer + 1) = 'S';
	*(buffer + 2) = PING;
	*(buffer + 3) = seq;
	*(buffer + 4) = 0x0;
	*(buffer + 5) = 0x0;
	*(buffer + 6) = 0x0;
	*(buffer + 7) = 0x0;
	*size = 8;
	return;
}

void buildReply(uint8_t * buffer, size_t * size, byte seqnumber){
	*buffer = 's';
	*(buffer + 1) = PINGREPLY;
	*(buffer + 2) = seqnumber;
	unsigned long d = micros();

	Serial.print("time inviato ");
	memcpy((buffer + 3), &d, 4);
	*size = 7;
}*/

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



/*
void serialEvent(){
	uint8_t *ptr;
	endtime = micros();
	ptr = receiveBuffer;


	size = Serial.readBytes(receiveBuffer, 1);
	if (*ptr != 's') return;
	size = Serial.readBytes(receiveBuffer, 6);
	if (size != 6) return;
	switch (*ptr)
	{
	case PING:
		buildReply(sendBuffer, &size, *(ptr + 1));
		Serial.write(sendBuffer, 7);
		break;
	case PINGREPLY:
		if (*(ptr + 1) == seq){
			seq++;
			ptr += 2;
			memcpy(&timestamp, ptr, 4);
			Serial.print("time ricevuto : ");
			Serial.println(timestamp);
			Serial.println(endtime);
			Serial.println(starttime);
			sent = false;
		}

	default:
		break;
	}

}*/

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
					phase = SYNCPHASE;
					exitFromCommandMode();
					break;
				case SYNC:
					Serial.readBytes(&seqnum, 1);
					byte syncackpkt[8];
					syncackpkt[0] = 'T';
					syncackpkt[1] = 'S';
					syncackpkt[2] = SYNCACK;
					syncackpkt[3] = seqnum;
					memcpy(syncackpkt + 4, &receptionTime, sizeof(receptionTime));
					Serial.write(syncackpkt, 8);
					digitalWrite(LED_BUILTIN, HIGH);					
					break;
				case SYNCACK:
					Serial.readBytes(&seqnum, 1);
					if (seqnum == seqnumSync)
					{
						byte timeBuffer[4];
						Serial.readBytes(timeBuffer, 4);
						unsigned long time;
						memcpy(&time, timeBuffer, sizeof(time));

						correction =  (time + ((receptionTime - startSyncTime) / 2)) - receptionTime;

						goInCommandMode();
						configureSlave();
						killConnection();

						phase = 0;
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
		if (!sent)
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