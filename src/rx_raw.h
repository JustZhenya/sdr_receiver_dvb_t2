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
#ifndef RX_RAW_H
#define RX_RAW_H

#include <QObject>
#include <QTime>
#include <QApplication>
#include <QFile>
#include <string>
#include <vector>

#include "rx_base.h"

class rx_raw : virtual public rx_base<float>
{
public:
    explicit rx_raw(QObject* parent = nullptr);
    virtual ~rx_raw();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    void update_gain_frequency_direct() override;
    const QString dev_name() override
    {
        return "RAW file";
    }
    const QString thread_name() override
    {
        return "rx_raw";
    }

private:
    void rx_execute(void *ptr, int nsamples);

private:

    bool done = true;

    QFile fd{};
    QString filename{};
    int bytes{0};
    std::vector<char> tmpbuf{};
    std::vector<float> floatbuf{};
    int8_t * buf8{nullptr};
    int16_t * buf16{nullptr};
    float * buf32{nullptr};
    std::chrono::time_point<std::chrono::high_resolution_clock> tt{};

    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    void update_gain_frequency() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // RX_RAW_H
