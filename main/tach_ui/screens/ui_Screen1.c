#include "../ui.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>

extern const uint8_t face_start[] asm("_binary_info2_face_rgb565_start");
LV_FONT_DECLARE(info2_digits_80);
LV_FONT_DECLARE(info2_digits_64);
static const lv_img_dsc_t face = {
    .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = 512, .h = 512},
    .data_size = 512 * 512 * 2, .data = face_start
};
lv_obj_t *ui_Screen1, *ui_rpm_bg, *ui_rpm_arc;
lv_obj_t *ui_label_mph_value, *ui_label_mph, *ui_label_gear;
lv_obj_t *ui_label_gear_value, *ui_label_odometer_value;
static lv_obj_t *values[2], *needles[2];
static lv_point_t points[2][2], previous_points[2][2];
static bool position_valid[2];
static int last_values[2], last_positions[2];

static lv_obj_t *black_panel(int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static void set_needle(unsigned index, float angle)
{
    /* Calibrated independently to each reference arc at image zoom 425. */
    const float cy = index == 0 ? 391.0f : 682.0f;
    const float inner = index == 0 ? 312.0f : 331.0f;
    const float outer = index == 0 ? 335.0f : 355.0f;
    float rad = angle * (3.14159265358979323846f / 180.0f);
    float x = cosf(rad), y = sinf(rad);
    lv_point_t screen[2] = {
        {(lv_coord_t)lroundf(360 + inner*x), (lv_coord_t)lroundf(cy + inner*y)},
        {(lv_coord_t)lroundf(360 + outer*x), (lv_coord_t)lroundf(cy + outer*y)}
    };
    if(position_valid[index] &&
       screen[0].x == previous_points[index][0].x &&
       screen[0].y == previous_points[index][0].y &&
       screen[1].x == previous_points[index][1].x &&
       screen[1].y == previous_points[index][1].y) return;
    lv_coord_t left = LV_MIN(screen[0].x, screen[1].x);
    lv_coord_t top = LV_MIN(screen[0].y, screen[1].y);
    lv_obj_invalidate(needles[index]);
    for(unsigned i=0;i<2;++i) {
        previous_points[index][i] = screen[i];
        points[index][i] = (lv_point_t){screen[i].x-left, screen[i].y-top};
    }
    lv_obj_set_pos(needles[index], left, top);
    lv_obj_set_size(needles[index], LV_ABS(screen[1].x-screen[0].x)+1,
                    LV_ABS(screen[1].y-screen[0].y)+1);
    lv_line_set_points(needles[index], points[index], 2);
    position_valid[index] = true;
}

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_black(), 0);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    ui_rpm_bg = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_rpm_bg, &face);
    lv_obj_center(ui_rpm_bg);
    lv_img_set_zoom(ui_rpm_bg, 425);
    /* Blank future header, preserving the flanking red accent strokes. */
    black_panel(276, 0, 180, 45);
    lv_obj_t *panels[2] = {
        black_panel(267, 220, 195, 64),
        black_panel(258, 478, 225, 64)
    };
    for(unsigned i=0;i<2;++i) {
        values[i] = lv_label_create(panels[i]);
        lv_obj_remove_style_all(values[i]);
        lv_obj_set_style_text_font(values[i], i == 0 ? &info2_digits_80 : &info2_digits_64, 0);
        lv_obj_set_style_text_color(values[i], lv_color_white(), 0);
        lv_label_set_text(values[i], "--");
        lv_obj_center(values[i]);
        needles[i] = lv_line_create(ui_Screen1);
        lv_obj_remove_style_all(needles[i]);
        lv_obj_set_style_line_width(needles[i], 7, 0);
        lv_obj_set_style_line_color(needles[i], lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_line_opa(needles[i], LV_OPA_COVER, 0);
        lv_obj_set_style_line_rounded(needles[i], false, 0);
        lv_obj_clear_flag(needles[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(needles[i], LV_OBJ_FLAG_HIDDEN);
        last_values[i] = last_positions[i] = INT_MIN;
        position_valid[i] = false;
    }
    lv_obj_t **legacy[] = {&ui_label_mph_value, &ui_label_mph, &ui_label_gear,
        &ui_label_gear_value, &ui_label_odometer_value};
    for(unsigned i=0;i<sizeof(legacy)/sizeof(legacy[0]);++i) {
        *legacy[i] = lv_label_create(ui_Screen1);
        lv_obj_add_flag(*legacy[i], LV_OBJ_FLAG_HIDDEN);
    }
    ui_rpm_arc = NULL;
}

void ui_info2_set_values(float fuel_pct, float battery_volts)
{
    const float inputs[2] = {fuel_pct, battery_volts};
    for(unsigned i=0;i<2;++i) {
        if(!values[i]) continue;
        bool valid = isfinite(inputs[i]) && inputs[i] >= 0 && inputs[i] <= (i==0 ? 100 : 20);
        int display = valid ? (int)lroundf(inputs[i] * (i==0 ? 1.0f : 10.0f)) : INT_MIN;
        if(display != last_values[i]) {
            if(!valid) lv_label_set_text(values[i], "--");
            else if(i == 0) {
                lv_obj_set_style_text_font(values[i], display>=100 ? &info2_digits_64 : &info2_digits_80, 0);
                lv_label_set_text_fmt(values[i], "%d%%", display);
            } else {
                char text[16];
                snprintf(text, sizeof(text), "%.1f V", display / 10.0);
                lv_label_set_text(values[i], text);
            }
            lv_obj_center(values[i]);
            last_values[i] = display;
        }
        if(!valid) {
            if(!lv_obj_has_flag(needles[i], LV_OBJ_FLAG_HIDDEN))
                lv_obj_add_flag(needles[i], LV_OBJ_FLAG_HIDDEN);
            last_positions[i] = INT_MIN;
            continue;
        }
        int position = (int)lroundf(inputs[i] * (i==0 ? 10.0f : 100.0f));
        if(position != last_positions[i]) {
            float value = i==0 ? position/10.0f : position/100.0f;
            float angle;
            if(i == 0) angle = 203.0f + value*1.34f;
            else {
                if(value < 11) value = 11;
                if(value > 15) value = 15;
                angle = 210.0f + (value-11.0f)*30.0f;
            }
            set_needle(i, angle);
            last_positions[i] = position;
        }
        if(lv_obj_has_flag(needles[i], LV_OBJ_FLAG_HIDDEN))
            lv_obj_clear_flag(needles[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_tach_set_rpm(int rpm) { (void)rpm; }
void ui_tach_set_gear(int gear) { (void)gear; }
void ui_Screen1_screen_destroy(void)
{
    if(ui_Screen1) lv_obj_del(ui_Screen1);
    ui_Screen1 = ui_rpm_bg = ui_rpm_arc = NULL;
    ui_label_mph_value = ui_label_mph = ui_label_gear = NULL;
    ui_label_gear_value = ui_label_odometer_value = NULL;
    for(unsigned i=0;i<2;++i) {
        values[i] = needles[i] = NULL;
        position_valid[i] = false;
        last_values[i] = last_positions[i] = INT_MIN;
    }
}
