/*
 *  Copyright 2020 Oleg Malyutin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef RX_SDRPLAY_H
#define RX_SDRPLAY_H

#ifdef USE_SDRPLAY

#include <QObject>
#include <QTime>
#include <QApplication>
#include <string>
#include <vector>

#include "sdrplay/mir_sdr.h"
#include "rx_base.h"

class rx_sdrplay : virtual public rx_base<int16_t>
{
public:
    explicit rx_sdrplay(QObject* parent = nullptr);
    virtual ~rx_sdrplay();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    const QString dev_name() override
    {
        return "SDRPlay";
    }
    const QString thread_name() override
    {
        return "rx_sdrplay";
    }

private:
    constexpr static int max_symbol = FFT_32K + FFT_32K / 4 + P1_LEN;
    //const int max_blocks = max_symbol / 384 * 32;
    constexpr static  int norm_blocks = max_symbol / 384 * 4;
    std::vector<int16_t> i_buffer;
    std::vector<int16_t> q_buffer;
    bool done = true;

    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // USE_SDRPLAY

#endif // RX_SDRPLAY_H
