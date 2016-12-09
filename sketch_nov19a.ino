#include <SPI.h>
#include "SX1272.h"
#include "DHT.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

// Define section
#define BAND868 //900, 433
#define MAX_NB_CHANNEL 9
#define STARTING_CHANNEL 10
#define ENDING_CHANNEL 18
uint8_t loraChannelIndex = 0;
uint32_t loraChannelArray[MAX_NB_CHANNEL] = {CH_10_868, CH_11_868, CH_12_868, CH_13_868, CH_14_868, CH_15_868, CH_16_868, CH_17_868, CH_18_868};
#define LORAMODE  1 //Mode
#define LORA_ADDR 5 //Self address
#define DEFAULT_DEST_ADDR 25 //Gateway address
#define DHTPIN 22 // номер пина, к которому подсоединен датчик температуры и влажности

#define FLUSHOUTPUT               Serial.flush();

//Variables
const int but = 7;         //кнопка
const int dat_air = 8;     //датчик воздуха 
const int rd = 4;          //светодиод
int dest_addr = DEFAULT_DEST_ADDR;
char cmd[260] = "Hello hackathon";          // answer to RPi. Represents button state. 0 - free, 1 - reserved
const int SIGNAL_LED = 13;    // L-led on board
bool radioON = false;
uint8_t loraMode = LORAMODE;
uint32_t loraChannel = loraChannelArray[loraChannelIndex];
char loraPower = 'x'; //innitial poser level, M (maximum), H (high), L (low)
uint8_t loraAddr = LORA_ADDR;
unsigned int inter_pkt_time = 1000; //Time between sending
unsigned int random_inter_pkt_time = 0;
long last_periodic_sendtime = 0;
int ST = 0;

int OS=0;
int S=0;

unsigned long timeb;
unsigned long timen;

DHT dht(DHTPIN, DHT11);  //датчик температуры воздуха

// Configure LoRa tranciever
void startConfig() {
  int e;
  // Set transmission mode and print the result
  e = sx1272.setMode(loraMode);
  // Select frequency channel
  if (loraMode == 11) {
    e = sx1272.setChannel(CH_18_868);
  }
  else {
    e = sx1272.setChannel(loraChannel);
  }
  // Select output power (Max, High or Low)
  e = sx1272.setPower(loraPower);
  // get preamble length
  e = sx1272.getPreambleLength();
  // Set the node address and print the result
  sx1272._nodeAddress = loraAddr;
  e = 0;
}

void setup() {
  int e;
  //Add our code here
  pinMode(A0, INPUT);
  dht.begin();
  Serial.begin(38400);
  // Power ON the module
  e = sx1272.ON();
  e = sx1272.getSyncWord();
  if (!e) {
    radioON = true;
    startConfig();
  }
  FLUSHOUTPUT;
  delay(1000);
  pinMode(dat_air, INPUT);
  pinMode(but, INPUT);
  pinMode(rd, OUTPUT);
  Serial.println("setup finished");
}

void loop() {
  int e=1;
  OS=S;
    
  if (digitalRead(but) == 0) {
   timen = millis();
   unsigned long res;
   res = timen - timeb;     
   if(OS==2){
    if ( res > 30000){              //ВРЕМЯ ПОЛИВА
      S=0;      //если кнопка была нажата ранее, то возврат в начальное состояние при отпускании 
    }
   }        
  } else {                //если нажата
    timeb = millis();
    if(OS==0||OS==1){       
      S=2;
      }
  }

{
    if (inter_pkt_time)
      if (millis() - last_periodic_sendtime > (random_inter_pkt_time ? random_inter_pkt_time : inter_pkt_time)) {
        sx1272.CarrierSense();
        long startSend = millis();
        int id_place = 1; //id парковочного места и собственно ардуино

        int radix = 10;  //система счисления
        char izm[15]; //результат
        char *ta;  //указатель на результат
        ta = itoa(id_place, izm, radix);
        
        int h = dht.readHumidity();
        char hum[3]; //результат
        char *r;  //указатель на результат
        r = itoa(h, hum, radix);

        // Считываем температуру
        int t = dht.readTemperature();
        char tem[3]; //результат
        char *p;  //указатель на результат
        p = itoa(t, tem, radix);

        // Проверка удачно прошло ли считывание.
        if (isnan(h) || isnan(t)) {
        Serial.println("Не удается считать показания");
        return;
        }

        int hs = analogRead(A0); //take a sample
        char hus[4]; //результат
        
        char *sa;  //указатель на результат
        sa = itoa(hs, hus, radix);
        int len = strlen(hus);
        
        strcat(izm, "$");   //конкатинация строк
        if (strlen(tem) == 1)  strcat(izm, "0");
        strcat(izm, tem);   //конкатинация строк
        strcat(izm, "$");   //конкатинация строк
        if (strlen(hum) == 1)  strcat(izm, "00");
        if (strlen(hum) == 2)  strcat(izm, "0");
        strcat(izm, hum);   //конкатинация строк
        strcat(izm, "$");   //конкатинация строк
        if (strlen(hus) == 1)  strcat(izm, "000");
        if (strlen(hus) == 2)  strcat(izm, "00");
        if (strlen(hus) == 3)  strcat(izm, "0");
        strcat(izm, hus);   //конкатинация строк
        strcat(izm, "$");
        if (S == 2) strcat(izm, "1"); else strcat(izm, "0");

        char* str ;
        if (S == 0 || S == 2)str = izm;
        if (S == 1)str = "1";

        if (S == 0) digitalWrite(rd, LOW);                     //показания светодиода 
        if (S == 1 || S == 2) digitalWrite(rd, HIGH);

        e = sx1272.sendPacketTimeout(dest_addr, (uint8_t*) str, strlen(str), 10000);   // send packet
        if (random_inter_pkt_time) {
          random_inter_pkt_time = random(100, inter_pkt_time);
        }
        last_periodic_sendtime = millis();
        Serial.print(" sended ");
        Serial.println(str);
        }

    uint16_t w_timer = 1000;
    if (loraMode == 1)
      w_timer = 1000;                          //пакет идёт раз в секунду
      e = sx1272.receivePacketTimeout(1000);

    // if packet was received?
    if (!e) {           //Действия при приёме пакета
      int a = 0, b = 0;
      uint8_t tmp_length;

      sx1272.getSNR();
      sx1272.getRSSIpacket();
      tmp_length = sx1272._payloadlength;
      Serial.println(tmp_length);
      if (tmp_length) {
        if (sx1272.packet_received.data[0] == 48)ST = 0;
        if (sx1272.packet_received.data[0] == 49)ST = 1;
        if(S==0||S==1){S=1;OS=1;}
        Serial.print("got");
        Serial.println(sx1272.packet_received.data[0]);
      }
    }
  }
}           //unsigned long time; time = millis(); - функция для получения времени с момента запуска ардуины 
