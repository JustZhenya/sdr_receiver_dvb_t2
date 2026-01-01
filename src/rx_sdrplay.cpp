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
#include "rx_sdrplay.h"
#include "rx_base.cpp"

//-------------------------------------------------------------------------------------------
rx_sdrplay::rx_sdrplay(QObject *parent) : QObject(parent)
{
    max_blocks = max_symbol / 384 * 32;
    GAIN_MAX = 78;
    GAIN_MIN = 0;
    blocking_start = true;
    conv.init(1, 1.0f / (1 << 14), 0.04f, 0.02f);
}
//-------------------------------------------------------------------------------------------
rx_sdrplay::~rx_sdrplay()
{
}
//-------------------------------------------------------------------------------------------
std::string rx_sdrplay::error (int err)
{
    switch (err) {
       case mir_sdr_Success:
          return "Success";
       case mir_sdr_Fail:
          return "Fail";
       case mir_sdr_InvalidParam:
          return "Invalid parameter";
       case mir_sdr_OutOfRange:
          return "Out of range";
       case mir_sdr_GainUpdateError:
          return "Gain update error";
       case mir_sdr_RfUpdateError:
          return "Rf update error";
       case mir_sdr_FsUpdateError:
          return "Fs update error";
       case mir_sdr_HwError:
          return "Hardware error";
       case mir_sdr_AliasingError:
          return "Aliasing error";
       case mir_sdr_AlreadyInitialised:
          return "Already initialised";
       case mir_sdr_NotInitialised:
          return "Not initialised";
       case mir_sdr_NotEnabled:
          return "Not enabled";
       case mir_sdr_HwVerError:
          return "Hardware Version error";
       case mir_sdr_OutOfMemError:
          return "Out of memory error";
       case mir_sdr_HwRemoved:
          return "Hardware removed";
       default:
          return "Unknown error";
    }
}
//-------------------------------------------------------------------------------------------
mir_sdr_ErrT rx_sdrplay::get(char* &_ser_no, unsigned char &_hw_ver)
{
    mir_sdr_ErrT err;
//    mir_sdr_DebugEnable(1);

    mir_sdr_DeviceT devices[4];
    unsigned int numDevs;
    err = mir_sdr_GetDevices(&devices[0], &numDevs, 4);

    if(err != 0) return err;

    _ser_no = devices[0].SerNo;
    _hw_ver = devices[0].hwVer;
    err = mir_sdr_SetDeviceIdx(0);

    return err;
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_init(uint32_t _rf_frequence, int _gain_db)
{
    mir_sdr_ErrT err;

    mir_sdr_Uninit();
    err = mir_sdr_DCoffsetIQimbalanceControl(0, 0);

    if(err != 0) return err;

    sample_rate = 9200000.0f; // max for 10bit (10000000.0f for 8bit)
    double sample_rate_mhz = static_cast<double>(sample_rate) / 1.0e+6;
    double rf_chanel_mhz = static_cast<double>(rf_frequency) / 1.0e+6;
    err = mir_sdr_Init(gain_db, sample_rate_mhz, rf_chanel_mhz,
                                 mir_sdr_BW_8_000, mir_sdr_IF_Zero, &len_out_device);

    if(err != 0) return err;

    max_len_out = len_out_device * max_blocks;
    i_buffer.resize(len_out_device);
    q_buffer.resize(len_out_device);

    return err;
}

//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_set_frequency()
{
    return mir_sdr_SetRf(rf_frequency, 1, 0);
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_set_gain()
{
    return mir_sdr_SetGr(GAIN_MAX-gain, 1, 0);
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::on_gain_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::rx_start()
{
    unsigned int first_sample_num;
    mir_sdr_ErrT err = mir_sdr_Success;
    int gr_changed = 0;
    int rf_changed = 0;
    int fs_changed = 0;
    float level_detect=std::numeric_limits<float>::max();

    while(done && (err == mir_sdr_Success)) {

        for(int n = 0; n < blocks; ++n) {

            err = mir_sdr_ReadPacket(&i_buffer[0], &q_buffer[0], &first_sample_num,
                                     &gr_changed, &rf_changed, &fs_changed);
            if(err != 0) {
                emit status(err);
            }
            conv.execute(0,len_out_device, &i_buffer, &q_buffer, ptr_buffer, level_detect, signal);
            rx_base::rx_execute(len_out_device, level_detect);
        }
    }
    mir_sdr_Uninit();
    mir_sdr_ReleaseDeviceIdx();
    return (err == mir_sdr_Success) ? 0 : -1;
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::rx_stop()
{
    done = false;
}
//-------------------------------------------------------------------------------------------
