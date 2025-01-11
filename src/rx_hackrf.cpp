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

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

//----------------------------------------------------------------------------------------------------------------------------
rx_hackrf::rx_hackrf(QObject *parent) : QObject(parent)
{
    conv.init(2, 1.0f / (1 << 7), 0.03f, 0.01f);
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
int rx_hackrf::init(double _rf_frequency, int _gain_db)
{
    int ret = 0;
    fprintf(stderr,"hackrf init\n");
    rf_frequency = _rf_frequency;
    ch_frequency = _rf_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 78;
        agc = true;
    }
    sample_rate = 10000000.0f; // max for 10bit (10000000.0f for 8bit)
    ret = hackrf_open( &_dev );
    hackrf_set_sample_rate( _dev, sample_rate );
    hackrf_set_freq( _dev, uint64_t(rf_frequency) );
    gain_db = _gain_db;
    uint32_t bw = hackrf_compute_baseband_filter_bw( uint32_t(8000000.0) );
    ret = hackrf_set_baseband_filter_bandwidth( _dev, bw );

    if(ret != 0) return ret;

    max_len_out = len_out_device * max_blocks;
    buffer_a.resize(max_len_out);
    buffer_b.resize(max_len_out);

    demodulator = new dvbt2_demodulator(id_hackrf, sample_rate);
    thread = new QThread;
    thread->setObjectName("demod");
    demodulator->moveToThread(thread);
    connect(this, &rx_hackrf::execute, demodulator, &dvbt2_demodulator::execute);
    connect(this, &rx_hackrf::stop_demodulator, demodulator, &dvbt2_demodulator::stop);
    connect(demodulator, &dvbt2_demodulator::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(demodulator, &dvbt2_demodulator::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    signal.agc = agc;

    reset();

    return ret;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::reset()
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
    ptr_buffer = &buffer_a[0];
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;
    set_rf_frequency();
    set_gain(true);
    conv.reset();

    qDebug() << "rx_hackrf::reset";
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::set_rf_frequency()
{
//    printf("rx_hackrf::set_rf_frequency %f\n", signal.coarse_freq_offset);
    if(!signal.frequency_changed){
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 20) {
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
        int err=hackrf_set_freq( _dev, uint64_t(rf_frequency) );
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
void rx_hackrf::set_gain(bool force)
{
    if(!signal.gain_changed){
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 50) {
            signal.gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if((agc && signal.change_gain) || force) {
        signal.change_gain = false;
        gain_changed = false;
        signal.gain_changed = false;
        gain_db += signal.gain_offset;
        int err=set_gain_internal(gain_db);
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
void rx_hackrf::set_gain_db(int gain)
{
    gain_db = gain;
    int err=set_gain_internal(gain);
    if(err != 0) {
        emit status(err);
    }
    else{
        signal.gain_changed = false;
        start_wait_gain_changed = clock();
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf::set_gain_internal(int gain)
{
    int clip_gain =0;
    if(gain>=40)
    {
        clip_gain = 40;
        gain -=40;
    }else {
        clip_gain = gain;
        gain %= 8;
        clip_gain -= gain;
    }
    int err=hackrf_set_lna_gain( _dev, uint32_t(clip_gain) );
    fprintf(stderr,"LNA=%d\n",clip_gain);
    clip_gain=0;
    if(gain)
    {
        if(gain>=10)
        {
            clip_gain = 10;
            gain -=10;
        }else
            clip_gain = 0;
    }
    err|=hackrf_set_amp_enable( _dev, clip_gain?1:0 );
    fprintf(stderr,"AMP=%d\n",clip_gain);
    clip_gain=0;
    if(gain)
    {
        if(gain>=50)
        {
            clip_gain = 50;
        }else
            clip_gain = gain;
    }
    err|=hackrf_set_vga_gain( _dev, uint32_t(clip_gain) );
    fprintf(stderr,"VGA=%d\n",clip_gain);
    return err;
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
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,nsamples / 2, &ptr[0], &ptr[1], ptr_buffer, level_detect,signal);
    len_buffer += nsamples / 2;
    ptr_buffer += nsamples / 2;

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
void rx_hackrf::start()
{
    reset();
    int err;
    ptr_buffer = &buffer_a[0];
    err = hackrf_start_rx(_dev, callback, (void*) this);
    len_buffer = 0;
    fprintf(stderr, "hackrf start rx %d\n", err);
}
//-------------------------------------------------------------------------------------------
void rx_hackrf::stop()
{
    done = false;
    hackrf_stop_rx(_dev);
    hackrf_close(_dev);
    emit stop_demodulator();
    if(thread->isRunning()) thread->wait(1000);
    emit finished();
}
//-------------------------------------------------------------------------------------------
