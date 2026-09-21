#pragma once

#include "esp_lcd_panel_ops.h"

extern esp_lcd_panel_handle_t panel;

void display_init();
void display_flush(int x1,int y1,int x2,int y2,void *data);