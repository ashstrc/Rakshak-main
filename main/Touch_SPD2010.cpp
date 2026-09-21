#include "Touch_SPD2010.h"
#include <Wire.h>
#include <esp_err.h>

/*
 * SPD2010 touch controller
 *
 * Waveshare ESP32-S3 Touch LCD 1.46:
 *   I2C address : 0x53
 *   SDA         : GPIO 11
 *   SCL         : GPIO 10
 *   INT         : GPIO 4
 *   TP_RST      : TCA9554 EXIO1
 *
 * The register address is sent little-endian (low byte first).
 */

struct SPD2010_Touch touch_data = {0};

extern void Switch_Page(int dir);

static bool touch_read_reg(uint16_t reg, uint8_t *data, uint8_t len)
{
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write((uint8_t)(reg >> 8));

    if (Wire.endTransmission(false) != 0)
        return false;

    uint8_t received = Wire.requestFrom((int)SPD2010_ADDR, (int)len, (int)true);
    if (received != len)
        return false;

    for (uint8_t i = 0; i < len; ++i)
        data[i] = Wire.read();

    return true;
}

static bool touch_write_reg(uint16_t reg, const uint8_t *data, uint8_t len)
{
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write((uint8_t)(reg >> 8));

    for (uint8_t i = 0; i < len; ++i)
        Wire.write(data[i]);

    return Wire.endTransmission(true) == 0;
}

bool I2C_Read_Touch(uint8_t addr, uint16_t reg, uint8_t *data, uint32_t len)
{
    if (addr != SPD2010_ADDR || len == 0 || len > 255)
        return false;

    return touch_read_reg(reg, data, (uint8_t)len);
}

bool I2C_Write_Touch(uint8_t addr, uint16_t reg, const uint8_t *data, uint32_t len)
{
    if (addr != SPD2010_ADDR || len > 255)
        return false;

    return touch_write_reg(reg, data, (uint8_t)len);
}

static esp_err_t write_tp_cmd(uint16_t reg, uint8_t a, uint8_t b)
{
    const uint8_t data[2] = {a, b};
    return touch_write_reg(reg, data, 2) ? ESP_OK : ESP_FAIL;
}

esp_err_t write_tp_point_mode_cmd()
{
    return write_tp_cmd(0x0050, 0x00, 0x00);
}

esp_err_t write_tp_start_cmd()
{
    return write_tp_cmd(0x0046, 0x00, 0x00);
}

esp_err_t write_tp_cpu_start_cmd()
{
    return write_tp_cmd(0x0004, 0x01, 0x00);
}

