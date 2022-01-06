#include "clock_display.h"

#include "rgb_helper.h"

#define BLACK (uint16_t)0

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

    if (hour < 12 || (hour == 12 && minute == 0)) {  // AM
        drawDayProgress(day_percent, PROGRESS_COLOR_AM_1, PROGRESS_COLOR_AM_2);
    } else {  // PM
        day_percent -= 1;
        drawDayProgress(day_percent, PROGRESS_COLOR_PM_1, PROGRESS_COLOR_PM_2);
    }

    if (hour == 0 && minute == 0) {  // midnight
        playSnakeAnimation(PROGRESS_COLOR_PM_2);
    } else if (hour == 12 && minute == 0) {  // noon
        playHoleAnimation();
    }

    Serial.print("took ");
    Serial.print(millis() - refresh_start_millis);
    Serial.println(" ms");
}

void ClockDisplay::drawDayProgress(float progress, uint16_t src_color, uint16_t dst_color, int num_skipped_pixels) {
    const int perimeter = (matrix.width() * 2) + (matrix.height() * 2) - 4;
    int pixels_to_draw = round(perimeter * progress);

    int pixels_drawn = 0;
    while (pixels_drawn++ < pixels_to_draw) {
        // fade from 1 to 2 top to bottom middle, back from 2 to 1 otherwise
        float percent = pixels_drawn / (perimeter / 2.f);
        if (percent >= 1) {
            percent = 2 - percent;
        }

        if (pixels_drawn > num_skipped_pixels) {
            Coord pixel = getBorderPixel(pixels_drawn - 1);
            uint16_t color = rgb565Fade(src_color, dst_color, percent);
            matrix.drawPixel(pixel.c, pixel.r, color);
        }
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

void ClockDisplay::playSnakeAnimation(uint16_t color) {
    // hacky way to play a snake animation around the screen
    for (int i = 0; i < 12; i++) {
        for (float progress = 0.f; progress < 1.f; progress += 0.01) {
            drawDayProgress(progress, color, color, progress * 50);
            if (progress >= 0.1f) {
                drawDayProgress(progress - 0.1f, BLACK, BLACK, (progress - 0.1) * 50);
            } else if (progress <= 0.1f) {
                drawDayProgress(1.f - (0.1f - progress), BLACK, BLACK, 50);
            }
            FastLED.delay(10);
        }
    }

    for (float progress = 0.f; progress <= 0.1f; progress += 0.01) {
        drawDayProgress(1.f - (0.1f - progress), BLACK, BLACK, 50);
        FastLED.delay(10);
    }
}

void ClockDisplay::playHoleAnimation() {
    // remove colors from filled border one by one randomly
    const int perimeter = (matrix.width() * 2) + (matrix.height() * 2) - 4;
    int arr[perimeter];
    for (int i = 0; i < perimeter; i++) {
        arr[i] = i;
    }

    // shuffle arr
    for (int i = 0; i < perimeter; i++) {
        int j = random(i, perimeter);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }

    for (int i = 0; i < perimeter; i++) {
        int pixel_index = arr[i];
        Coord pixel = getBorderPixel(pixel_index);
        matrix.drawPixel(pixel.c, pixel.r, BLACK);
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