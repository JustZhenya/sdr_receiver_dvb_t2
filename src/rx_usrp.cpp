
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

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

//----------------------------------------------------------------------------------------------------------------------------
rx_usrp::rx_usrp(QObject *parent) : QObject(parent)
{
    conv.init(2, 1.0f / (1 << 15), 0.03f, 0.01f);

}
//-------------------------------------------------------------------------------------------
rx_usrp::~rx_usrp()
{
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
int rx_usrp::init(double _rf_frequency, int _gain_db)
{
    int ret = 0;
    rf_frequency = _rf_frequency;
    ch_frequency = _rf_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 20;
        agc = true;
    }
    sample_rate = 9000000.0f;
    uhd::device_addr_t device_addr{"uhd,num_recv_frames=128"};
    _dev = uhd::usrp::multi_usrp::make(device_addr);
    _dev->set_rx_rate(sample_rate, chan);
    _dev->set_rx_freq(uhd::tune_request_t(rf_frequency),chan);

    gain_db = _gain_db;
    _dev->set_rx_bandwidth(8000000, chan);
    _dev->set_rx_agc(false, chan);

    //if(ret != 0) return ret;

    max_len_out = len_out_device * max_blocks;
    buffer_a.resize(max_len_out);
    buffer_b.resize(max_len_out);

    demodulator = new dvbt2_demodulator(id_usrp, sample_rate);
    thread = new QThread;
    thread->setObjectName("demod");
    demodulator->moveToThread(thread);
    connect(this, &rx_usrp::execute, demodulator, &dvbt2_demodulator::execute, Qt::QueuedConnection);
    connect(this, &rx_usrp::stop_demodulator, demodulator, &dvbt2_demodulator::stop);
    connect(demodulator, &dvbt2_demodulator::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(demodulator, &dvbt2_demodulator::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    signal.agc = agc;

    reset();

    return ret;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::reset()
{
    signal.reset = false;
    rf_frequency = ch_frequency;
    signal.coarse_freq_offset = 0.0;
    signal.change_frequency = true;
    signal.correct_resample = 0.0;
    if(agc) {
        gain_db = 20;
    }
    signal.gain_offset = 0;
    signal.change_gain = true;
    ptr_buffer = buffer_a.data();
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;
    set_rf_frequency();
    set_gain(true);
    conv.reset();

    qDebug() << "rx_usrp::reset";
}
//-------------------------------------------------------------------------------------------
void rx_usrp::set_rf_frequency()
{
//    printf("rx_usrp::set_rf_frequency %f\n", signal.coarse_freq_offset);
    if(!signal.frequency_changed){
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 100) {
            signal.frequency_changed = true;
            emit radio_frequency(rf_frequency);
        }
    }
    if(signal.change_frequency) {
        signal.change_frequency = false;
        frequency_changed = false;
        signal.frequency_changed = false;
//        signal.correct_resample = signal.coarse_freq_offset / rf_frequency;
        rf_frequency += signal.coarse_freq_offset;
        const uhd::tune_result_t res = _dev->set_rx_freq(uhd::tune_request_t(rf_frequency), chan);
        //rf_frequency = res.actual_dsp_freq;
        int err=0;
        if(err != 0) {
            emit status(err);
        }
        else{
            signal.frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_usrp::set_gain(bool force)
{
    if(!signal.gain_changed){
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 20) {
            _dev->set_rx_dc_offset(0, chan);
            _dev->set_rx_iq_balance(0, chan);
            signal.gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if((agc && signal.change_gain) || force) {
        signal.change_gain = false;
        gain_changed = false;
        signal.gain_changed = false;
        gain_db += signal.gain_offset;
        _dev->set_rx_dc_offset(1, chan);
        _dev->set_rx_gain(gain_db, chan);
        int err=0;
        if(err != 0) {
            emit status(err);
        }
        else{
            signal.gain_changed = false;
            start_wait_gain_changed = clock();
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_usrp::set_gain_db(int gain)
{
    _dev->set_rx_dc_offset(1, chan);
    _dev->set_rx_gain(gain_db, chan);
    gain_db = gain;
    int err=0;
    if(err != 0) {
        emit status(err);
    }
    else{
        signal.gain_changed = false;
        start_wait_gain_changed = clock();
    }
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_usrp::rx_execute(void *in_ptr, int nsamples)
{
    int16_t * ptr = (int16_t*)in_ptr;
    if(!done)
        return;
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,nsamples, &ptr[0], &ptr[1],ptr_buffer,level_detect,signal);
    len_buffer += nsamples;
    ptr_buffer += nsamples;

    if(demodulator->mutex->try_lock()) {

        if(signal.reset){
            reset();

            demodulator->mutex->unlock();

            return;

        }
        emit buffered(len_buffer/nsamples, max_blocks);
        #if 0
        // coarse frequency setting
        set_rf_frequency();
        // AGC
        set_gain();
        #endif

        if(swap_buffer) {
            emit execute(len_buffer, &buffer_a[0], level_detect, &signal);
            ptr_buffer = buffer_b.data();
        }
        else {
            emit execute(len_buffer, &buffer_b[0], level_detect, &signal);
            ptr_buffer = buffer_a.data();
        }
        swap_buffer = !swap_buffer;
        len_buffer = 0;
        blocks = 1;

        demodulator->mutex->unlock();
    }
    else {
        ++blocks;
        if(blocks > max_blocks){
            fprintf(stderr, "reset buffer blocks: %d\n", blocks);
            blocks = 1;
            len_buffer = 0;
            if(swap_buffer) {
                ptr_buffer = buffer_a.data();
            }
            else {
                ptr_buffer = buffer_b.data();
            }
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_usrp::start()
{
    reset();
    ptr_buffer = buffer_a.data();
    int err = 0;

    uhd::stream_args_t stream_args("sc16","sc16");
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
    len_buffer = 0;
    emit stop_demodulator();
    if(thread->isRunning()) thread->wait(1000);
    emit finished();
}
//-------------------------------------------------------------------------------------------
void rx_usrp::stop()
{
    _rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    done = false;
}
//-------------------------------------------------------------------------------------------