esp_err_t write_tp_clear_int_cmd()
{
    /*
     * SPD2010 needs ACK + re-arm on the interrupt register.
     */
    if (write_tp_cmd(0x0002, 0x01, 0x00) != ESP_OK)
        return ESP_FAIL;

    delayMicroseconds(200);

    if (write_tp_cmd(0x0002, 0x00, 0x00) != ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t read_tp_status_length(tp_status_t *tp_status)
{
    if (!tp_status)
        return ESP_ERR_INVALID_ARG;

    uint8_t data[4] = {0};

    if (!touch_read_reg(0x0020, data, 4))
        return ESP_FAIL;

    memset(tp_status, 0, sizeof(*tp_status));

    tp_status->status_low.pt_exist = data[0] & 0x01;
    tp_status->status_low.gesture  = (data[0] >> 1) & 0x01;
    tp_status->status_low.aux      = (data[0] >> 3) & 0x01;

    tp_status->status_high.tic_busy   = (data[1] >> 7) & 0x01;
    tp_status->status_high.tic_in_bios = (data[1] >> 6) & 0x01;
    tp_status->status_high.tic_in_cpu  = (data[1] >> 5) & 0x01;
    tp_status->status_high.tint_low    = (data[1] >> 4) & 0x01;
    tp_status->status_high.cpu_run     = (data[1] >> 3) & 0x01;

    uint16_t len = (uint16_t)data[2] | ((uint16_t)data[3] << 8);

    /* 4-byte minimum status packet, 64-byte local buffer maximum. */
    tp_status->read_len = (len >= 4 && len <= 64) ? len : 0;

    return ESP_OK;
}

esp_err_t read_tp_hdp(tp_status_t *tp_status, SPD2010_Touch *touch)
{
    if (!tp_status || !touch)
        return ESP_ERR_INVALID_ARG;

    if (tp_status->read_len < 10 || tp_status->read_len > 64)
        return ESP_FAIL;

    uint8_t data[64] = {0};

    if (!touch_read_reg(0x0300, data, (uint8_t)tp_status->read_len))
        return ESP_FAIL;

    memset(touch, 0, sizeof(*touch));

    const uint8_t check_id = data[4];

    if (check_id <= 0x0A && tp_status->status_low.pt_exist)
    {
        uint8_t count = (tp_status->read_len - 4) / 6;

        if (count > 10)
            count = 10;

        touch->touch_num = count;
        touch->pack_code = check_id;

        for (uint8_t i = 0; i < count; ++i)
        {
            const uint8_t off = i * 6;

            touch->rpt[i].id = data[4 + off];

            touch->rpt[i].x =
                (uint16_t)(((data[7 + off] & 0xF0) << 4) |
                           data[5 + off]);

            touch->rpt[i].y =
                (uint16_t)(((data[7 + off] & 0x0F) << 8) |
                           data[6 + off]);

            touch->rpt[i].weight = data[8 + off];
        }

        if (count > 0)
        {
            if (!touch_data.down)
            {
                touch->down = 1;
                touch->down_x = touch->rpt[0].x;
                touch->down_y = touch->rpt[0].y;
            }
        }
    }
    else if (check_id == 0xF6 && tp_status->status_low.gesture)
    {
        touch->touch_num = 0;
        touch->pack_code = check_id;
        touch->gesture = data[6] & 0x07;
    }

    return ESP_OK;
}

esp_err_t read_tp_hdp_status(tp_hdp_status_t *tp_hdp_status)
{
    if (!tp_hdp_status)
        return ESP_ERR_INVALID_ARG;

    uint8_t data[8] = {0};

    if (!touch_read_reg(0xFC02, data, 8))
        return ESP_FAIL;

    tp_hdp_status->status = data[5];
    tp_hdp_status->next_packet_len =
        (uint16_t)data[2] | ((uint16_t)data[3] << 8);

    return ESP_OK;
}

esp_err_t Read_HDP_REMAIN_DATA(tp_hdp_status_t *tp_hdp_status)
{
    if (!tp_hdp_status)
        return ESP_ERR_INVALID_ARG;

    if (tp_hdp_status->next_packet_len == 0 ||
        tp_hdp_status->next_packet_len > 32)
        return ESP_FAIL;

    uint8_t data[32] = {0};

    return touch_read_reg(
        0x0300,
        data,
        (uint8_t)tp_hdp_status->next_packet_len
    ) ? ESP_OK : ESP_FAIL;
}

esp_err_t read_fw_version()
{
    uint8_t data[18] = {0};

    if (!touch_read_reg(0x2600, data, 18))
    {
        Serial.println("SPD2010: firmware read FAILED");
        return ESP_FAIL;
    }

    uint16_t version = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    uint32_t pid =
        (uint32_t)data[6] |
        ((uint32_t)data[7] << 8) |
        ((uint32_t)data[8] << 16) |
        ((uint32_t)data[9] << 24);

    Serial.print("SPD2010: firmware OK, version=");
    Serial.print(version);
    Serial.print(", PID=0x");
    Serial.println(pid, HEX);

    return ESP_OK;
}

esp_err_t tp_read_data(SPD2010_Touch *touch)
{
    if (!touch)
        return ESP_ERR_INVALID_ARG;

    tp_status_t status;
    memset(&status, 0, sizeof(status));
    memset(touch, 0, sizeof(*touch));

    if (read_tp_status_length(&status) != ESP_OK)
        return ESP_FAIL;

    /*
     * The controller can boot into BIOS/CPU housekeeping states.
     * Bring it into point mode and then start the touch engine.
     */
    if (status.status_high.tic_in_bios)
    {
        write_tp_clear_int_cmd();
        write_tp_cpu_start_cmd();
        return ESP_FAIL;
    }

    if (status.status_high.tic_in_cpu)
    {
        write_tp_point_mode_cmd();
        write_tp_start_cmd();
        write_tp_clear_int_cmd();
        return ESP_FAIL;
    }

    if (status.status_high.cpu_run && status.read_len == 0)
    {
        write_tp_clear_int_cmd();
        return ESP_FAIL;
    }

    if (status.status_low.pt_exist || status.status_low.gesture)
    {
        if (read_tp_hdp(&status, touch) != ESP_OK)
        {
            write_tp_clear_int_cmd();
            return ESP_FAIL;
        }

        write_tp_clear_int_cmd();

        /*
         * Some packets are followed by another HDP packet.
         * Drain the controller's queue without blocking for long.
         */
        for (uint8_t i = 0; i < 4; ++i)
        {
            tp_hdp_status_t hdp;
            if (read_tp_hdp_status(&hdp) != ESP_OK)
                break;

            if (hdp.status == 0x82)
            {
                write_tp_clear_int_cmd();
                break;
            }

            if (hdp.status == 0x00)
            {
                if (Read_HDP_REMAIN_DATA(&hdp) != ESP_OK)
                    break;
                continue;
            }

            break;
        }

        return ESP_OK;
    }

    if (status.status_high.cpu_run && status.status_low.aux)
        write_tp_clear_int_cmd();

    return ESP_FAIL;
}

/* ---------- RESET ---------- */

uint8_t SPD2010_Touch_Reset(void)
{
    /*
     * On this board TP_RST is TCA9554 EXIO1.
     * TCA9554 pins in this project are 1-based.
     */
    Mode_EXIO(EXIO_PIN1, 0);

    Set_EXIO(EXIO_PIN1, Low);
    delay(50);

    Set_EXIO(EXIO_PIN1, High);
    delay(80);

    return 1;
}

/* ---------- INITIALIZATION ---------- */

uint8_t Touch_Init(void)
{
    Serial.println("SPD2010 TOUCH INIT");

    pinMode(EXAMPLE_PIN_NUM_TOUCH_INT, INPUT_PULLUP);

    /* Touch reset is routed through the TCA9554. */
    SPD2010_Touch_Reset();

    delay(100);

    if (read_fw_version() != ESP_OK)
    {
        Serial.println("SPD2010 TOUCH INIT FAILED");
        return 0;
    }

    /*
     * Put the controller into point mode and start it.
     * The controller may already be running, so failures here
     * are reported but do not immediately abort initialization.
     */
    write_tp_point_mode_cmd();
    delay(5);
    write_tp_start_cmd();
    delay(5);
    write_tp_clear_int_cmd();

    Serial.println("SPD2010 TOUCH READY");
    return 1;
}

/* ---------- LVGL COORDINATE API ---------- */

bool Touch_Get_xy(uint16_t *x, uint16_t *y,
                  uint16_t *strength,
                  uint8_t *point_num,
                  uint8_t max_point_num)
{
    if (!x || !y || !point_num || max_point_num == 0)
        return false;

    *x = 0;
    *y = 0;
    *point_num = 0;

    if (strength)
        *strength = 0;

    SPD2010_Touch sample = {0};

    if (tp_read_data(&sample) != ESP_OK)
        return false;

    uint8_t count = sample.touch_num;

    static uint32_t lastTouchPrint = 0;

if (millis() - lastTouchPrint > 300)
{
    Serial.printf("TOUCH RAW: count=%d x=%d y=%d\n",
                  sample.touch_num,
                  sample.rpt[0].x,
                  sample.rpt[0].y);

    lastTouchPrint = millis();
}

    if (count > max_point_num)
        count = max_point_num;

    if (count == 0)
        return false;

*x = sample.rpt[0].x;
*y = sample.rpt[0].y;

if (strength) *strength = sample.rpt[0].weight;

    *point_num = count;

    return true;
}

/* ---------- TOUCH LOOP ---------- */

void Touch_Loop(void)
{
    SPD2010_Touch sample = {0};

    esp_err_t touch_result = tp_read_data(&sample);

if (touch_result != ESP_OK)
    return;

    if (sample.touch_num > 0)
    {
        touch_data = sample;

        if (!touch_data.down)
        {
            touch_data.down = 1;
            touch_data.down_x = sample.rpt[0].x;
            touch_data.down_y = sample.rpt[0].y;
        }
    }

    if (sample.gesture == 1)
    {
        Switch_Page(1);
        delay(150);
    }
    else if (sample.gesture == 2)
    {
        Switch_Page(-1);
        delay(150);
    }
}
