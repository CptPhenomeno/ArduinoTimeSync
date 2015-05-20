//Type of message
const byte ELECT				= 0x10;
const byte ELECT_ACK			= 0x11;
const byte SYNC					= 0x12;
const byte SYNC_ACK				= 0x13;
const byte SYNC_CORR			= 0X14;
const byte DATA					= 0X20;
const byte START_ACQUIRE		= 0x21;
const byte STOP_ACQUIRE			= 0x22;
const byte START_ACQUIRE_ACK	= 0x23;
const byte STOP_ACQUIRE_ACK		= 0x24;

//Automata status
const byte SYNC_PHASE		= 0xA0;
const byte SEND_DATA_PHASE	= 0XA1;
const byte SILENCE			= 0xA3;

const byte SYNC_ATTEMPTS = 40;

const unsigned long SYNC_TIMEOUT_MICROS = 5000000;



long double x[SYNC_ATTEMPTS];
long double y[SYNC_ATTEMPTS];
long double results[2] = { 0, 0 };

long pingBuffer[SYNC_ATTEMPTS];
long correctionBuffer[SYNC_ATTEMPTS];

uint8_t seqnumSync = 0;

boolean sent = false;
boolean waitLow = false;
boolean waitHigh = true;

long double angular_coefficient = 0;
long double linear_factor = 0;
long startTimeSync = 0;

long timetosend = 0;

byte indexSyncBuffer;

byte phase = SILENCE;

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
			byte checknum;
			Serial.readBytes(&msgType, 1);
			switch (msgType)
			{
			case START_ACQUIRE:
				phase = SEND_DATA_PHASE;
				byte startAcquireAck[3];
				startAcquireAck[0] = 'T';
				startAcquireAck[1] = 'S';
				startAcquireAck[2] = START_ACQUIRE_ACK;

				Serial.write(startAcquireAck, 3);
				break;
			case STOP_ACQUIRE:
				phase = SILENCE;
				byte stopAcquireAck[3];
				stopAcquireAck[0] = 'T';
				stopAcquireAck[1] = 'S';
				stopAcquireAck[2] = STOP_ACQUIRE_ACK;

				Serial.write(stopAcquireAck, 3);
				break;
			case ELECT:
				char mac[13];
				Serial.readBytes(&checknum, 1);
				Serial.readBytes(mac, 12);
				byte ack[4];
				ack[0] = 'T';
				ack[1] = 'S';
				ack[2] = ELECT_ACK;
				ack[3] = checknum;
				Serial.write(ack, 4);
				delay(500);
				//TODO
				goInCommandMode();
				killConnection();
				configureMaster();
				reboot();
				goInCommandMode();
				clearSerial();

				connectTo(mac);
				exitFromCommandMode();
				phase = SYNC_PHASE;
				indexSyncBuffer = 0;
				break;
			case SYNC:
				Serial.readBytes(&checknum, 1);
				seqnumSync = checknum;
				byte syncackpkt[8];
				syncackpkt[0] = 'T';
				syncackpkt[1] = 'S';
				syncackpkt[2] = SYNC_ACK;
				syncackpkt[3] = checknum;
				memcpy(syncackpkt + 4, &receptionTime, sizeof(receptionTime));
				Serial.write(syncackpkt, 8);
				break;
			case SYNC_ACK:
				Serial.readBytes(&checknum, 1);
				if (checknum == seqnumSync)
				{
					if (indexSyncBuffer == 0)
						startTimeSync = receptionTime;
					byte timeBuffer[4];
					Serial.readBytes(timeBuffer, 4);
					long time;
					memcpy(&time, timeBuffer, sizeof(time));

					pingBuffer[indexSyncBuffer] = (receptionTime - startSyncTime);

					if (pingBuffer[indexSyncBuffer] > 70000){
						sent = false;
						break;
					}

					correctionBuffer[indexSyncBuffer] = (long)receptionTime - (long)time;
					x[indexSyncBuffer] = receptionTime - startTimeSync;
					y[indexSyncBuffer] = (correctionBuffer[indexSyncBuffer] - pingBuffer[indexSyncBuffer]/2);
			
					delay(500);

					indexSyncBuffer++;


					if (indexSyncBuffer >= SYNC_ATTEMPTS )
					{
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

						simpLinReg(x, y, results, SYNC_ATTEMPTS);

						memset(timeBuffer, 0, 4);
						memcpy(timeBuffer, &receiverCorrection, sizeof(receiverCorrection));

						byte msgCorrection[20];
						msgCorrection[0] = 'T';
						msgCorrection[1] = 'S';
						msgCorrection[2] = SYNC_CORR;
						msgCorrection[3] = checknum;
						memcpy(msgCorrection + 4, &results[0], sizeof(results[0]));
						memcpy(msgCorrection + 12, &results[1], sizeof(results[1]));


						angular_coefficient = results[0];
						linear_factor = results[1];
						
						delay(1000);

						goInCommandMode();
						killConnection();
						configureSlave();
						exitFromCommandMode();

						phase = SILENCE;
					}
					else
					{
						sent = false;
					}
				}
				break;
			case SYNC_CORR:
				Serial.readBytes(&checknum, 1);
				if (checknum == seqnumSync)
				{
					byte timeBuffer[8];
					Serial.readBytes(timeBuffer, 8);
					memcpy(&angular_coefficient, timeBuffer, sizeof(angular_coefficient));
					Serial.readBytes(timeBuffer, 8);
					memcpy(&linear_factor, timeBuffer, sizeof(linear_factor));

					angular_coefficient = 1 / angular_coefficient;
					linear_factor = -linear_factor;
					phase = SILENCE;
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

	long double tmp = 0;
	long double tmp2 = 0;

	switch (phase)
	{
	case SYNC_PHASE:
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
				blinkDebug(10, 100);
				sent = false;
			}
		}
		break;
	case SEND_DATA_PHASE:
		//timetosend = read_from_buffer();
		tmp = (micros() - startTimeSync);
		tmp2 = angular_coefficient*tmp;
		timetosend = micros() - (tmp2 + linear_factor);
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
