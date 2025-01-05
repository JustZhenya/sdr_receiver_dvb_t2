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

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

//----------------------------------------------------------------------------------------------------------------------------
rx_miri::rx_miri(QObject *parent) : QObject(parent)
{
    fprintf(stderr,"rx_miri::rx_miri\n");

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
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------------
int rx_miri::init(double _rf_frequency, int _gain_db)
{
    int ret = 0;
    static char fmt[] = "336_S16\x00";
    //static char fmt[] = "504_S16\x00";
    fprintf(stderr,"miri init\n");
    rf_frequency = _rf_frequency;
    ch_frequency = _rf_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 20;
        agc = true;
    }
    sample_rate = 9000000.0f;
    ret = mirisdr_open( &_dev, 0 );
    mirisdr_set_sample_format( _dev, fmt);
    mirisdr_set_sample_rate( _dev, sample_rate );
    mirisdr_set_center_freq( _dev, uint32_t(rf_frequency) );
    gain_db = _gain_db;
    ret = mirisdr_set_bandwidth( _dev, 12000000 );

    if(ret != 0) return ret;

    max_len_out = len_out_device * max_blocks;
    buffer_a.resize(max_len_out);
    buffer_b.resize(max_len_out);

    demodulator = new dvbt2_demodulator(id_miri, sample_rate);
    thread = new QThread;
    thread->setObjectName("demod");
    demodulator->moveToThread(thread);
    connect(this, &rx_miri::execute, demodulator, &dvbt2_demodulator::execute, Qt::QueuedConnection);
    connect(this, &rx_miri::stop_demodulator, demodulator, &dvbt2_demodulator::stop);
    connect(demodulator, &dvbt2_demodulator::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(demodulator, &dvbt2_demodulator::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    signal = new signal_estimate;

    reset();

    return ret;
}
//-------------------------------------------------------------------------------------------
void rx_miri::reset()
{
    signal->reset = false;
    rf_frequency = ch_frequency;
    signal->coarse_freq_offset = 0.0;
    signal->change_frequency = true;
    signal->correct_resample = 0.0;
    if(agc) {
        gain_db = 20;
    }
    signal->gain_offset = 0;
    signal->change_gain = true;
    ptr_buffer = buffer_a.data();
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;
    set_rf_frequency();
    set_gain(true);

    qDebug() << "rx_miri::reset";
}
//-------------------------------------------------------------------------------------------
void rx_miri::set_rf_frequency()
{
//    printf("rx_miri::set_rf_frequency %f\n", signal->coarse_freq_offset);
    if(!signal->frequency_changed){
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 100) {
            signal->frequency_changed = true;
            emit radio_frequency(rf_frequency);
        }
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        frequency_changed = false;
        signal->frequency_changed = false;
        signal->correct_resample = signal->coarse_freq_offset / rf_frequency;
        rf_frequency += signal->coarse_freq_offset;
        int err=mirisdr_set_center_freq( _dev, uint32_t(rf_frequency) );
        if(err != 0) {
            emit status(err);
        }
        else{
            signal->frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_miri::set_gain(bool force)
{
    if(!signal->gain_changed){
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 20) {
            signal->gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if((agc && signal->change_gain) || force) {
        signal->change_gain = false;
        gain_changed = false;
        signal->gain_changed = false;
        gain_db += signal->gain_offset;
        int err=mirisdr_set_tuner_gain( _dev, gain_db );
        if(err != 0) {
            emit status(err);
        }
        else{
            signal->gain_changed = false;
            start_wait_gain_changed = clock();
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_miri::callback(unsigned char *buf, uint32_t len, void *context)
{
    if(!buf) return;

    rx_miri *ctx = static_cast<rx_miri*>(context);

    ctx->rx_execute(buf,len/2);
}
//----------------------------------------------------------------------------------------------------------------------------
void rx_miri::rx_execute(void *in_ptr, int nsamples)
{
    int16_t * ptr = (int16_t*)in_ptr;
    if(!done)
        return;
    for(int i = 0; i < nsamples; ++i)
        ptr_buffer[i] = ptr[i];
    len_buffer += nsamples / 2;
    ptr_buffer += nsamples;

    if(demodulator->mutex->try_lock()) {

        if(signal->reset){
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
            emit execute(len_buffer, &buffer_a[0], &buffer_a[1], signal);
            ptr_buffer = buffer_b.data();
        }
        else {
            emit execute(len_buffer, &buffer_b[0], &buffer_b[1], signal);
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
void rx_miri::start()
{
    reset();
    int err;
    ptr_buffer = buffer_a.data();
    err = mirisdr_reset_buffer(_dev);
    err = mirisdr_read_async( _dev, callback, (void *)this, 64, len_out_device );
    mirisdr_set_bias( _dev, 0 );
    mirisdr_close(_dev);
    len_buffer = 0;
    emit stop_demodulator();
    if(thread->isRunning()) thread->wait(1000);
    emit finished();
    fprintf(stderr, "mirisdr start rx %d\n", err);
}
//-------------------------------------------------------------------------------------------
void rx_miri::stop()
{
    done = false;
    mirisdr_cancel_async( _dev );
    fprintf(stderr, "mirisdr stop\n");
}
//-------------------------------------------------------------------------------------------
