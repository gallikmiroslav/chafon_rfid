// MIT License
//
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <stdarg.h>
#include "rfid.h"

#define CMD_OK      0x00
#define CMD_FAIL    0xA1
#define CMD_CRC_ERR 0xA2

#ifdef DEBUG_PRINT
#define DBG_PRINT                                printf
#define DBG_PRINT_BUFF(buff, len)                print_buff(buff, len)
#define DBG_PRINT_PACKET(buff_name, buff, len)   print_packet(buff_name, buff, len)

void print_buff(const void *buff, size_t len)
{
    uint8_t *p = (uint8_t*)buff;
    uint8_t *q = p + len;

    while(p != q)
        printf(" %02X", *p++);
}

void print_packet(const char *buff_name, const void *buff, uint8_t hdr_len)
{
    uint8_t *p = (uint8_t*)buff;
    uint8_t len = *p + 1;

    if (buff_name)
        printf("%s\n", buff_name);
    printf("  ");
    for (size_t i = hdr_len; i > 0; i--)
        printf(" %02X", *p++);
    printf("\x1b[34m");
    for (size_t i = len - (hdr_len + 2); i > 0; i--)
        printf(" %02X", *p++);
    printf("\x1b[31m");
    for (size_t i = 2; i > 0; i--)
        printf(" %02X", *p++);
    printf("\x1b[0m\n");
}
#else
#define DBG_PRINT                                    void(0)
#define DBG_PRINT_BUFF(buff, len)                    void(0)
#define DBG_PRINT_PACKET(buff_name, buff, hdr_len)   void(0)
#endif

#define CRC_INIT        0xFFFF
#define CRC_POLYNOMIAL  0x8408

uint16_t crc_update(uint16_t crc, uint8_t value)
{
    uint16_t c = crc;

    c ^= value;
    for(uint8_t i = 8; i > 0; i--)
    {
        if (c & 0x0001)
            c = (c >> 1) ^ CRC_POLYNOMIAL;
        else
            c = c >> 1;
    }
    return c;
}

uint16_t crc_update_buff(uint16_t crc, const void *buff, uint8_t len)
{
    uint8_t *p = (uint8_t*)buff;
    uint8_t *q = p + len;

    while(p != q)
        crc = crc_update(crc, *p++);

    return crc;
}

void send_buffer(int port_fd, uint16_t *crc, int *state, const void *buff, uint8_t len)
{
    uint8_t *p = (uint8_t*)buff;
    ssize_t ret;

    if (state && *state)
        return;

    while (len)
    {
        ret = write(port_fd, p, len);
        if (ret > 0)
        {
            DBG_PRINT_BUFF(p, ret);
            if (crc)
                *crc = crc_update_buff(*crc, p, ret);
            p += ret;
            len -= ret;
        }
        else
        {
            if (state)
                *state = CMD_FAIL;
            DBG_PRINT("\n ERROR SENDING DATA\n");
            break;
        }
    }
}

void send_buffer_va(int port_fd, uint16_t *crc, int *state, ...)
{
    uint8_t *ptr;
    uint8_t len;
    va_list va;

    if (state && *state)
        return;

    va_start(va, state);
    while((ptr = va_arg(va, void*)))
    {
        len = va_arg(va, int);
        if (crc && ((uint16_t*)ptr != crc))
            send_buffer(port_fd, crc, state, ptr, len);
        else
            send_buffer(port_fd, NULL, state, ptr, len);
    }
    va_end(va);
}

void recv_buffer(int port_fd, uint16_t *crc, int *state, void *buff, uint8_t len)
{
    uint8_t *p = (uint8_t*)buff;
    ssize_t ret;

    if (state && *state)
        return;

    while (len)
    {
        ret = read(port_fd, p, len);
        if (ret > 0)
        {
            DBG_PRINT_BUFF(p, ret);
            if (crc)
                *crc = crc_update_buff(*crc, p, ret);
            p += ret;
            len -= ret;
        } else
        {
            if (state)
                *state = CMD_FAIL;
            DBG_PRINT("\n ERROR RECEIVING DATA\n");
            break;
        }
    }
}

