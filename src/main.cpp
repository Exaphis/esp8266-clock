#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <Timezone.h>
#include <sntp.h>
#include <time.h>

#include "clock_display.h"
#include "secrets.h"

#define BLACK (uint16_t)0
#define TIME_COLOR (uint16_t)0xFFFF

#define PROGRESS_COLOR_1_AM (uint16_t)0xA243
#define PROGRESS_COLOR_2_AM (uint16_t)0x8C84

#define PROGRESS_COLOR_1_PM (uint16_t)0x999A
#define PROGRESS_COLOR_2_PM (uint16_t)0x4EFA

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"
#define NTP_SERVER_3 "time.google.com"

#define NTP_DEFAULT_UPDATE_INTERVAL (60 * 1000)

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define MATRIX_PIN D1
#define MATRIX_BRIGHTNESS 10

CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
FastLED_NeoMatrix matrix(leds, MATRIX_WIDTH, MATRIX_HEIGHT,
                         NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
ClockDisplay display(matrix);

TimeChangeRule us_pdt = {"PDT", Second, Sun, Mar, 2, -420};  //UTC -7 hours
TimeChangeRule us_pst = {"PST", First, Sun, Nov, 2, -480};   //UTC -8 hours
Timezone us_pacific(us_pdt, us_pst);

unsigned long last_ntp_update = 0;
unsigned long ntp_update_interval = NTP_DEFAULT_UPDATE_INTERVAL;

void setup() {
    Serial.begin(115200);
    Serial.println();

    FastLED.addLeds<WS2812B, MATRIX_PIN, GRB>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);

    matrix.fillScreen(0);
    matrix.setBrightness(MATRIX_BRIGHTNESS);
    FastLED.delay(2000);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting");

    int loading_x = 0;
    int loading_y = 0;

    // http://mathertel.blogspot.com/2018/10/robust-esp8266-network-connects.html
    // wait max. 30 seconds for connecting
    wl_status_t wifi_status;
    unsigned long maxTime = millis() + (30 * 1000);

    while (true) {
        matrix.fillScreen(0);
        matrix.drawPixel(loading_x, loading_y, TIME_COLOR);
        matrix.show();

        // move left->right, up->down
        loading_x += 1;
        loading_y += loading_x / MATRIX_WIDTH;
        loading_y %= MATRIX_HEIGHT;
        loading_x %= MATRIX_WIDTH;

        wifi_status = WiFi.status();
        Serial.printf("(%d).", wifi_status);

        if ((wifi_status == WL_CONNECTED) || (wifi_status == WL_NO_SSID_AVAIL) ||
            (wifi_status == WL_CONNECT_FAILED) || (millis() >= maxTime))
            break;  // exit this loop

        FastLED.delay(100);
    }
    Serial.printf("(%d)\n", wifi_status);

    if (WiFi.status() != WL_CONNECTED) {
        ESP.restart();
    }

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    // ensure that NTP will update immediately on first loop
    last_ntp_update = (unsigned long)-ntp_update_interval;
}

void loop() {
    // update local time from NTP if needed
    if (last_ntp_update + ntp_update_interval < millis()) {
        last_ntp_update = millis();

        Serial.print("updating ntp... ");
        configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
        Serial.print("took ");
        Serial.print(millis() - last_ntp_update);
        Serial.println(" ms");

        FastLED.delay(200);  // ntp takes time to update??
    }

    time_t local_time = us_pacific.toLocal(time(0));
    display.drawTime(hour(local_time), minute(local_time));

    FastLED.delay(200);
}