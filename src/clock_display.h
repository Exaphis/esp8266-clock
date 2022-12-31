#pragma once

#include <FastLED_NeoMatrix.h>
#include <U8g2_for_Adafruit_GFX.h>

typedef struct coord {
    int r;
    int c;
} Coord;

class ClockDisplay {
   public:
    ClockDisplay(FastLED_NeoMatrix& matrix) : matrix(matrix), perimeter((matrix.width() * 2) + (matrix.height() * 2) - 4) {
        u8g2_for_adafruit_gfx.begin(matrix);
        u8g2_for_adafruit_gfx.setForegroundColor(TIME_COLOR);
        u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mn);
    }
    void drawTime(int hour, int minute);

    //    private:
    Coord getBorderPixel(int index);
    void drawBorderPixel(int index, bool black = false);

    void drawDayProgress(float progress, bool bottom_up);
    void printTime(int hour, int minute);
    void playSnakeAnimation();
    void playHoleAnimation();

    FastLED_NeoMatrix& matrix;
    U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;
    int last_minute = -1;
    const int perimeter;

    static constexpr uint16_t TIME_COLOR = 0xFFFF;
    static constexpr uint16_t PROGRESS_COLOR_AM_1 = 0xA243;
    static constexpr uint16_t PROGRESS_COLOR_AM_2 = 0x8C84;
    static constexpr uint16_t PROGRESS_COLOR_PM_1 = 0x999A;
    static constexpr uint16_t PROGRESS_COLOR_PM_2 = 0x4EFA;
    static constexpr int TEXT_START_R = 7;
    static constexpr int TEXT_START_C = 4;

    uint16_t top_color = TIME_COLOR;
    uint16_t bottom_color = TIME_COLOR;
};