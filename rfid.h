// Copyright © 2023 Miroslav Gallik
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the “Software”), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef CHAFON_RFID_RFID_H
#define CHAFON_RFID_RFID_H

#include <stdint.h>
#include <termios.h>

// ---------------------------------------------------------------------------------------------------------------------
// ------  CONFIG AREA -------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

// Print packets packet
#ifdef DEBUG
#define DEBUG_PRINT
#endif

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

#define RFID_MEM_RESERVED       0x00
#define RFID_MEM_EPC            0x01
#define RFID_MEM_TID            0x02
#define RFID_MEM_USER           0x03

#pragma pack(push, 1)
typedef struct
{
    uint16_t version;
    uint8_t type;
    uint8_t tr_type;
    uint8_t dmaxfre;
    uint8_t dminfre;
    uint8_t power;
    uint8_t scntm;
    uint8_t ant;
    uint8_t res1;
    uint8_t res2;
    uint8_t check_ant;
} rfid_reader_info_t;

typedef struct
{
    uint8_t read_mode;
    uint8_t tag_protocol;
    uint8_t read_pause_time;
    uint8_t filter_time;
    uint8_t q_value;
    uint8_t session;

    uint8_t mask_mem;
    uint16_t mask_addr;
    uint8_t mask_len;
    uint8_t mask_data[32];

    uint8_t tid_addr;
    uint8_t tid_len;
} rfid_reader_working_mode_t;

typedef struct
{
    uint8_t len;
    uint8_t addr;
    uint8_t cmd;
    uint8_t status;
} rfid_header_t;
#pragma pack(pop)

typedef struct
{
    int port_fd;
    int state;
    rfid_header_t header;

    uint8_t addr;
    
    uint8_t epc_len;
    uint8_t epc_data[32];

    uint32_t password;

    uint8_t mask_mem;
    uint16_t mask_addr;
    uint8_t mask_len;
    uint8_t *mask_data;
    uint8_t mask_data_len;
} rfid_t;

typedef void (*tag_inventory_cb)(rfid_t *rfid, uint8_t ant, uint16_t num, uint16_t id, uint8_t data_len, uint8_t *data,
                                 uint8_t rssi, uint8_t count);

int rfid_init(rfid_t *rfid, const char *device, speed_t speed);
void rfid_done(rfid_t *rfid);

void rfid_set_epc(rfid_t *rfid, void *epc, uint8_t len);
void rfid_set_password(rfid_t *rfid, uint32_t password);
// Warning: mask_data must be valid during commands execution. There is no copy operation
void rfid_set_mask(rfid_t *rfid, uint8_t mask_mem, uint16_t mask_addr, uint8_t *mask_data, uint8_t mask_len);
void rfid_set_address(rfid_t *rfid, uint8_t addr);

void rfid_6c_tag_inventory(rfid_t *rfid, uint8_t q_value, uint8_t session, uint8_t tid_addr, uint8_t tid_len,
                           uint8_t target, uint8_t ant, uint8_t scan_time,
                           tag_inventory_cb tag_callback);
int rfid_6c_read_data(rfid_t *rfid, uint8_t mem, uint8_t word_ptr, uint8_t num, uint8_t *response);
int rfid_6c_write_data(rfid_t *rfid, uint8_t w_num, uint8_t mem, uint8_t word_ptr, uint8_t *wdt);
int rfid_6c_write_epc(rfid_t *rfid, uint8_t e_num, uint32_t password, uint8_t *w_epc);
int rfid_6c_kill_tag(rfid_t *rfid, uint32_t kill_password);