void recv_buffer_va(int port_fd, uint16_t *crc, int *state, ...)
{
    uint8_t *ptr;
    va_list va;

    if (state && *state)
        return;

    va_start(va, state);
    while((ptr = va_arg(va, void*)))
        recv_buffer(port_fd, crc, state, ptr, va_arg(va, int));
    va_end(va);
}

void send_command_va(rfid_t *rfid, int cmd, ...)
{
    uint8_t cmd_len = 4;
    uint16_t crc = CRC_INIT;
    va_list va;
    void *ptr;

    rfid->state = CMD_OK;

    DBG_PRINT("SEND:  ");

    // count data size
    va_start(va, cmd);
    while((ptr = va_arg(va, void*)))
        cmd_len += va_arg(va, int);
    va_end(va);

    // send header
    send_buffer_va(rfid->port_fd, &crc, &rfid->state, &cmd_len, 1, &rfid->addr, 1, &cmd, 1, NULL);
    // send data
    DBG_PRINT("\x1b[34m");
    va_start(va, cmd);
    while((ptr = va_arg(va, void*)))
        send_buffer(rfid->port_fd, &crc, &rfid->state, ptr, va_arg(va, int));
    va_end(va);
    DBG_PRINT("\x1b[0m");
    // send crc
    send_buffer(rfid->port_fd, &crc, &rfid->state, &crc, 2);

    DBG_PRINT("\n");
}

void recv_command_va(rfid_t *rfid, int flags, ...)
{
    uint16_t crc1 = CRC_INIT;
    uint16_t crc2;
    va_list va;
    void *ptr;
    ssize_t len;
    ssize_t data_len;

    if (rfid->state != CMD_OK)
        return;

    // read header
    if (!flags) // skip header function
    {
        DBG_PRINT("RECV:  ");
        recv_buffer(rfid->port_fd, &crc1, &rfid->state, &rfid->header, 4);
    }
    data_len = rfid->header.len - 5;

    // read data
    DBG_PRINT("\x1b[34m");
    va_start(va, flags);
    while(data_len && (ptr = va_arg(va, void*)))
    {
        len = va_arg(va, int);
        if (len < 0)
            len = rfid->header.len + len;
        recv_buffer(rfid->port_fd, &crc1, &rfid->state, ptr, len);
        data_len -= len;
    }
    va_end(va);
    // read remaining data
    if (data_len)
    {
        DBG_PRINT("\x1b[31m");
        while (data_len)
        {
            len = read(rfid->port_fd, &crc1, 1);
            if (len > 0)
            {
                DBG_PRINT_BUFF(&crc1, 1);
                data_len--;
            }
            else
                break;
        }
    }
    DBG_PRINT("\x1b[0m");
    // read crc
    recv_buffer(rfid->port_fd, NULL, &rfid->state, &crc2, 2);

    //test results
    if (rfid->header.status != CMD_OK)
        rfid->state = rfid->header.status;
    if (crc1 != crc2)
    {
        DBG_PRINT(" CRC_ERR 0x%04X != 0x%04X", crc1, crc2);
        rfid->state = CMD_CRC_ERR;
    }
    DBG_PRINT("\n");
}

