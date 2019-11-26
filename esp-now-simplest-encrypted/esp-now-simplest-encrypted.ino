#define SLEEP_SECS 5

#define SEND_TIMEOUT 100
#define PIN_MODE D1 //pin low = sender mode

extern "C" {
  #include <espnow.h>
}
#include <ESP8266WiFi.h>

static uint8_t KEY_LEN = 16;
//uint8_t key[16]= {0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44};
//static uint8_t KEY_LEN = 4;
static char KEY_STRING[] = "Koivu8inna";

uint8_t key[16];
//= {1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0};
#define WIFI_CHANNEL 1
bool MODE_SENDER = 1;
bool incoming = false;

uint8_t *populateKey(uint8_t *keyBuf, uint8_t *mac, char *keyStr, uint8_t keyLen = KEY_LEN) {
  uint8_t keyStrLen = strlen(keyStr);
  static uint8_t macLen = 6;
  for (uint32_t i = 0; i < keyLen; i++) {
    keyBuf[i] = keyStr[(i % keyStrLen) ^ (i % macLen)];
  }
  return keyBuf;
}

uint8_t newMac[] = {0x36, 0x33, 0x33, 0x33, 0x34, 0x39};

struct __attribute__((packed)) MESSAGE_DATA {
  uint32_t value;
  bool original;
} messageDataIn, messageDataOut;

void initVariant() {
  pinMode(PIN_MODE, INPUT_PULLUP);  
  MODE_SENDER = digitalRead(PIN_MODE); 
  pinMode(PIN_MODE, INPUT); //No need to heat the internal pull-up resistor anymore

  WiFi.mode(WIFI_AP);
//  WiFi.softAP("ESPNOW", nullptr, 3);
//  WiFi.softAPdisconnect(false);
  if (MODE_SENDER)
    wifi_set_macaddr(SOFTAP_IF, &newMac[0]);
  else
    wifi_set_macaddr(SOFTAP_IF, &newMac[0]);
}

uint8_t bs[sizeof(messageDataOut)];

uint8_t sendEspNow(uint32_t val, bool original = true) {
    messageDataOut.value = val;
    messageDataOut.original = original;
    Serial.printf("Sending %u at %u\n",messageDataOut.value,micros());
    memcpy(bs, &messageDataOut, sizeof(messageDataOut));
    esp_now_send(NULL, bs, sizeof(messageDataOut));
}



/////////////////////////////////////////////////////////////////////////////

void gotoSleep() {
  Serial.printf("Awake for %i ms, going to sleep for %i secs...\n", millis(), SLEEP_SECS);
  ESP.deepSleepInstant(SLEEP_SECS * 1000000, RF_NO_CAL);
}

/////////////////////////////////////////////////////////////////////////////
bool received = false;

uint32_t sentTime = 0;

void sendCallBackFunction(uint8_t *mac, uint8_t status) {
  sentTime = micros();
  Serial.printf("\nSent message at %u status %u receiver mac",sentTime, status);
  for (uint8_t i = 0; i < 6; i++) {
    Serial.printf("%x ", mac[i]);
  }
  Serial.println("");
//  gotoSleep();
}

void receiveCallBackFunction(uint8_t *senderMac, uint8_t *incomingData, uint8_t len) {
  Serial.printf("\nNewmsg at %u len %u mac ",micros(),len,senderMac);
  for (uint8_t i = 0; i < 6; i++) {
    Serial.printf("%x ", senderMac[i]);
  }
  Serial.println("");
  memcpy(&messageDataIn, incomingData, len);
  Serial.println(messageDataIn.value);
  incoming = true;
  if (messageDataIn.original) {
    Serial.printf("Received value %u\n",messageDataIn.value);
    sendEspNow(messageDataIn.value + 1, 0);
  } else {
    Serial.printf("Received reply %u, loopback time %u\n",messageDataIn.value, micros() - sentTime);
    sentTime = 0;
  }
}

/////////////////////////////////////////////////////////////////////////////

void setup() {
    Serial.begin(115200);
    uint8_t keyBuf[KEY_LEN];
    populateKey(keyBuf, newMac, KEY_STRING);
    if (esp_now_init() != 0) {
        Serial.println(F("ESP_Now init failed"));
        gotoSleep();
    }
    populateKey(keyBuf, newMac, KEY_STRING);
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(newMac, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, keyBuf, KEY_LEN);
    bool sendStatus = esp_now_register_send_cb(sendCallBackFunction);
    messageDataIn.value = -1;
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(receiveCallBackFunction);
    if(MODE_SENDER) {
    Serial.println(F("\nSender mode"));
  } else {
    Serial.println(F("\nReceiver mode"));
  }
}

void loop() {
  sendEspNow(random(0,65536));
  delay(random(3000,10000));
}
