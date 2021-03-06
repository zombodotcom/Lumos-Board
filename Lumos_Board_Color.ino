#include "FastLED.h"

FASTLED_USING_NAMESPACE
using namespace std;
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "StringSplitter.h"
#include <cstring>
#include <cstdlib>
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

//led stuff
#define DATA_PIN   27
//#define CLK_PIN   4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    300
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
String pattern;
String pat;
String sBrightFix;
int brightFixs;
String rxColorFix;
String sColorfix1;
String sColorFix;
uint8_t rgbR=128;
uint8_t rgbG=128;
uint8_t rgbB=128;
CRGB solidColor=CRGB::Blue;


#define FASTLED_SHOW_CORE 0

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

/** show() for ESP32
    Call this function instead of FastLED.show(). It signals core 0 to issue a show,
    then waits for a notification that it is done.
*/
void FastLEDshowESP32()
{
  if (userTaskHandle == 0) {
    // -- Store the handle of the current task, so that the show task can
    //    notify it when it's done
    userTaskHandle = xTaskGetCurrentTaskHandle();

    // -- Trigger the show task
    xTaskNotifyGive(FastLEDshowTaskHandle);

    // -- Wait to be notified that it's done
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    userTaskHandle = 0;
  }
}

/** show Task
    This function runs on core 0 and just waits for requests to call FastLED.show()
*/
void FastLEDshowTask(void *pvParameters)
{
  // -- Run forever...
  for (;;) {
    // -- Wait for the trigger
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // -- Do the show (synchronously)
    FastLED.show();

    // -- Notify the calling task
    xTaskNotifyGive(userTaskHandle);
  }
}

void setSolidColor(uint8_t x, uint8_t y, uint8_t z, String sColorFix)
{
  StringSplitter *splitter = new StringSplitter(sColorFix, ',', 3);
  int itemCount = splitter->getItemCount();
  Serial.println(itemCount);
  int rgb[itemCount];

  for (int i = 0; i < itemCount; i++) {
    String item = splitter->getItemAtIndex(i);
    rgb[i] = atoi(item.c_str());
    Serial.println(rgb[i]);
  }
  rgbR=rgb[0];
  rgbG=rgb[1];
  rgbB=rgb[2];
}
void brightFix(String value) {
  int bFix1;
  value.remove(0, 11);
  bFix1 = value.toInt();
  FastLED.setBrightness(bFix1);
}
String setPattern(String pat) {
  pattern = pat;
  return pattern;
}
void pride()
{

  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
}



class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;


    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }

        Serial.println();
        Serial.println("*********");
      }
      // Do stuff based on the command received from the app
      // For some reason using rxValue.compare("A") == 0 doesn't work. Maybe
      // there are hidden characters I'm not seeing?
      if (rxValue.find("rainbow") != -1) {
        Serial.println("Turning ON!");
        pat = "rainbow";
        setPattern(pat);

      }

        if (rxValue.find("brightness") != -1) {
      sBrightFix = (rxValue.c_str());
      brightFix(sBrightFix);
    }
    
      if (rxValue.find("juggle") != -1) {
        Serial.println("Turning ON!");
        pat = "juggle";
        setPattern(pat);

      }
      if (rxValue.find("sinelon") != -1) {
        Serial.println("Turning ON!");
        pat = "sinelon";
        setPattern(pat);

      }
      if (rxValue.find("bpm") != -1) {
        Serial.println("Turning ON!");
        pat = "bpm";
        setPattern(pat);

      }

      if (rxValue.find("pride") != -1) {
        Serial.println("Turning ON!");
        pat = "pride";
        setPattern(pat);

      }

     if (rxValue.find("color") != -1) {
      sColorFix = (rxValue.c_str());
      sColorFix.trim();
      sColorFix.replace("color,", "");
      sColorFix.replace(" ", "");
      setSolidColor(rgbR,rgbG,rgbB,sColorFix);
      Serial.println(rgbR);
      pat = "color";
      setPattern(pat);
    }

      else if (rxValue.find("off") != -1) {
      Serial.println("Turning OFF!");
        pat = "off";
        setPattern(pat);
      }


    }
};

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  // Create the BLE Device
  BLEDevice::init("Lumos Board"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
int core = xPortGetCoreID();
  Serial.print("Main code running on core ");
  Serial.println(core);

  // -- Create the FastLED show task
  xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);
}

void loop() {
  EVERY_N_MILLISECONDS( 20 ) {
    gHue++;  // slowly cycle the "base color" through the rainbow
  }

  if (deviceConnected) {


    // Fabricate some arbitrary junk for now...
    if (pattern == "rainbow") {
//      FastLED.show();
      fill_rainbow( leds, NUM_LEDS, gHue, 7);

    }
    if (pattern == "off") {
//      FastLED.show();
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }

    if (pattern == "sinelon") {
//      FastLED.show();
      sinelon();
    }
    if (pattern == "bpm") {
//      FastLED.show();
      bpm();
    }
    if (pattern == "juggle") {
//      FastLED.show();
      juggle();
    }

    if (pattern == "pride") {
//      FastLED.show();
      pride();
    }
if (pattern == "color") {
      FastLED.show();
      fill_solid(leds, NUM_LEDS, CRGB(rgbR,rgbG,rgbB));
    }

  }
  
  if (!deviceConnected) {
    FastLED.show();
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  }
  
FastLEDshowESP32();
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16( i + 7, 0, NUM_LEDS - 1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

