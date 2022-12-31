#pragma once

void rgb565To888(int color, int* out_red, int* out_green, int* out_blue) {
    *out_red = (color & 0xF800) >> 11;
    *out_green = (color & 0x7E0) >> 5;
    *out_blue = (color & 0x1F);

    // https://stackoverflow.com/a/9069480
    *out_red = (*out_red * 527 + 23) >> 6;
    *out_green = (*out_green * 259 + 33) >> 6;
    *out_blue = (*out_blue * 527 + 23) >> 6;
}

void rgb888To565(int red, int green, int blue, uint16_t* out_color) {
    // https://stackoverflow.com/a/9069480
    red = (red * 249 + 1014) >> 11;
    green = (green * 253 + 505) >> 10;
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
