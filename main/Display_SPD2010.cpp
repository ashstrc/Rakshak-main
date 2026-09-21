#include "Display_SPD2010.h"

#include <stdlib.h>
#include <string.h>
#include "esp_intr_alloc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_spd2010.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"

uint8_t LCD_Backlight = 100;

esp_lcd_panel_handle_t panel_handle = NULL;

/* -------------------------------------------------- */
/* RESET                                              */
/* -------------------------------------------------- */

void SPD2010_Reset()
{
    Set_EXIO(EXIO_PIN2, Low);
    delay(50);
    Set_EXIO(EXIO_PIN2, High);
    delay(50);
}

void LCD_Init()
{
    SPD2010_Init();
    Touch_Init();
}

/* -------------------------------------------------- */
/* SPI BUS INIT                                       */
/* -------------------------------------------------- */

bool QSPI_Init(void)
{
    static const spi_bus_config_t host_config =
    {
        .data0_io_num = ESP_PANEL_LCD_SPI_IO_DATA0,
        .data1_io_num = ESP_PANEL_LCD_SPI_IO_DATA1,
        .sclk_io_num  = ESP_PANEL_LCD_SPI_IO_SCK,
        .data2_io_num = ESP_PANEL_LCD_SPI_IO_DATA2,
        .data3_io_num = ESP_PANEL_LCD_SPI_IO_DATA3,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0,
    };

    if (spi_bus_initialize(SPI2_HOST, &host_config, SPI_DMA_CH_AUTO) != ESP_OK)
    {
        printf("SPI init failed\n");
        return false;
    }

    printf("SPI init OK\n");
    return true;
}

/* -------------------------------------------------- */
/* PANEL INIT                                         */
/* -------------------------------------------------- */

bool SPD2010_Init()
{
    SPD2010_Reset();

    pinMode(ESP_PANEL_LCD_SPI_IO_TE, OUTPUT);

    if(!QSPI_Init())
    {
        printf("QSPI init failed\n");
        return false;
    }

    const esp_lcd_panel_io_spi_config_t io_config =
    {
        .cs_gpio_num = ESP_PANEL_LCD_SPI_IO_CS,
        .dc_gpio_num = -1,
        .spi_mode = ESP_PANEL_LCD_SPI_MODE,
        .pclk_hz = ESP_PANEL_LCD_SPI_CLK_HZ,
        .trans_queue_depth = ESP_PANEL_LCD_SPI_TRANS_QUEUE_SZ,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = ESP_PANEL_LCD_SPI_CMD_BITS,
        .lcd_param_bits = ESP_PANEL_LCD_SPI_PARAM_BITS,
        .flags =
        {
            .dc_low_on_data = 0,
            .octal_mode = 0,
            .quad_mode = 1,
            .sio_mode = 0,
            .lsb_first = 0,
            .cs_high_active = 0,
        },
    };

    esp_lcd_panel_io_handle_t io_handle = NULL;

    if(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)ESP_PANEL_HOST_SPI_ID_DEFAULT,
        &io_config,
        &io_handle) != ESP_OK)
    {
        printf("LCD IO config failed\n");
        return false;
    }

    printf("LCD IO OK\n");

    spd2010_vendor_config_t vendor_config =
    {
        .flags =
        {
            .use_qspi_interface = 1,
        },
    };

    esp_lcd_panel_dev_config_t panel_config =
{
    .reset_gpio_num = EXAMPLE_LCD_PIN_NUM_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = EXAMPLE_LCD_COLOR_BITS,
    .vendor_config = (void *)&vendor_config,
};

    if(esp_lcd_new_panel_spd2010(io_handle, &panel_config, &panel_handle) != ESP_OK)
    {
        printf("Panel create failed\n");
        return false;
    }

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    printf("SPD2010 LCD OK\n");

    return true;
}

/* -------------------------------------------------- */
/* DRAW WINDOW                                        */
/* -------------------------------------------------- */

void LCD_addWindow(
    uint16_t Xstart,
    uint16_t Ystart,
    uint16_t Xend,
    uint16_t Yend,
    uint16_t* color)
{
    Xend++;
    Yend++;

    if (Xend > EXAMPLE_LCD_WIDTH)  Xend = EXAMPLE_LCD_WIDTH;
    if (Yend > EXAMPLE_LCD_HEIGHT) Yend = EXAMPLE_LCD_HEIGHT;

    esp_lcd_panel_draw_bitmap(
        panel_handle,
        Xstart,
        Ystart,
        Xend,
        Yend,
        color);
}

/* -------------------------------------------------- */
/* BACKLIGHT                                          */
/* -------------------------------------------------- */

void Backlight_Init()
{
    ledcAttach(LCD_Backlight_PIN, Frequency, Resolution);

    /* FULL BRIGHTNESS */
    ledcWrite(LCD_Backlight_PIN, 1023);
}

void Set_Backlight(uint8_t Light)
{
    if(Light > 100) Light = 100;

    uint32_t duty = (Light * 1023) / 100;

    ledcWrite(LCD_Backlight_PIN, duty);
}