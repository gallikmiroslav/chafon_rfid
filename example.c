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
#include <stdlib.h>
#include <unistd.h>
#include "rfid.h"

void tag(rfid_t *rfid, uint8_t ant, uint16_t num, uint16_t id, uint8_t data_len, uint8_t *data,
         uint8_t rssi, uint8_t prm)
{
    // todo do something
}

void tests(rfid_t *rfid)
{
    rfid_reader_info_t ri;
    rfid_stat_t stat;
    rfid_reader_working_mode_t wm;
    uint8_t tmp;

    rfid_set_address(rfid, 0x00);

    //rfid_x_buzzer_control(rfid, 1, 1, 3);

    //rfid_x_get_reader_information(rfid, &ri);
    //rfid_x_get_reader_mode(rfid, &wm);


    rfid_set_mask(rfid, RFID_MEM_TID, 0x00, NULL, 0);
    rfid_x_set_reader_mode(rfid, 0x00);
    for (int i = 0; i < 5000; ++i)
    {
        rfid_6c_tag_inventory(rfid, 0x04, 0x00, 0, 4, 0, 0x82, 10, &stat, tag);
        sleep(1);
    }

    rfid_x_get_reader_information(rfid, &ri);
}

int main()
{
    rfid_t rf;

    if (rfid_init(&rf, "/dev/ttyUSB0", B57600))
    {
        printf("Error opening serial port\n");
        exit(-1);
    }

    tests(&rf);
    rfid_done(&rf);

    return 0;
}
