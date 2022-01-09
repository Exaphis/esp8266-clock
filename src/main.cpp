#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <TZ.h>
#include <coredecls.h>

#include "clock_display.h"
#include "secrets.h"

#define BLACK (uint16_t)0
#define TIME_COLOR (uint16_t)0xFFFF

#define PROGRESS_COLOR_1_AM (uint16_t)0xA243
#define PROGRESS_COLOR_2_AM (uint16_t)0x8C84

#define PROGRESS_COLOR_1_PM (uint16_t)0x999A
#define PROGRESS_COLOR_2_PM (uint16_t)0x4EFA

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define MATRIX_PIN D1
#define MATRIX_BRIGHTNESS 10

CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
FastLED_NeoMatrix matrix(leds, MATRIX_WIDTH, MATRIX_HEIGHT,
                         NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
ClockDisplay display(matrix);

bool time_is_set = false;

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000() {
    return 60 * 1000;  // 60s
}

void sntp_time_set(bool from_sntp) {
    if (from_sntp) {
        Serial.println("Time set from SNTP!");
        time_is_set = true;
    }
}

void setup_ntp() {
    Serial.print("Setting time using SNTP...");
    unsigned long time_start_millis = millis();

    settimeofday_cb(sntp_time_set);
    configTime(TZ_America_Los_Angeles, "pool.ntp.org", "time.nist.gov", "time.windows.com");

    while (!time_is_set) {
        FastLED.delay(100);
    }

    Serial.print("took ");
    Serial.print(millis() - time_start_millis);
    Serial.println(" ms");
}

void setup_wifi() {
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
            break;

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
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    FastLED.addLeds<WS2812B, MATRIX_PIN, GRB>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);

    matrix.fillScreen(0);
    matrix.setBrightness(MATRIX_BRIGHTNESS);
    FastLED.delay(2000);

    setup_wifi();
    setup_ntp();
}

void loop() {
    time_t now = time(0);
    struct tm *tm = localtime(&now);
    display.drawTime(tm->tm_hour, tm->tm_min);

    FastLED.delay(200);
}