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
#include "rx_miri.h"
#include "rx_base.cpp"


//----------------------------------------------------------------------------------------------------------------------------
rx_miri::rx_miri(QObject *parent) : rx_base(parent)
{
    len_out_device = 128 * 1024 * 4;
    max_blocks = 256;
    GAIN_MAX = 104;
    GAIN_MIN = 0;
    blocking_start = true;
    conv.init(2, 1.0f / (1 << 15), 0.04f, 0.015f);

}
//-------------------------------------------------------------------------------------------
rx_miri::~rx_miri()
{
    fprintf(stderr,"rx_miri::~rx_miri\n");
}
//-------------------------------------------------------------------------------------------
std::string rx_miri::error (int err)
{
    switch (err) {
       case 0:
          return "Success";
       case -1:
          return "Mirisdr error";
       default:
          return "Unknown error "+std::to_string(err);
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_miri::get(std::string &_ser_no, std::string &_hw_ver)
{

  static std::vector<std::string> devices;
  static std::vector<unsigned char> hw_ver;
  std::string label;

  devices.resize(0);
  hw_ver.resize(0);
  for (unsigned int i = 0; i < mirisdr_get_device_count(); i++) {
    devices.push_back( std::string(mirisdr_get_device_name( i ) ));
  }
  if(devices.size() == 0)
    return -1;
  _ser_no = devices[0];
  _hw_ver = "0";
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
int rx_miri::hw_init(uint32_t _rf_frequency, int _gain_db)
{
    int ret = 0;
    static char fmt[] = "336_S16\x00";
    //static char fmt[] = "504_S16\x00";
    fprintf(stderr,"miri init\n");
    sample_rate = 9000000.0f;
    ret = mirisdr_open( &_dev, 0 );
    if(ret != 0) return ret;
#ifdef HAVE_SET_HW_FALVOUR
  unsigned int flavour = 2;
  mirisdr_set_hw_flavour( _dev, mirisdr_hw_flavour_t(flavour));
#endif
    mirisdr_set_sample_format( _dev, fmt);
    mirisdr_set_sample_rate( _dev, sample_rate );
    mirisdr_set_center_freq( _dev, rf_frequency );
    ret = mirisdr_set_bandwidth( _dev, 12000000 );
    if(ret != 0) return ret;
    return ret;
}
//-------------------------------------------------------------------------------------------
void rx_miri::set_biastee(const bool state)
{
    mirisdr_set_bias( _dev, state );
}
//-------------------------------------------------------------------------------------------
int rx_miri::hw_set_frequency()
{
    int err = mirisdr_set_center_freq( _dev, uint32_t(rf_frequency) );
    if(err == 0)
    {
        conv.enable_anti_spur(false);
        for(unsigned k = 0 ; k < sizeof(spurs)/sizeof(spurs[0]) ; k++)
            if(std::abs(rf_frequency-spurs[k])<double(sample_rate))
            {
                conv.set_anti_spur(std::polar(1.f,float((spurs[k] - rf_frequency) * M_PI_X_2 / double(sample_rate))));
                break;
            }
    }
    return err;
}
//-------------------------------------------------------------------------------------------
void rx_miri::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_miri::hw_set_gain()
{
    return mirisdr_set_tuner_gain( _dev, gain );
}
//-------------------------------------------------------------------------------------------
void rx_miri::on_gain_changed()
{
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_miri::callback(unsigned char *buf, uint32_t len, void *context)
{
    if(!buf) return;

    rx_miri *ctx = static_cast<rx_miri*>(context);

    ctx->rx_execute(buf,len/2);
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_miri::update_gain_frequency_direct()
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_miri::update_gain_frequency()
{
}
//-------------------------------------------------------------------------------------------
void rx_miri::rx_execute(void *in_ptr, int nsamples)
{
    int16_t * ptr = (int16_t*)in_ptr;
    if(!done)
        return;
    nsamples /= 2;
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,nsamples, &ptr[0], &ptr[1],ptr_buffer,level_detect,signal);

    rx_base::rx_execute(nsamples, level_detect);
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_miri::hw_start()
{
    int err = 0;
    err = mirisdr_reset_buffer(_dev);
    err = mirisdr_read_async( _dev, callback, (void *)this, 64, len_out_device );
    if(err == 0)
        mirisdr_set_bias( _dev, 0 );
    mirisdr_close(_dev);
    len_buffer = 0;
    fprintf(stderr, "mirisdr start rx %d\n", err);
    return err;
}
//-------------------------------------------------------------------------------------------
void rx_miri::hw_stop()
{
    done = false;
    mirisdr_cancel_async( _dev );
    fprintf(stderr, "mirisdr stop\n");
}
//-------------------------------------------------------------------------------------------
