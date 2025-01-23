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
#ifndef RX_MIRISDR_H
#define RX_MIRISDR_H

#include <QObject>
#include <QTime>
#include <QApplication>
#include <string>
#include <vector>
#include <mirisdr.h>

#include "DVB_T2/dvbt2_demodulator.h"

class rx_miri : public QObject
{
    Q_OBJECT
public:
    explicit rx_miri(QObject* parent = nullptr);
    ~rx_miri();

    std::string error (int err);
    int get(std::string &_ser_no, std::string &_hw_ver);
    int init(double _rf_frequency, int _gain_db);
    dvbt2_demodulator* demodulator;

signals:
    void execute(int _len_in, complex* _in, float _level_estimate, signal_estimate* signal_);
    void status(int _err);
    void radio_frequency(double _rf);
    void level_gain(int _gain);
    void stop_demodulator();
    void finished();
    void buffered(int nbuffers, int totalbuffers);

public slots:
    void start();
    void stop();
    void set_rf_frequency();
    void set_gain(bool force=false);
    void set_gain_db(int gain);
private:
    void rx_execute(void *ptr, int nsamples);
    static void callback(unsigned char *buf, uint32_t len, void *context);

private:
    QThread* thread;
    signal_estimate signal{};

    int gain_db;
    bool gain_changed;
    double rf_frequency;
    double ch_frequency;
    bool frequency_changed;

    float sample_rate;
    int max_len_out;

    std::vector<complex> buffer_a;
    std::vector<complex> buffer_b;
    complex* ptr_buffer;
    int  blocks = 1;
    const int len_out_device = 128*1024*4;
    const int max_blocks = 256;
    int len_buffer = 0;
    bool swap_buffer = true;

    int64_t rf_bandwidth_hz;
    int64_t sample_rate_hz;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    float frequency_offset = 0.0f;
    bool change_frequency = false;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;
    bool agc = false;
    bool change_gain = false;
    bool done = true;
    int gain_offset = 0;

    // spur list
    static constexpr double spurs[]=
    {
        4.82e8,
        5.04e8,
        5.28e8,
        6.48e8,
        6.72e8,
        6.96e8,
        7.20e8,
        7.44e8,
        7.68e8,
        7.92e8,
        8.16e8,
        8.40e8,
    };


    mirisdr_dev_t*_dev;
    int err;
    convert_iq<int16_t> conv{};
    void reset();
};

#endif // RX_MIRISDR_H
