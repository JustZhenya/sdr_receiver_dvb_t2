
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
#include "rx_usrp.h"

#include "rx_base.cpp"

//----------------------------------------------------------------------------------------------------------------------------
rx_usrp::rx_usrp(QObject *parent) : rx_base(parent)
{
    len_out_device = 128 * 1024 * 4;
    max_blocks = 256;
    GAIN_MAX = 100;
    GAIN_MIN = 0;
    blocking_start = true;
    conv.init(2, 1.0f / (1 << 15), 0.04f, 0.015f);

}
//-------------------------------------------------------------------------------------------
rx_usrp::~rx_usrp()
{
    fprintf(stderr,"rx_usrp::~rx_usrp\n");
}
//-------------------------------------------------------------------------------------------
std::string rx_usrp::error (int err)
{
    switch (err) {
       case 0:
          return "Success";
       case -1:
          return "error";
       default:
          return "Unknown error "+std::to_string(err);
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_usrp::get(std::string &_ser_no, std::string &_hw_ver)
{

  static std::vector<std::string> devices;
  static std::vector<unsigned char> hw_ver;
  std::string label;

  devices.resize(0);
  hw_ver.resize(0);
  
  uhd::device_addr_t hint;
  for (const uhd::device_addr_t &dev : uhd::device::find(hint))
  {
    std::string args = "uhd," + dev.to_string();

    std::string type = dev.cast< std::string >("type", "usrp");
    std::string name = dev.cast< std::string >("name", "");
    std::string serial = dev.cast< std::string >("serial", "");
    devices.push_back( name + " " +serial);
    hw_ver.push_back(0);
  }

  if(devices.size() == 0)
    return -1;
  _ser_no = devices[0];
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
int rx_usrp::hw_init(uint32_t _rf_frequency, int _gain_db)
{
    int ret = 0;
    sample_rate = 9000000.0f;
    uhd::device_addr_t device_addr{"uhd,num_recv_frames=128"};
    _dev = uhd::usrp::multi_usrp::make(device_addr);
    _dev->set_rx_rate(sample_rate, chan);
    _dev->set_rx_freq(uhd::tune_request_t(rf_frequency),chan);

    _dev->set_rx_bandwidth(8000000, chan);
    _dev->set_rx_agc(false, chan);

    return ret;
}
//-------------------------------------------------------------------------------------------
int rx_usrp::hw_set_frequency()
{
    _dev->set_rx_freq(uhd::tune_request_t(rf_frequency), chan);
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_usrp::hw_set_gain()
{
    _dev->set_rx_dc_offset(1, chan);
    _dev->set_rx_gain(gain, chan);
    fprintf(stderr,"rx_usrp::hw_set_gain\n");
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::on_gain_changed()
{
    fprintf(stderr,"rx_usrp::on_gain_changed\n");
    _dev->set_rx_dc_offset(0, chan);
    _dev->set_rx_iq_balance(0, chan);
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_usrp::update_gain_frequency_direct()
{
    if(!this)
        return;
    if(!_dev)
        return;
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_usrp::update_gain_frequency()
{
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_usrp::rx_execute(void *in_ptr, int nsamples)
{
    int16_t * ptr = (int16_t*)in_ptr;
    if(!done)
        return;
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,nsamples, &ptr[0], &ptr[1],ptr_buffer,level_detect,signal);
    rx_base::rx_execute(nsamples, level_detect);
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_usrp::hw_start()
{
    fprintf(stderr,"rx_usrp::hw_start\n");
    uhd::stream_args_t stream_args("sc16","sc16");
    in_buf.resize(len_out_device);
    if (! _rx_stream)
    {
        _rx_stream = _dev->get_rx_stream(stream_args);
        _samps_per_packet = _rx_stream->get_max_num_samps();
    }
    const size_t bpi = uhd::convert::get_bytes_per_item(stream_args.cpu_format);
    // setup a stream command that starts streaming slightly in the future
    static const double reasonable_delay = 0.1; // order of magnitude over RTT
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    stream_cmd.time_spec = _dev->get_time_now() + uhd::time_spec_t(reasonable_delay);
    _rx_stream->issue_stream_cmd(stream_cmd);
    const uhd::tune_result_t res = _dev->set_rx_freq(uhd::tune_request_t(rf_frequency), chan);
    while(done)
    {
        size_t num_samps = _rx_stream->recv(&in_buf[0], len_out_device / 2, _metadata, _recv_timeout, _recv_one_packet);
        rx_execute(&in_buf[0], num_samps);
    }
    const size_t nbytes = 4096;
    while (true)
    {
        _rx_stream->recv(&in_buf[0], nbytes / bpi, _metadata, 0.0);

        if (_metadata.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            break;
    }
    fprintf(stderr,"rx_usrp::hw_start exit\n");
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::hw_stop()
{
    _rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    done = false;
    fprintf(stderr,"rx_usrp::hw_stop\n");
}
//-------------------------------------------------------------------------------------------
