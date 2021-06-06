#define FASTLED_ALLOW_INTERRUPTS       0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "secrets.h"

#define BLACK                (uint16_t)0
#define TIME_COLOR           (uint16_t)0xFFFF
#define SYNC_FAILED_COLOR    (uint16_t)0xFFFF

#define PROGRESS_COLOR_1_AM  (uint16_t)0xA243
#define PROGRESS_COLOR_2_AM  (uint16_t)0x8C84

#define PROGRESS_COLOR_1_PM  (uint16_t)0x999A
#define PROGRESS_COLOR_2_PM  (uint16_t)0x4EFA

#define NTP_DEFAULT_UPDATE_INTERVAL (60*1000)
#define NTP_FAIL_UPDATE_INTERVAL    (30*1000);

#define MATRIX_WIDTH        32
#define MATRIX_HEIGHT       8
#define MATRIX_LEDS         (MATRIX_WIDTH * MATRIX_HEIGHT)
#define MATRIX_PIN          D1
#define MATRIX_BRIGHTNESS   10

CRGB leds[MATRIX_LEDS];
FastLED_NeoMatrix matrix(leds, MATRIX_WIDTH, MATRIX_HEIGHT,
                         NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG
                        );
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

WiFiUDP ntp_udp;
NTPClient time_client(ntp_udp);

TimeChangeRule us_pdt = {"PDT", Second, Sun, Mar, 2, -420};  //UTC -7 hours
TimeChangeRule us_pst = {"PST", First, Sun, Nov, 2, -480};   //UTC -8 hours
Timezone us_pacific(us_pdt, us_pst);

uint16_t fg_color = -1;
int last_minute = -1;
unsigned long last_ntp_update = 0;
unsigned long ntp_update_interval = NTP_DEFAULT_UPDATE_INTERVAL;

