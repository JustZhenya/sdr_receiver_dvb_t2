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
#include "rx_raw.h"
#include "rx_base.cpp"
#include <QFileDialog>
#include <QFileInfo>

//----------------------------------------------------------------------------------------------------------------------------
rx_raw::rx_raw(QObject *parent) : rx_base(parent)
{
    len_out_device = 128 * 1024 * 4;
    tmpbuf.resize(len_out_device);
    floatbuf.resize(len_out_device);
    max_blocks = 128;
    GAIN_MAX = 0;
    GAIN_MIN = 0;
    blocking_start = true;
    conv.init(2, 1.0f / (1 << 15), 0.03f, 0.015f);

}
//-------------------------------------------------------------------------------------------
rx_raw::~rx_raw()
{
    fprintf(stderr,"rx_raw::~rx_raw\n");
}
//-------------------------------------------------------------------------------------------
std::string rx_raw::error (int err)
{
    switch (err) {
       case 0:
          return "Success";
       case -1:
          return "Error -1";
       default:
          return "Other error " + std::to_string(err);
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_raw::get(std::string &_ser_no, std::string &_hw_ver)
{

    static std::vector<std::string> devices;
    static std::vector<unsigned char> hw_ver;
    std::string label;

    devices.resize(0);
    hw_ver.resize(0);
    filename = QFileDialog::getOpenFileName(QApplication::activeWindow(), "Open RAW IQ file","",
                                            "RAW (*.raw)");
    if(filename.isEmpty())
        return -1;
    QFileInfo info(filename);
    QStringList list = info.baseName().split('_');
    if(list.size() < 5)
        return -2;
    bool sr_ok = false;
    sample_rate = list.at(4).toLongLong(&sr_ok);
    if(!sr_ok)
        return -3;
    const auto fmt_str = list.at(5);
    if(fmt_str == "8")
    {
        bytes = 1;
        buf8 = reinterpret_cast<int8_t *>(&tmpbuf[0]);
    }else if(fmt_str == "16")
    {
        bytes = 2;
        buf16 = reinterpret_cast<int16_t *>(&tmpbuf[0]);
    }else if(fmt_str == "fc"){
        bytes = 4;
        buf32 = reinterpret_cast<float *>(&tmpbuf[0]);
    }else
        return -4;
    fd.setFileName(filename);
    if(!fd.open(QIODevice::ReadOnly))
        return -5;
    _ser_no = filename.toStdString();
    _hw_ver = "0";
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
int rx_raw::hw_init(uint32_t _rf_frequency, int _gain_db)
{
    (void)_rf_frequency;
    (void)_gain_db;
    return 0;
}
//-------------------------------------------------------------------------------------------
int rx_raw::hw_set_frequency()
{
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_raw::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_raw::hw_set_gain()
{
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_raw::on_gain_changed()
{
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_raw::update_gain_frequency_direct()
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_raw::update_gain_frequency()
{
}
//-------------------------------------------------------------------------------------------
void rx_raw::rx_execute(void *in_ptr, int nsamples)
{
    float * ptr = (float*)in_ptr;
    if(!done)
        return;
    nsamples /= 2;
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,nsamples, &ptr[0], &ptr[1],ptr_buffer,level_detect,signal);

    rx_base::rx_execute(nsamples, level_detect);
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_raw::hw_start()
{
    int err = 0;
    tt = std::chrono::high_resolution_clock::now();
    while(done)
    {
        auto bytes_read = fd.read(&tmpbuf[0], tmpbuf.size());
        auto now = std::chrono::high_resolution_clock::now();
        if(bytes_read<0)
            break;
        auto halfsamples=bytes_read / bytes;
        std::chrono::duration<double, std::milli> elapsed = now - tt;
        const double blocktime_ms = 1000. * double(halfsamples) / double(sample_rate * 2);
        while(elapsed.count() < blocktime_ms)
        {
            QThread::msleep(blocktime_ms - elapsed.count());
            now = std::chrono::high_resolution_clock::now();
            elapsed = now - tt;
        }
        tt = now;
        switch(bytes)
        {
        case 1:
            for(int k=0;k<halfsamples;k++)
                floatbuf[k] = buf8[k] * (1.f/127.f);
            rx_execute(&floatbuf[0],halfsamples);
        break;
        case 2:
            for(int k=0;k<halfsamples;k++)
                floatbuf[k] = buf16[k] * (1.f/32767.f);
            rx_execute(&floatbuf[0],halfsamples);
        break;
        case 4:
        default:
            rx_execute(&buf32[0],halfsamples);
        }
        if(bytes_read<int64_t(tmpbuf.size()))
            if(!fd.seek(0))
                break;
    }
    fd.close();
    return err;
}
//-------------------------------------------------------------------------------------------
void rx_raw::hw_stop()
{
    done = false;
}
//-------------------------------------------------------------------------------------------
