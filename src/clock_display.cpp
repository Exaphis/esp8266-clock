#include "clock_display.h"

#include "rgb_helper.h"

#define BLACK (uint16_t)0
#define SNAKE_LEN 5
#define SNAKE_LOOPS 12

void ClockDisplay::drawTime(int hour, int minute) {
    // save processing time if refresh is not needed
    if (minute == last_minute) {
        return;
    }
    Serial.print("updating display... ");
    unsigned long refresh_start_millis = millis();

    last_minute = minute;
    matrix.fillScreen(0);

    // print time before day progress outline because printing time overwrites bottom pixels
    printTime(hour, minute);

    float day_percent = (hour * 60 + minute) / (12.f * 60);

    // draw day progress outline and update colors
    if (hour < 12) {  // AM
        top_color = PROGRESS_COLOR_AM_1;
        bottom_color = PROGRESS_COLOR_AM_2;

        drawDayProgress(day_percent, false);
    } else {  // PM
        // draw AM colors at noon because we want to show the hole animation
        // that will transition the AM colors to the PM colors
        bool is_noon = hour == 12 && minute == 0;
        if (is_noon) {
            top_color = PROGRESS_COLOR_AM_1;
            bottom_color = PROGRESS_COLOR_AM_2;
            drawDayProgress(1, false);
        }

        top_color = PROGRESS_COLOR_PM_1;
        bottom_color = PROGRESS_COLOR_PM_2;

        day_percent -= 1;
        if (!is_noon) {
            drawDayProgress(day_percent, true);
        }
    }

    if (hour == 0 && minute == 0) {  // midnight
        playSnakeAnimation();
    } else if (hour == 12 && minute == 0) {  // noon
        playHoleAnimation();
    }

    Serial.print("took ");
    Serial.print(millis() - refresh_start_millis);
    Serial.println(" ms");
}

void ClockDisplay::drawDayProgress(float progress, bool bottom_up) {
    for (int i = 0; i < perimeter / 2; i++) {
        float percent = (float)i / (perimeter / 2);
        bool black = bottom_up ? percent <= progress : percent > progress;

        drawBorderPixel(i, black);
        drawBorderPixel(perimeter - i - 1, black);
    }
}

void ClockDisplay::printTime(int hour, int minute) {
    // convert hour to 12 hour format
    if (hour == 0) {
        hour = 12;
    } else if (hour > 12) {
        hour -= 12;
    }

    char time_str[32];
    sprintf(time_str, "%02d:%02d", hour, minute);
    char* text = time_str;

    // draw time characters
    int curr_x = TEXT_START_C;
    int curr_y = TEXT_START_R;

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

    // draw 2 pixel wide colon
    matrix.drawPixel(15, 2, TIME_COLOR);
    matrix.drawPixel(15, 3, TIME_COLOR);
    matrix.drawPixel(16, 2, TIME_COLOR);
    matrix.drawPixel(15, 5, TIME_COLOR);
    matrix.drawPixel(15, 6, TIME_COLOR);
    matrix.drawPixel(16, 5, TIME_COLOR);
}

void ClockDisplay::playSnakeAnimation() {
    // snake animation around the perimeter of the screen
    int snake_head_i = 0;

    for (int i = 0; i < (SNAKE_LOOPS * perimeter) - SNAKE_LEN; i++) {
        drawBorderPixel(snake_head_i);

        int snake_tail_i = snake_head_i - SNAKE_LEN;
        if (snake_tail_i < 0) {
            snake_tail_i += perimeter;
        }
        drawBorderPixel(snake_tail_i, true);

        FastLED.delay(10);
        snake_head_i = (snake_head_i + 1) % perimeter;
    }

    // remove the snake tail
    for (int i = SNAKE_LEN; i >= 0; i--) {
        drawBorderPixel(perimeter - i - 1, true);
        FastLED.delay(10);
    }
}

void ClockDisplay::playHoleAnimation() {
    // draw colors in border, randomly, one by one
    // used to switch from AM to PM
    int arr[perimeter];
    for (int i = 0; i < perimeter; i++) {
        arr[i] = i;
    }

    // shuffle arr (Fisher-Yates shuffle)
    for (int i = 0; i < perimeter; i++) {
        int j = random(i, perimeter);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }

    for (int i = 0; i < perimeter; i++) {
        int pixel_index = arr[i];
        drawBorderPixel(pixel_index);
        FastLED.delay(500);
    }
}

Coord ClockDisplay::getBorderPixel(int index) {
    if (index < 0) {
        return {-1, -1};
    }

    if (index < matrix.width() / 2) {
        return {0, (matrix.width() / 2) + index};
    } else {
        // subtraction offset by one to not double count corners
        index -= (matrix.width() / 2) - 1;
    }

    if (index < matrix.height()) {
        return {index, matrix.width() - 1};
    } else {
        index -= matrix.height() - 1;
    }

    if (index < matrix.width()) {
        return {matrix.height() - 1, matrix.width() - 1 - index};
    } else {
        index -= matrix.width() - 1;
    }

    if (index < matrix.height()) {
        return {matrix.height() - 1 - index, 0};
    } else {
        index -= matrix.height() - 1;
    }

    if (index < matrix.width() / 2) {
        return {0, index};
    } else {
        return {-1, -1};
    }
}

void ClockDisplay::drawBorderPixel(int i, bool black) {
    Coord pixel = getBorderPixel(i);
    uint16_t color = black ? BLACK : rgb565Fade(top_color, bottom_color, (float)(i + 1) / perimeter);
    matrix.drawPixel(pixel.c, pixel.r, color);
}