void multi_packet_response(rfid_t *rfid, tag_inventory_cb tag_callback)
{
    int fd = rfid->port_fd;
    uint16_t crc1;
    uint16_t crc2;
    uint8_t antenna;
    uint8_t num;
    uint8_t prm;

    uint8_t d_len;
    uint8_t d_data[128];
    uint8_t rssi;

    if (rfid->state != CMD_OK)
        return;

    do
    {
        crc1 = CRC_INIT;
        DBG_PRINT("RECV:  ");
        recv_buffer(fd, &crc1, &rfid->state, &rfid->header, 4);
        DBG_PRINT("\x1b[34m");
        if (rfid->header.status < 0x05)
        {
            switch (rfid->header.cmd)
            {
                case 0x01:
                case 0x0f:
                case 0x1a:
                case 0x1b:
                    recv_buffer_va(fd, &crc1, &rfid->state, &antenna, 1, &num, 1, NULL);
                    for (size_t i = 0; i < num; i++)
                    {
                        recv_buffer(fd, &crc1, &rfid->state, &d_len, 1);
                        d_len &= 0x7f;
                        recv_buffer_va(fd, &crc1, &rfid->state, d_data, d_len, &rssi, 1, NULL);
                        tag_callback(rfid, antenna, num, i, d_len, d_data, rssi, 0);
                    }
                    break;
                case 0x19:
                    recv_buffer_va(fd, &crc1, &rfid->state, &antenna, 1, &num, 1, NULL);
                    for (size_t i = 0; i < num; i++)
                    {
                        recv_buffer_va(fd, &crc1, &rfid->state, &prm, 1, &d_len, 1, NULL);
                        recv_buffer_va(fd, &crc1, &rfid->state, d_data, d_len, &rssi, 1, NULL);
                        tag_callback(rfid, antenna, num, i, d_len, d_data, rssi, prm);
                    }
                    break;
                case 0x72:
                    recv_buffer_va(fd, &crc1, &rfid->state, &num, 1, NULL);
                    for (size_t i = 0; i < num; i++)
                    {
                        recv_buffer_va(fd, &crc1, &rfid->state, &antenna, 1, &d_len, 1, NULL);
                        recv_buffer_va(fd, &crc1, &rfid->state, d_data, d_len, &rssi, 1, &prm, 1, NULL);
                        tag_callback(rfid, antenna, num, i, d_len, d_data, rssi, prm);
                    }
                    break;
            }
        } else
            rfid->state = rfid->header.status;

        DBG_PRINT("\x1b[0m");
        recv_buffer(fd, NULL, &rfid->state, &crc2, 2);
        if (crc1 != crc2)
        {
            DBG_PRINT(" CRC_ERR 0x%04X != 0x%04X", crc1, crc2);
            rfid->state = CMD_CRC_ERR;
        }
        DBG_PRINT("\n");
    } while ((rfid->header.status == 0x03) && ((rfid->state == CMD_OK) || (rfid->state == CMD_CRC_ERR)));
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

int rfid_init(rfid_t *rfid, const char *device, speed_t speed)
{
    struct termios tty;

    memset(rfid, 0, sizeof(rfid_t));

    rfid->addr = 0xff;
    rfid_set_mask(rfid, RFID_MEM_EPC, 0x00, NULL, 0);
    
    rfid->port_fd = open(device, O_RDWR | O_NOCTTY);
    if (rfid->port_fd >= 0)
    {
        tcgetattr(rfid->port_fd, &tty);
        cfsetspeed(&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;                 // disable break processing
        tty.c_lflag = 0;                        // no signaling chars, no echo,
                                                // no canonical processing
        tty.c_oflag = 0;                        // no remapping, no delays
        tty.c_cc[VMIN]  = 0;                    // read doesn't block
        tty.c_cc[VTIME] = 250;                  // 25 seconds read timeout
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
        tty.c_cflag |= (CLOCAL | CREAD);        // ignore modem controls,
                                                // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        tty.c_oflag &=~(ONLCR|OCRNL);           // example how stupid default values are
        tty.c_iflag &=~(INLCR|ICRNL);           //

        tcsetattr(rfid->port_fd, TCSANOW, &tty);
        return 0;
    } else
        return CMD_FAIL;
}

void rfid_done(rfid_t *rfid)
{
    if (rfid->port_fd >= 0)
    {
        close(rfid->port_fd);
        rfid->port_fd = -1;
    }
}

void rfid_set_epc(rfid_t *rfid, void *epc, uint8_t len)
{
    memset(rfid->epc_data, 0, 32);
    if (epc && len && (len < 33))
        memcpy(rfid->epc_data, epc, len);
    rfid->epc_len = len;
}

void rfid_set_password(rfid_t *rfid, uint32_t password)
{
    rfid->password = password;
}

// Warning: mask_data must be valid during commands execution. There is no copy operation
void rfid_set_mask(rfid_t *rfid, uint8_t mask_mem, uint16_t mask_addr, uint8_t *mask_data, uint8_t mask_len)
{
    rfid->mask_mem = mask_mem;
    rfid->mask_addr = mask_addr;
    if (mask_data)
    {
        rfid->mask_data = mask_data;
        rfid->mask_len = mask_len;
        rfid->mask_data_len = (mask_len + 7) / 8;
    }
    else
    {
        rfid->mask_data = (uint8_t*)&rfid->header;
        rfid->mask_len = 0;
        rfid->mask_data_len = 0;
    }
}

void rfid_set_address(rfid_t *rfid, uint8_t addr)
{
    rfid->addr = addr;    
}

// ---------------------------------------------------------------------------------------------------------------------
// --- ISO18000-6C commands --------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int rfid_6c_tag_inventory(rfid_t *rfid, uint8_t q_value, uint8_t session, uint8_t tid_addr, uint8_t tid_len,
                           uint8_t target, uint8_t ant, uint8_t scan_time, rfid_stat_t *stat,
                           tag_inventory_cb tag_callback)
{
    if (stat)   // requesting statistic data
        q_value |= 0x80;
    else
        q_value &= 0x7f;

    send_command_va(rfid, 0x01, &q_value, 1, &session, 1, &rfid->mask_mem, 1,
                    &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    &tid_addr, 1, &tid_len, 1, &target, 1, &ant, 1, &scan_time, 1, NULL);
    multi_packet_response(rfid, tag_callback);

    if (stat)
        recv_command_va(rfid, 0, &stat->ant, 1, &stat->read_rate, 2, &stat->total_count, 4, NULL);
    return rfid->state;
}

int rfid_6c_read_data(rfid_t *rfid, uint8_t mem, uint16_t word_ptr, uint8_t num, uint8_t *response)
{
    send_command_va(rfid, 0x02, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &word_ptr, 2, &num, 1, &rfid->password, 4, &rfid->mask_mem, 1,
                    &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, response, num * 2, NULL);
    return rfid->state;
}

int rfid_6c_write_data(rfid_t *rfid, uint8_t w_num, uint8_t mem, uint16_t word_ptr, uint8_t *wdt)
{
    send_command_va(rfid, 0x03, &w_num, 1, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &word_ptr, 2, wdt, w_num * 2, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_write_epc(rfid_t *rfid, uint8_t e_num, uint8_t *w_epc)
{
    send_command_va(rfid, 0x04, &e_num, 1, &rfid->password, 4, w_epc, e_num * 2, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_kill_tag(rfid_t *rfid, uint32_t kill_password)
{
    send_command_va(rfid, 0x05, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &kill_password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_set_protection(rfid_t *rfid, uint8_t select, uint8_t protect)
{
    send_command_va(rfid, 0x06, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &select, 1, &protect, 1, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_block_erase(rfid_t *rfid, uint8_t mem, uint16_t word_ptr, uint8_t num)
{
    send_command_va(rfid, 0x07, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &word_ptr, 2, &num, 1, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_read_protection_config_epc(rfid_t *rfid)
{
    send_command_va(rfid, 0x08, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_read_protection_config(rfid_t *rfid)
{
    send_command_va(rfid, 0x09, &rfid->password, 4, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_read_protection_unlock(rfid_t *rfid)
{
    send_command_va(rfid, 0x0a, &rfid->password, 4, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_read_protection_check(rfid_t *rfid, uint8_t *protect)
{
    send_command_va(rfid, 0x0b, NULL);
    recv_command_va(rfid, 0, protect, 1, NULL);
    return rfid->state;
}

int rfid_6c_eas(rfid_t *rfid, uint8_t eas)
{
    send_command_va(rfid, 0x0c, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &rfid->password, 4, &eas, 1,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_eas_alert(rfid_t *rfid)
{
    send_command_va(rfid, 0x0d, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_single_tag_inventory(rfid_t *rfid, tag_inventory_cb tag_callback)
{
    send_command_va(rfid, 0x0f, NULL);
    multi_packet_response(rfid, tag_callback);
    return rfid->state;
}

int rfid_6c_write_block(rfid_t *rfid, uint8_t mem, uint8_t wordptr, uint8_t *data, uint8_t wordlen)
{
    send_command_va(rfid, 0x10, &wordlen, 1, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &wordptr, 1, data, wordlen * 2, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_6c_monza4qt_get_params(rfid_t *rfid, uint8_t *control)
{
    uint8_t tmp;
    send_command_va(rfid, 0x11, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    NULL);
    recv_command_va(rfid, 0, &tmp, 1, control, 1, NULL);
    return rfid->state;
}

int rfid_6c_monza4qt_set_params(rfid_t *rfid, uint8_t control)
{
    uint8_t tmp = 0;
    send_command_va(rfid, 0x12, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &tmp, 1, &control, 1, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}


int rfid_6c_extended_read(rfid_t *rfid, uint8_t mem, uint16_t word_ptr, uint8_t num, uint8_t *data)
{
    send_command_va(rfid, 0x15, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &word_ptr, 2, &num, 1, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, data, num * 2, NULL);
    return rfid->state;
}

int rfid_6c_extended_write(rfid_t *rfid, uint8_t mem, uint16_t word_ptr, uint8_t num, uint8_t *data)
{
    send_command_va(rfid, 0x16, &num, 1, &rfid->epc_len, 1, rfid->epc_data, rfid->epc_len * 2,
                    &mem, 1, &word_ptr, 2, &data, num * 2, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

void rfid_6c_tag_inventory_buffer (rfid_t *rfid, uint8_t q_value, uint8_t session, uint8_t tid_addr, uint8_t tid_len,
                           uint8_t target, uint8_t ant, uint8_t scan_time,
                           uint16_t *buff_cnt, uint16_t *tag_num)
{
    send_command_va(rfid, 0x18, &q_value, 1, &session, 1, &rfid->mask_mem, 1,
                    &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    &tid_addr, 1, &tid_len, 1, &target, 1, &ant, 1, &scan_time, 1, NULL);
    recv_command_va(rfid, 0, buff_cnt, 2, tag_num, 2, NULL);
}

int rfid_6c_mix_inventory(rfid_t *rfid, uint8_t q_value, uint8_t session, uint8_t read_mem, uint16_t read_addr, uint8_t read_len,
                          uint8_t target, uint8_t ant, uint8_t scan_time, rfid_stat_t *stat, tag_inventory_cb tag_callback)
{
    if (stat)   // requesting statistic data
        q_value |= 0x80;
    else
        q_value &= 0x7f;

    send_command_va(rfid, 0x19, &q_value, 1, &session, 1,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    &read_mem, 1, &read_addr, 2, &read_len, 1, &rfid->password, &target, 1, &ant, 1, &scan_time, 1, NULL);
    multi_packet_response(rfid, tag_callback);

    if (stat)
        recv_command_va(rfid, 0, &stat->ant, 1, &stat->read_rate, 2, &stat->total_count, 4, NULL);
    return rfid->state;
}

int rfid_6c_epc_inventory(rfid_t *rfid, uint8_t match_type, uint16_t match_len, uint16_t match_offset, uint8_t *epc_data,
                          tag_inventory_cb tag_callback)
{
    uint8_t len = (match_len + 7) / 8;
    send_command_va(rfid, 0x1a, &match_type, 1, &match_len, 2, &match_offset, 2, epc_data, len, NULL);
    multi_packet_response(rfid, tag_callback);
    return rfid->state;
}

int rfid_6c_qt_inventory(rfid_t *rfid, uint8_t q_value, uint8_t session, uint8_t target, uint8_t ant, uint8_t scan_time,
                         rfid_stat_t *stat, tag_inventory_cb tag_callback)
{
    if (stat)   // requesting statistic data
        q_value |= 0x80;
    else
        q_value &= 0x7f;

    send_command_va(rfid, 0x1b, &q_value, 1, &session, 1, &target, 1, &ant, 1, &scan_time, 1, NULL);
    multi_packet_response(rfid, tag_callback);

    if (stat)
        recv_command_va(rfid, 0, &stat->ant, 1, &stat->read_rate, 2, &stat->total_count, 4, NULL);
    return rfid->state;
}

// ---------------------------------------------------------------------------------------------------------------------
// --- ISO18000-6B commands --------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

// todo 0x50
// todo 0x51 
// todo 0x52 
// todo 0x53 
// todo 0x54 
// todo 0x55 

// ---------------------------------------------------------------------------------------------------------------------
// --- Reader custom commands ------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int rfid_x_get_reader_information(rfid_t *rfid, rfid_reader_info_t *info)
{
    send_command_va(rfid, 0x21, NULL);
    recv_command_va(rfid, 0, info, sizeof(rfid_reader_info_t), NULL);
    return rfid->state;
}

int rfid_x_set_working_frequency(rfid_t *rfid, uint8_t max_freq, uint8_t min_freq)
{
    send_command_va(rfid, 0x22, &max_freq, 1, &min_freq, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_reader_address(rfid_t *rfid, uint8_t new_addr)
{
    send_command_va(rfid, 0x24, &new_addr, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_inventory_time(rfid_t *rfid, uint8_t time)
{
    send_command_va(rfid, 0x25, &time, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_baud_rate(rfid_t *rfid, uint8_t baud_rate)
{
    send_command_va(rfid, 0x28, &baud_rate, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_rf_power(rfid_t *rfid, uint8_t power)
{
    send_command_va(rfid, 0x2f, &power, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_buzzer_control(rfid_t *rfid, uint8_t active_time, uint8_t silent_time, uint8_t repeat)
{
    send_command_va(rfid, 0x33, &active_time, 1, &silent_time, 1, &repeat, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_tag_customised_function(rfid_t *rfid, uint8_t *inlay_type)
{
    send_command_va(rfid, 0x3a, inlay_type, 1, NULL);
    recv_command_va(rfid, 0, inlay_type, 1, NULL);
    return rfid->state;
}

int rfid_x_set_antenna_multiplexing(rfid_t *rfid, uint8_t config)
{
    send_command_va(rfid, 0x3f, &config, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_buzzer(rfid_t *rfid, uint8_t enabled)
{
    send_command_va(rfid, 0x40, &enabled, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_gpio(rfid_t *rfid, uint8_t gpio)
{
    send_command_va(rfid, 0x46, &gpio, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_gpio(rfid_t *rfid, uint8_t *gpio)
{
    send_command_va(rfid, 0x47, NULL);
    recv_command_va(rfid, 0, gpio, 1, NULL);
    return rfid->state;
}

int rfid_x_get_serial(rfid_t *rfid, uint32_t *serial)
{
    send_command_va(rfid, 0x4c, NULL);
    recv_command_va(rfid, 0, serial, 4, NULL);
    return rfid->state;
}

int rfid_x_set_antenna_check(rfid_t *rfid, uint8_t enabled)
{
    send_command_va(rfid, 0x66, &enabled, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_comm_interface(rfid_t *rfid, uint8_t interface)
{
    send_command_va(rfid, 0x6a, &interface, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_antenna_return_loss_threshold(rfid_t *rfid, uint8_t *return_loss)
{
    uint8_t prm = 0;
    send_command_va(rfid, 0x6e, &prm, 1, NULL);
    recv_command_va(rfid, 0, return_loss, 1, NULL);
    return rfid->state;
}
int rfid_x_set_antenna_return_loss_threshold(rfid_t *rfid, uint8_t return_loss)
{
    uint8_t prm = return_loss | 0x80;
    send_command_va(rfid, 0x6e, &prm, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_max_epc_len(rfid_t *rfid, uint8_t len)
{
    send_command_va(rfid, 0x70, &len, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_max_epc_len(rfid_t *rfid, uint8_t *len)
{
    send_command_va(rfid, 0x71, NULL);
    recv_command_va(rfid, 0, len, 1, NULL);
    return rfid->state;
}

int rfid_x_buffer_read(rfid_t *rfid, tag_inventory_cb tag_callback)
{
    send_command_va(rfid, 0x72, NULL);
    multi_packet_response(rfid, tag_callback);
    return rfid->state;
}

int rfid_x_buffer_clear(rfid_t *rfid)
{
    send_command_va(rfid, 0x73, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_buffer_tag_cnt(rfid_t *rfid, uint16_t *cnt)
{
    send_command_va(rfid, 0x74, NULL);
    recv_command_va(rfid, 0, cnt, 2, NULL);
    return rfid->state;
}

int rfid_x_set_real_time_inventory_params(rfid_t *rfid, uint8_t tag_protocol, uint8_t read_pause_time,
                                          uint8_t filter_time, uint8_t q_value, uint8_t session, uint8_t tid_addr, uint8_t tid_len)
{
    send_command_va(rfid, 0x75, &tag_protocol, 1, &read_pause_time, 1, &filter_time, 1,
                    &q_value, 1, &session, 1,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len,
                    &tid_addr, 1, &tid_len, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_set_reader_mode(rfid_t *rfid, uint8_t read_mode)
{
    send_command_va(rfid, 0x76, &read_mode, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_reader_mode(rfid_t *rfid, rfid_reader_working_mode_t *reader_mode)
{
    send_command_va(rfid, 0x77, NULL);
    recv_command_va(rfid, 0, reader_mode, sizeof(rfid_reader_working_mode_t), NULL);
    return rfid->state;
}

int rfid_x_set_heartbeat_time(rfid_t *rfid, uint8_t time)
{
    uint8_t prm = 0x80 | time;
    send_command_va(rfid, 0x78, &prm, 1, NULL);
    recv_command_va(rfid, 0, &prm, 1, NULL);
    return rfid->state;
}

int rfid_x_get_heartbeat_time(rfid_t *rfid, uint8_t *time)
{
    uint8_t prm = 0;
    send_command_va(rfid, 0x78, &prm, 1, NULL);
    recv_command_va(rfid, 0, time, 1, NULL);
    return rfid->state;
}

int rfid_x_set_rf_power_write (rfid_t *rfid, uint8_t power)
{
    send_command_va(rfid, 0x79, &power, 1, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_rf_power_write (rfid_t *rfid, uint8_t *power)
{
    send_command_va(rfid, 0x7a, NULL);
    recv_command_va(rfid, 0, power, 1, NULL);
    return rfid->state;
}

int rfid_x_set_write_retry_cnt(rfid_t *rfid, uint8_t cnt)
{
    uint8_t prm = 0x80 | cnt;
    send_command_va(rfid, 0x7b, &prm, 1, NULL);
    recv_command_va(rfid, 0, &prm, 1, NULL);
    return rfid->state;
}

int rfid_x_get_write_retry_cnt(rfid_t *rfid, uint8_t *cnt)
{
    uint8_t prm = 0;
    send_command_va(rfid, 0x7b, &prm, 1, NULL);
    recv_command_va(rfid, 0, cnt, 1, NULL);
    return rfid->state;
}

int rfid_x_set_customised_function_password (rfid_t *rfid, uint32_t password)
{
    send_command_va(rfid, 0x7d, password, 4, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_x_get_customised_function_password (rfid_t *rfid, uint32_t *password)
{
    send_command_va(rfid, 0x7e, NULL);
    recv_command_va(rfid, 0, &password, 4, NULL);
    return rfid->state;
}

int rfid_x_set_profile(rfid_t *rfid, uint8_t profile)
{
    uint8_t prm = 0x80 | profile;
    send_command_va(rfid, 0x7f, &prm, 1, NULL);
    recv_command_va(rfid, 0, &prm, 1, NULL);
    return rfid->state;
}

int rfid_x_get_profile(rfid_t *rfid, uint8_t *profile)
{
    uint8_t prm = 0;
    send_command_va(rfid, 0x7f, &prm, 1, NULL);
    recv_command_va(rfid, 0, profile, 1, NULL);
    return rfid->state;
}

int rfid_x_set_drm(rfid_t *rfid, uint8_t drm_mode)
{
    uint8_t prm = 0x80 | drm_mode;
    send_command_va(rfid, 0x90, &prm, 1, NULL);
    recv_command_va(rfid, 0, &prm, 1, NULL);
    return rfid->state;
}

int rfid_x_get_drm(rfid_t *rfid, uint8_t *drm_mode)
{
    uint8_t prm = 0;
    send_command_va(rfid, 0x90, &prm, 1, NULL);
    recv_command_va(rfid, 0, drm_mode, 1, NULL);
    return rfid->state;
}

int rfid_x_get_antenna_return_loss(rfid_t *rfid, uint32_t test_freq, uint8_t antenna, uint8_t *return_loss)
{
    send_command_va(rfid, 0x91, &test_freq, 4, &antenna, 1, NULL);
    recv_command_va(rfid, 0, return_loss, 1, NULL);
    return rfid->state;
}

int rfid_x_get_temperature(rfid_t *rfid, int *temp)
{
    uint8_t ret[2];
    send_command_va(rfid, 0x92, NULL);
    recv_command_va(rfid, 0, ret, 2, NULL);
    if (rfid->state == CMD_OK)
    {
        if (ret[0])
            *temp = ret[1];
        else
            *temp = -ret[1];
    }
    return rfid->state;
}

// ---------------------------------------------------------------------------------------------------------------------
// -- EM4325 commands --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

int rfid_em4325_sync_timestamp (rfid_t *rfid, uint32_t utc_time)
{
    send_command_va(rfid, 0x85, utc_time, 4, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}

int rfid_em4325_get_temperature(rfid_t *rfid, uint8_t send_uid, uint8_t new_sample, uint8_t *uid,
                                uint32_t *sensor_data, uint32_t *utc_time)
{
    send_command_va(rfid, 0x86, rfid->port_fd,
                    &rfid->epc_len, 1, &rfid->epc_data, rfid->epc_len * 2, &send_uid, 1, &new_sample, 1, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, uid, -13, sensor_data, 4, utc_time, 4, NULL);
    return rfid->state;
}

int rfid_em4325_spi(rfid_t *rfid,
                    uint8_t cmd_len, uint8_t res_len, uint8_t clk, uint8_t delay, uint8_t interval, uint8_t *cmd, uint8_t *res)
{
    send_command_va(rfid, 0x87, &cmd_len, 1, &rfid->epc_len, 1, &rfid->epc_data, rfid->epc_len * 2,
                    &res_len, 1, &clk, 1, &delay, 1, &interval, 1, cmd, cmd_len, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, res, res_len, NULL);
    return rfid->state;
}

int rfid_em4325_reset_alert(rfid_t *rfid)
{
    send_command_va(rfid, 0x88, rfid->port_fd,
                    &rfid->epc_len, 1, &rfid->epc_data, rfid->epc_len * 2, &rfid->password, 4,
                    &rfid->mask_mem, 1, &rfid->mask_addr, 2, &rfid->mask_len, 1, rfid->mask_data, rfid->mask_data_len, NULL);
    recv_command_va(rfid, 0, NULL);
    return rfid->state;
}
