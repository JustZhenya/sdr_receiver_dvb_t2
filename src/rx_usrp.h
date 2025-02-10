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
#ifndef RX_USRP_H
#define RX_USRP_H

#include <QObject>
#include <QTime>
#include <QApplication>
#include <string>
#include <vector>
#include <uhd/config.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/convert.hpp>

#include "rx_base.h"

class rx_usrp : virtual public rx_base<int16_t>
{
public:
    explicit rx_usrp(QObject* parent = nullptr);
    virtual ~rx_usrp();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    void update_gain_frequency_direct() override;
    const QString dev_name() override
    {
        return "USRP";
    }
    const QString thread_name() override
    {
        return "rx_usrp";
    }

private:
    void rx_execute(void *ptr, int nsamples);

private:
    bool done = true;

    std::vector<uint16_t> in_buf{};
    ::uhd::usrp::multi_usrp::sptr _dev;
    ::uhd::rx_streamer::sptr _rx_stream;
    ::uhd::rx_metadata_t _metadata;
    double _recv_timeout{0.1};
    bool _recv_one_packet{0};
    size_t _samps_per_packet{0};
    int chan{0};

    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    void update_gain_frequency() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // RX_USRP_H
