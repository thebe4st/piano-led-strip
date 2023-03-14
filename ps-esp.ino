/**
  FP-30:0xF65221A20B50
*/
#include <FastLED.h>
#include <Arduino.h>
#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_Client_ESP32.h>

// 调色板
// #define CP HeatColors_p
// #define CP cold_p
#define CP warm_p

#define LED_PIN 18
#define NUM_LEDS 144
#define BLINK_WIDTH 2

#define BLACK CRGB(0, 0, 0)

#define REDUCE_LIGHT 0.2
#define REDUCE_LIGHT_INTERVAL 15

// 定义我的灯带放置在钢琴的哪个起始位置
#define KEY_START 30
#define KEY_END 101

// ESP32必须初始化为 115200
#define SERIAL_FREQ 115200


CRGBPalette16 warm_p = CRGBPalette16(
                                CRGB(0xF5,0x6B,0x5B),  CRGB(0xD4,0x59,0x20),  CRGB(0xeb,0x93,0x30),  CRGB(0xd1,0xac,0x56),CRGB(0xf9,0xdf,0x55), 
                                CRGB(0xF5,0x6B,0x5B),  CRGB(0xD4,0x59,0x20),  CRGB(0xeb,0x93,0x30),  CRGB(0xd1,0xac,0x56),CRGB(0xf9,0xdf,0x55), 
                                CRGB(0xF5,0x6B,0x5B),  CRGB(0xD4,0x59,0x20),  CRGB(0xeb,0x93,0x30),  CRGB(0xd1,0xac,0x56),CRGB(0xf9,0xdf,0x55), 
                                CRGB(0xf9,0xdf,0x55));

CRGBPalette16 cold_p = CRGBPalette16(
                                CRGB(0xFa,0x70,0xec),  CRGB(0x8b,0x48,0xdb),  CRGB(0x50,0x67,0xeb),  CRGB(0x3b,0xa9,0xd1),CRGB(0x46,0xfa,0xbc), 
                                CRGB(0xFa,0x70,0xec),  CRGB(0x8b,0x48,0xdb),  CRGB(0x50,0x67,0xeb),  CRGB(0x3b,0xa9,0xd1),CRGB(0x46,0xfa,0xbc), 
                                CRGB(0xFa,0x70,0xec),  CRGB(0x8b,0x48,0xdb),  CRGB(0x50,0x67,0xeb),  CRGB(0x3b,0xa9,0xd1),CRGB(0x46,0xfa,0xbc), 
                                CRGB(0xFa,0x70,0xec));

struct LED {
  int pos;     // 该灯珠的颜色在调色板中的位置
  bool press;  // 当前按键是否按下
  int light;   // 当前亮度
};

BLEMIDI_CREATE_INSTANCE("FP-30", MIDI)

CRGB leds[NUM_LEDS];  // FastLED 的数，关联灯带
LED mLeds[NUM_LEDS];  // 自定义的LED数组，关联状态

CRGB genColorByLED(LED led) {
  CRGB ret = ColorFromPalette(CP, led.pos, led.light, LINEARBLEND);
  return ret;
}

// 使自定义的LED灯带数组生效
void applyLedSetting() {
  for (int i = 0; i < NUM_LEDS; i++) {
    LED l = mLeds[i];
    if (l.light <= 0) {
      leds[i] = BLACK;
      continue;
    }
    leds[i] = genColorByLED(l);
  }
  FastLED.show();
}

unsigned long t0 = millis();
unsigned long t1 = millis();
bool isConnected = false;

void ReadCB(void *parameter) {
  for (;;) {
    MIDI.read();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  vTaskDelay(1);
}

void mapLED(int note, int velocity, bool press) {
  // 不处理超出边界的
  if(note < KEY_START || note > KEY_END) {
    return;
  }
  long v = map(velocity, 0, 100, 0, 255) * REDUCE_LIGHT;
  long pos = map(note, KEY_START, KEY_END, 0 + BLINK_WIDTH, NUM_LEDS - 1 - BLINK_WIDTH);
  int cPos = rand() % 255;
  for (int i = 0; i <= BLINK_WIDTH; i++) {
    if (press) {
      // 仅当它不亮的时候才换颜色
      if (mLeds[pos + i].light <= 0) {
        mLeds[pos + i].pos = cPos;
      }
      if (mLeds[pos - i].light <= 0) {
        mLeds[pos - i].pos = cPos;
      }
      mLeds[pos + i].light = v;
      mLeds[pos - i].light = v;
    }
    mLeds[pos + i].press = press;
    mLeds[pos - i].press = press;
  }
}
void setup() {
  Serial.begin(SERIAL_FREQ);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  BLEMIDI.setHandleConnected([]() {
    Serial.println("---------CONNECTED---------");
    isConnected = true;
  });

  BLEMIDI.setHandleDisconnected([]() {
    Serial.println("---------NOT CONNECTED---------");
    isConnected = false;
  });

  MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
    Serial.print("NoteON: CH: ");
    Serial.print(channel);
    Serial.print(" | ");
    Serial.print(note);
    Serial.print(", ");
    Serial.println(velocity);

    mapLED(note, velocity, true);
    applyLedSetting();
  });
  MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
    mapLED(note, velocity, false);
    applyLedSetting();
  });

  xTaskCreatePinnedToCore(ReadCB,
                          "MIDI-READ",
                          3000,
                          NULL,
                          1,
                          NULL,
                          1);  //Core0 or Core1

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
}

void loop() {
  if (isConnected && (millis() - t0) > 1000) {
    t0 = millis();
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
  if (isConnected && (millis() - t1) > REDUCE_LIGHT_INTERVAL) {
    t1 = millis();
    // 循环一下 减少松开按钮的亮度
    for (int i = 0; i < NUM_LEDS; i++) {
      if (!mLeds[i].press && mLeds[i].light > 0) {
        mLeds[i].light--;
      }
    }
    applyLedSetting();
  }
}