int rfid_x_get_reader_information(rfid_t *rfid, rfid_reader_info_t *info);
int rfid_x_set_working_frequency(rfid_t *rfid, uint8_t max_freq, uint8_t min_freq);
int rfid_x_set_reader_address(rfid_t *rfid, uint8_t new_addr);
int rfid_x_set_inventory_time(rfid_t *rfid, uint8_t time);
int rfid_x_set_baud_rate(rfid_t *rfid, uint8_t baud_rate);
int rfid_x_set_rf_power(rfid_t *rfid, uint8_t power);
int rfid_x_buzzer_control(rfid_t *rfid, uint8_t active_time, uint8_t silent_time, uint8_t repeat);
int rfid_x_tag_customised_function(rfid_t *rfid, uint8_t *inlay_type);
int rfid_x_set_antenna_multiplexing(rfid_t *rfid, uint8_t config);
int rfid_x_set_buzzer(rfid_t *rfid, uint8_t enabled);
int rfid_x_set_gpio(rfid_t *rfid, uint8_t gpio);
int rfid_x_get_gpio(rfid_t *rfid, uint8_t *gpio);
int rfid_x_get_serial(rfid_t *rfid, uint32_t *serial);
int rfid_x_set_antenna_check(rfid_t *rfid, uint8_t enabled);
int rfid_x_set_comm_interface(rfid_t *rfid, uint8_t interface);
int rfid_x_get_antenna_return_loss_threshold(rfid_t *rfid, uint8_t *return_loss);
int rfid_x_set_antenna_return_loss_threshold(rfid_t *rfid, uint8_t return_loss);
int rfid_x_set_max_epc_len(rfid_t *rfid, uint8_t len);
int rfid_x_get_max_epc_len(rfid_t *rfid, uint8_t *len);
int rfid_x_buffer_read(rfid_t *rfid, tag_inventory_cb tag_callback);
int rfid_x_buffer_clear(rfid_t *rfid);
int rfid_x_buffer_tag_cnt(rfid_t *rfid, uint16_t *cnt);
int rfid_x_set_real_time_inventory_params(rfid_t *rfid, uint8_t tag_protocol, uint8_t read_pause_time,
                                          uint8_t filter_time, uint8_t q_value, uint8_t session, uint8_t tid_addr, uint8_t tid_len);
int rfid_x_set_reader_mode(rfid_t *rfid, uint8_t read_mode);
int rfid_x_get_reader_mode(rfid_t *rfid, rfid_reader_working_mode_t *reader_mode);
int rfid_x_set_heartbeat_time(rfid_t *rfid, uint8_t time);
int rfid_x_get_heartbeat_time(rfid_t *rfid, uint8_t *time);
int rfid_x_set_rf_power_write (rfid_t *rfid, uint8_t power);
int rfid_x_get_rf_power_write (rfid_t *rfid, uint8_t *power);
int rfid_x_set_write_retry_cnt(rfid_t *rfid, uint8_t cnt);
int rfid_x_get_write_retry_cnt(rfid_t *rfid, uint8_t *cnt);
int rfid_x_set_customised_function_password (rfid_t *rfid, uint32_t password);
int rfid_x_get_customised_function_password (rfid_t *rfid, uint32_t *password);
int rfid_x_set_profile(rfid_t *rfid, uint8_t profile);
int rfid_x_get_profile(rfid_t *rfid, uint8_t *profile);
int rfid_x_set_drm(rfid_t *rfid, uint8_t drm_mode);
int rfid_x_get_drm(rfid_t *rfid, uint8_t *drm_mode);
int rfid_x_get_antenna_return_loss(rfid_t *rfid, uint32_t test_freq, uint8_t antenna, uint8_t *return_loss);
int rfid_x_get_temperature(rfid_t *rfid, int *temp);

int rfid_em4325_sync_timestamp(rfid_t *rfid, uint32_t utc_time);
int rfid_em4325_get_temperature(rfid_t *rfid, uint8_t send_uid, uint8_t new_sample, uint8_t *uid,
                                uint32_t *sensor_data, uint32_t *utc_time);
int rfid_em4325_spi(rfid_t *rfid, uint8_t cmd_len, uint8_t res_len, uint8_t clk, uint8_t delay,
                    uint8_t interval, uint8_t *cmd, uint8_t *res);
int rfid_em4325_reset_alert(rfid_t *rfid);

#endif //CHAFON_RFID_RFID_H
