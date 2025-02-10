/*
 *  Copyright 2025 vladisslav2011 vladisslav2011@gmail.com.
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
#ifndef RX_HACKRF_H
#define RX_HACKRF_H

#include <QObject>
#include <QTime>
#include <QApplication>
#include <string>
#include <libhackrf/hackrf.h>

#include "rx_base.h"

class rx_hackrf : virtual public rx_base<int8_t>
{
public:
    explicit rx_hackrf(QObject* parent = nullptr);
    virtual ~rx_hackrf();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    void update_gain_frequency_direct() override;
    const QString dev_name() override
    {
        return "HackRF";
    }
    const QString thread_name() override
    {
        return "rx_hackrf";
    }
private:
    void rx_execute(void *ptr, int nsamples);
    static int callback(hackrf_transfer* transfer);

private:
    bool done = true;
    hackrf_device *_dev;
    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    void update_gain_frequency() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // RX_HACKRF_H
