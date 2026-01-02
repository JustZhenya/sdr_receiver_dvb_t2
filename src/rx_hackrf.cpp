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
#include "rx_hackrf.h"

#include "rx_base.cpp"

//----------------------------------------------------------------------------------------------------------------------------
rx_hackrf::rx_hackrf(QObject *parent) : rx_base(parent)
{
    len_out_device = 128 * 1024 * 4;
    max_blocks = 256;
    GAIN_MAX = 100;
    GAIN_MIN = 0;
    blocking_start = false;
    conv.init(2, 1.0f / (1 << 7), 0.04f, 0.015f);
    hackrf_init(); /* call only once before the first open */
    fprintf(stderr,"rx_hackrf::rx_hackrf\n");

}
//-------------------------------------------------------------------------------------------
rx_hackrf::~rx_hackrf()
{
    hackrf_exit(); /* call only once after last close */
    fprintf(stderr,"rx_hackrf::~rx_hackrf\n");
}
//-------------------------------------------------------------------------------------------
std::string rx_hackrf::error (int err)
{
    switch (err) {
       case HACKRF_SUCCESS:
          return "Success";
       case HACKRF_ERROR_INVALID_PARAM:
          return "Invalid parameter";
       case HACKRF_ERROR_NOT_FOUND:
          return "HackRF not found";
       case HACKRF_ERROR_BUSY:
          return "Device busy";
       case HACKRF_ERROR_NO_MEM:
          return "Out of memory error";
       case HACKRF_ERROR_LIBUSB:
          return "Libusb error";
       case HACKRF_ERROR_THREAD:
          return "HackRF thread error";
       case HACKRF_ERROR_STREAMING_THREAD_ERR:
          return "HackRF streaming thread error";
       case HACKRF_ERROR_STREAMING_STOPPED:
          return "HackRF error: streaming stopped";
       case HACKRF_ERROR_STREAMING_EXIT_CALLED:
          return "HackRF error: exit called";
       case HACKRF_ERROR_USB_API_VERSION:
          return "HackRF wrong USB api version";
       case HACKRF_ERROR_OTHER:
          return "HackRF error: other";
       default:
          return "Unknown error";
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf::get(std::string &_ser_no, std::string &_hw_ver)
{

  static std::vector<std::string> devices;
  static std::vector<unsigned char> hw_ver;
  std::string label;

  devices.resize(0);
  hw_ver.resize(0);
#ifdef LIBHACKRF_HAVE_DEVICE_LIST
  hackrf_device_list_t *list = hackrf_device_list();
  
  for (int i = 0; i < list->devicecount; i++) {
    if (list->serial_numbers[i]) {
      std::string serial (list->serial_numbers[i] );
      if (serial.length() > 6)
        serial = serial.substr(serial.length() - 6, 6);
      devices.push_back(serial);
      hw_ver.push_back(0);
    } else {
      devices.push_back("hackrf"); /* will pick the first one, serial number is required for choosing a specific one */
      hw_ver.push_back(0);
    }
  }
  
  hackrf_device_list_free(list);
#else
  int ret;
  hackrf_device *dev = NULL;
  ret = hackrf_open(&dev);
  if ( HACKRF_SUCCESS == ret )
  {
    std::string args = "hackrf=0";

    devices.push_back( args );

    ret = hackrf_close(dev);
  }
#endif
  if(devices.size() == 0)
    return HACKRF_ERROR_NOT_FOUND;
  _ser_no = devices[0];
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf::hw_init(uint32_t _rf_frequency, int _gain_db)
{
    int ret = 0;
    fprintf(stderr,"hackrf init\n");
    sample_rate = 10000000.0f; // max for 10bit (10000000.0f for 8bit)
    ret = hackrf_open( &_dev );
    if(ret != 0) return ret;
    hackrf_set_sample_rate( _dev, sample_rate );
    hackrf_set_freq( _dev, uint64_t(rf_frequency) );
    uint32_t bw = hackrf_compute_baseband_filter_bw( uint32_t(8000000.0) );
    ret = hackrf_set_baseband_filter_bandwidth( _dev, bw );
    return ret;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::set_biastee(const bool state)
{
    if(_dev)
        hackrf_set_antenna_enable(_dev, state);
}
//-------------------------------------------------------------------------------------------
int rx_hackrf::hw_set_frequency()
{
    return hackrf_set_freq( _dev, uint64_t(rf_frequency) );
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_hackrf::hw_set_gain()
{
    int err = 0;
    int gain_db = gain;
    int clip_gain =0;
    if(gain_db>=40)
    {
        clip_gain = 40;
        gain_db -=40;
    }else {
        clip_gain = gain_db;
        gain_db %= 8;
        clip_gain -= gain_db;
    }
    err=hackrf_set_lna_gain( _dev, uint32_t(clip_gain) );
    fprintf(stderr,"LNA=%d\n",clip_gain);
    clip_gain=0;
    if(gain_db)
    {
        if(gain_db>=14)
        {
            clip_gain = 14;
            gain_db -= 14;
        }else
            clip_gain = 0;
    }
    err|=hackrf_set_amp_enable( _dev, clip_gain?1:0 );
    fprintf(stderr,"AMP=%d\n",clip_gain);
    clip_gain=0;
    if(gain_db)
    {
        if(gain_db>=50)
        {
            clip_gain = 50;
        }else
            clip_gain = gain_db;
    }
    err|=hackrf_set_vga_gain( _dev, uint32_t(clip_gain) );
    fprintf(stderr,"VGA=%d\n",clip_gain);
    return err;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::on_gain_changed()
{
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_hackrf::update_gain_frequency_direct()
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::update_gain_frequency()
{
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf::callback(hackrf_transfer* transfer)
{
    if(!transfer) return 0;

    uint8_t *ptr = transfer->buffer;
    rx_hackrf *ctx = static_cast<rx_hackrf*>(transfer->rx_ctx);

    ctx->rx_execute(ptr,transfer->valid_length);

    if(!ctx->done)
    {
        return -1;
    }

    return 0;
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_hackrf::rx_execute(void *in_ptr, int nsamples)
{
    int8_t * ptr = (int8_t*)in_ptr;
    float level_detect = std::numeric_limits<float>::max();
    conv.execute(0,nsamples / 2, &ptr[0], &ptr[1], ptr_buffer, level_detect,signal);
    rx_base::rx_execute(nsamples / 2, level_detect);
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf::hw_start()
{
    int err = hackrf_start_rx(_dev, callback, (void*) this);
    len_buffer = 0;
    fprintf(stderr, "hackrf start rx %d\n", err);
    return err;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::hw_stop()
{
    done = false;
    hackrf_set_antenna_enable(_dev, 0);
    hackrf_stop_rx(_dev);
    hackrf_close(_dev);
    _dev = nullptr;
    fprintf(stderr, "hackrf stop rx\n");
}
//-------------------------------------------------------------------------------------------