void setup() {
  Serial.begin(115200);
  Serial.println();

  FastLED.addLeds<WS2812B, MATRIX_PIN, GRB>(leds, MATRIX_LEDS);
  u8g2_for_adafruit_gfx.begin(matrix);
  u8g2_for_adafruit_gfx.setForegroundColor(TIME_COLOR);
  u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mn);

  matrix.fillScreen(0);
  matrix.setBrightness(MATRIX_BRIGHTNESS);
  //  matrix.show();
  //
  //  u8g2_for_adafruit_gfx.setForegroundColor(SYNC_FAILED_COLOR);
  //  print_no_slashed_zero("00:00", 4, 7);
  //  drawDayProgress(&matrix, 1, PROGRESS_COLOR_1_AM, PROGRESS_COLOR_2_AM);
  //  drawDayProgress(&matrix, 1, PROGRESS_COLOR_1_PM, PROGRESS_COLOR_2_PM);
  //  matrix.show();
  //  Serial.println("fastled show");
  //  FastLED.delay(0xFFFFF);

  FastLED.delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");

  int loading_x = 0;
  int loading_y = 0;
  while (WiFi.status() != WL_CONNECTED) {
    matrix.fillScreen(0);
    matrix.drawPixel(loading_x, loading_y, WiFi.status() == WL_CONNECT_FAILED ? SYNC_FAILED_COLOR : TIME_COLOR);
    matrix.show();

    // move left->right, up->down
    loading_x += 1;
    loading_y += loading_x / MATRIX_WIDTH;
    loading_y %= MATRIX_HEIGHT;
    loading_x %= MATRIX_WIDTH;

    delay(1000);
    Serial.print(WiFi.status() == WL_CONNECT_FAILED ? "*" : ".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  time_client.begin();
  // do not finish setup if time is not synched
  while (!time_client.forceUpdate());
  fg_color = TIME_COLOR;
}

void rgb565To888(int color, int *out_red, int *out_green, int *out_blue) {
  *out_red = (color & 0xF800) >> 11;
  *out_green = (color & 0x7E0) >> 5;
  *out_blue = (color & 0x1F);

  // https://stackoverflow.com/a/9069480
  *out_red = (*out_red * 527 + 23) >> 6;
  *out_green = (*out_green * 259 + 33 ) >> 6;
  *out_blue = (*out_blue * 527 + 23 ) >> 6;
}

void rgb888To565(int red, int green, int blue, uint16_t* out_color) {
  // https://stackoverflow.com/a/9069480
  red = (red * 249 + 1014) >> 11;
  green = (green * 253 +  505) >> 10;
  blue = (blue * 249 + 1014) >> 11;

  *out_color = (red << 11) | (green << 5) | blue;
}

uint16_t rgb565Fade(uint16_t color_src, uint16_t color_dst, float percent) {
  int src_r = 0;
  int src_g = 0;
  int src_b = 0;
  rgb565To888(color_src, &src_r, &src_g, &src_b);

  int dst_r = 0;
  int dst_g = 0;
  int dst_b = 0;
  rgb565To888(color_dst, &dst_r, &dst_g, &dst_b);

  // https://stackoverflow.com/a/21835834
  int ans_r = round((1.f - percent) * src_r + percent * dst_r);
  int ans_g = round((1.f - percent) * src_g + percent * dst_g);
  int ans_b = round((1.f - percent) * src_b + percent * dst_b);

  uint16_t ans = 0;
  rgb888To565(ans_r, ans_g, ans_b, &ans);
  return ans;
}

void drawDayProgress(FastLED_NeoMatrix* matrix_ptr, float progress, uint16_t src_color, uint16_t dst_color, int num_skipped_pixels = 0) {
  int perimeter = (MATRIX_WIDTH * 2) + (MATRIX_HEIGHT * 2) - 4;
  int pixels_to_draw = round(perimeter * progress);

  int curr_x = MATRIX_WIDTH / 2;
  int curr_y = 0;

  int x_dir = 1;
  int y_dir = 0;
  int pixels_drawn = 0;
  while (pixels_drawn++ < pixels_to_draw) {
    // fade from 1 to 2 top to bottom middle, back from 2 to 1 otherwise
    float percent = pixels_drawn / (perimeter / 2.f);
    if (percent >= 1) {
      percent = 2 - percent;
    }

    if (pixels_drawn > num_skipped_pixels) {
      uint16_t color = rgb565Fade(src_color, dst_color, percent);
      matrix_ptr->drawPixel(curr_x, curr_y, color);
    }

    curr_x += x_dir;
    curr_y += y_dir;

    if (curr_x == MATRIX_WIDTH - 1 && curr_y == 0) {
      x_dir = 0;
      y_dir = 1;
    }
    else if (curr_x == MATRIX_WIDTH - 1 && curr_y == MATRIX_HEIGHT - 1) {
      x_dir = -1;
      y_dir = 0;
    }
    else if (curr_x == 0 && curr_y == MATRIX_HEIGHT - 1) {
      x_dir = 0;
      y_dir = -1;
    }
    else if (curr_x == 0 && curr_y == 0) {
      x_dir = 1;
      y_dir = 0;
    }
  }
}

void playSnakeAnimation(FastLED_NeoMatrix* matrix_ptr, uint16_t color) {
  // hacky way to play a snake animation around the screen
  for (int i = 0; i < 12; i++) {
    for (float progress = 0.f; progress < 1.f; progress += 0.01) {
      drawDayProgress(matrix_ptr, progress, color, color, progress * 50);
      if (progress >= 0.1f) {
        drawDayProgress(matrix_ptr, progress - 0.1f, BLACK, BLACK, (progress - 0.1) * 50);
      }
      else if (progress <= 0.1f) {
        drawDayProgress(matrix_ptr, 1.f - (0.1f - progress), BLACK, BLACK, 50);
      }
      FastLED.delay(10);
    }
  }

  for (float progress = 0.f; progress <= 0.1f; progress += 0.01) {
    drawDayProgress(matrix_ptr, 1.f - (0.1f - progress), BLACK, BLACK, 50);
    FastLED.delay(10);
  }
}

void print_no_slashed_zero(char* text, int start_x, int start_y) {
  int curr_x = start_x;
  int curr_y = start_y;

  for (; *text; text++) {
    char tmp_str[2] = {*text, '\0'};
    u8g2_for_adafruit_gfx.setCursor(curr_x, curr_y);
    u8g2_for_adafruit_gfx.print(tmp_str);
    if (tmp_str[0] == '0') {
      matrix.drawPixel(curr_x + 1, curr_y - 3, BLACK);
      matrix.drawPixel(curr_x + 2, curr_y - 4, BLACK);
    }

    curr_x += u8g2_for_adafruit_gfx.getUTF8Width(tmp_str);
  }
}

void loop() {
  FastLED.delay(200);
  time_t local_time = us_pacific.toLocal(time_client.getEpochTime());
  int local_minute = minute(local_time);

  // save processing time if refresh is not needed
  if (local_minute != last_minute) {
    Serial.print("matrix refreshing...");
    unsigned long refresh_start_millis = millis();

    char time_str[6];
    sprintf(time_str, "%02d:%02d", hourFormat12(local_time), local_minute);

    matrix.fillScreen(0);
    print_no_slashed_zero(time_str, 4, 7);

    // draw 2 pixel wide colon
    matrix.drawPixel(15, 2, fg_color);
    matrix.drawPixel(15, 3, fg_color);
    matrix.drawPixel(16, 2, fg_color);
    matrix.drawPixel(15, 5, fg_color);
    matrix.drawPixel(15, 6, fg_color);
    matrix.drawPixel(16, 5, fg_color);

    float day_percent = (hour(local_time) * 60 + local_minute) / (12.f * 60);
    if (isAM(local_time)) {
      drawDayProgress(&matrix, day_percent, PROGRESS_COLOR_1_AM, PROGRESS_COLOR_2_AM);
    }
    else {
      day_percent -= 1;
      drawDayProgress(&matrix, day_percent, PROGRESS_COLOR_1_PM, PROGRESS_COLOR_2_PM);
    }

    if (hourFormat12(local_time) == 0 && local_minute == 0) {
      playSnakeAnimation(&matrix, PROGRESS_COLOR_2_PM);  // midnight
    }
    else if (hourFormat12(local_time) == 12 && local_minute == 0) {
      playSnakeAnimation(&matrix, PROGRESS_COLOR_2_AM);
    }

    last_minute = local_minute;

    Serial.print("matrix refresh took ");
    Serial.print(millis() - refresh_start_millis);
    Serial.println(" ms");
  }
  else if (last_ntp_update + ntp_update_interval < millis()) {
    Serial.print("updating ntp... ");
    bool update_success = time_client.forceUpdate();
    Serial.println(update_success ? "success" : "failed");

    int new_fg_color = update_success ? TIME_COLOR : SYNC_FAILED_COLOR;
    u8g2_for_adafruit_gfx.setForegroundColor(new_fg_color);

    if (new_fg_color != fg_color) {
      last_minute = -1; // force time redraw
    }

    fg_color = new_fg_color;
    last_ntp_update = millis();
    ntp_update_interval = update_success ? NTP_DEFAULT_UPDATE_INTERVAL : NTP_FAIL_UPDATE_INTERVAL;
  }
}
