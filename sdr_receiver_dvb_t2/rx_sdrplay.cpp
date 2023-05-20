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

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

#include <deque>
#include <algorithm>

static std::deque<short> g_i_queue, g_q_queue;

static int last_gain = 0;
static double last_freq = 0.0;
static int g_rf_changed = 0, g_gr_changed = 0;

static std::mutex g_m;
static std::condition_variable g_cv;

static void stream_cb(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    {
        std::unique_lock lock(g_m);

        g_i_queue.insert(g_i_queue.end(), xi, xi + numSamples);
        g_q_queue.insert(g_q_queue.end(), xq, xq + numSamples);

        HANDLE* handle = (HANDLE*) cbContext;

        sdrplay_api_DeviceParamsT* params;
        sdrplay_api_GetDeviceParams(*handle, &params);

        if(params && params->rxChannelA->tunerParams.rfFreq.rfHz != last_freq)
        {
            g_rf_changed = 1;
            last_freq = params->rxChannelA->tunerParams.rfFreq.rfHz;
        }

        if(params && params->rxChannelA->tunerParams.gain.gRdB != last_gain)
        {
            g_gr_changed = 1;
            last_gain = params->rxChannelA->tunerParams.gain.gRdB;
        }
    }

    g_cv.notify_one();
}

static void event_cb(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    // do nothing
}

sdrplay_api_CallbackFnsT callbacks = {
    .StreamACbFn = stream_cb,
    .StreamBCbFn = stream_cb,
    .EventCbFn = event_cb
};

sdrplay_api_ErrT my_ReadPacket(short* i_ptr, short* q_ptr, int* gr_changed, int* rf_changed, int* fs_changed, int samples_wanted)
{
    std::unique_lock lock(g_m);
    g_cv.wait(lock, [samples_wanted] { return g_i_queue.size() > samples_wanted && g_q_queue.size() > samples_wanted; });

    std::copy(g_i_queue.begin(), g_i_queue.begin() + samples_wanted, i_ptr);
    g_i_queue.erase(g_i_queue.begin(), g_i_queue.begin() + samples_wanted);

    std::copy(g_q_queue.begin(), g_q_queue.begin() + samples_wanted, q_ptr);
    g_q_queue.erase(g_q_queue.begin(), g_q_queue.begin() + samples_wanted);

    *rf_changed = g_rf_changed;
    g_rf_changed = 0;

    *gr_changed = g_gr_changed;
    g_gr_changed = 0;

    return sdrplay_api_Success;
}

rx_sdrplay::rx_sdrplay(QObject *parent) : QObject(parent)
{  

}

rx_sdrplay::~rx_sdrplay()
{
    
}

string rx_sdrplay::error(sdrplay_api_ErrT err)
{
    return (string) sdrplay_api_GetErrorString(err);
}

sdrplay_api_ErrT rx_sdrplay::get(char* &_ser_no, unsigned char &_hw_ver)
{

    sdrplay_api_Open();
//    mir_sdr_DebugEnable(1);

    sdrplay_api_DeviceT devices[4];
    unsigned int numDevs;
    err = sdrplay_api_GetDevices(&devices[0], &numDevs, 4);

    if(err != 0) return err;

    _ser_no = devices[0].SerNo;
    _hw_ver = devices[0].hwVer;
    devices[0].tuner = sdrplay_api_Tuner_A;
    err = sdrplay_api_SelectDevice(&devices[0]);
    selected_device = devices[0];

    sdrplay_api_UnlockDeviceApi();
    sdrplay_api_DebugEnable(selected_device.dev, sdrplay_api_DbgLvl_Message);

    return err;
}

sdrplay_api_ErrT rx_sdrplay::init(double _rf_frequence, int _gain_db)
{
    rf_frequence = _rf_frequence;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 43;
        agc = true;
    }
    sample_rate = 9200000.0f; // max for 10bit (10000000.0f for 8bit)

    sdrplay_api_DeviceParamsT* params;

    err = sdrplay_api_GetDeviceParams(selected_device.dev, &params);
    if(err) return err;

    params->rxChannelA->tunerParams.gain.gRdB = gain_db;
    params->rxChannelA->tunerParams.rfFreq.rfHz = static_cast<double>(rf_frequence);
    params->rxChannelA->tunerParams.bwType = sdrplay_api_BW_8_000;
    params->rxChannelA->tunerParams.ifType = sdrplay_api_IF_Zero;
    params->devParams->fsFreq.fsHz = sample_rate;
    params->rxChannelA->ctrlParams.dcOffset.DCenable = false;
    params->rxChannelA->ctrlParams.dcOffset.IQenable = false;
    params->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;

    err = sdrplay_api_Init(selected_device.dev, &callbacks, &selected_device.dev);
    if(err) return err;

    len_out_device = 2048;

    max_len_out = len_out_device * max_blocks;
    i_buffer_a = new short[max_len_out];
    q_buffer_a = new short[max_len_out];
    i_buffer_b = new short[max_len_out];
    q_buffer_b = new short[max_len_out];

    mutex_out = new QMutex;
    signal_out = new QWaitCondition;

    frame = new dvbt2_frame(signal_out, mutex_out, id_sdrplay, max_len_out, len_out_device, sample_rate);
    thread = new QThread;
    frame->moveToThread(thread);
    connect(this, &rx_sdrplay::execute, frame, &dvbt2_frame::execute);
    connect(this, &rx_sdrplay::stop_demodulator, frame, &dvbt2_frame::stop);
    connect(frame, &dvbt2_frame::finished, frame, &dvbt2_frame::deleteLater);
    connect(frame, &dvbt2_frame::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    return err;
}

void rx_sdrplay::start()
{
    short* ptr_i_bubber = i_buffer_a;
    short* ptr_q_buffer = q_buffer_a;
    int gr_changed = 0;
    int rf_changed = 0;
    int fs_changed = 0;

    float frequency_offset = 0.0f;
    bool change_frequency = false;
    bool frequency_changed = true;

    int gain_offset = 0;
    bool change_gain = false;
    bool gain_changed = true;    

    int blocks = 1;
    const int norm_blocks = 500;
    const int decrease_blocks = 100;
    int len_buffer = 0;
    emit radio_frequency(rf_frequence);
    emit level_gain(gain_db);
    while(done) {
        // get samples
        for(int n = 0; n < blocks; ++n) {
            err = my_ReadPacket(ptr_i_bubber, ptr_q_buffer, &gr_changed, &rf_changed, &fs_changed, len_out_device);
            if(err != 0) emit status(err);
            if(rf_changed) {
                rf_changed = 0;
                frequency_changed = true;
                emit radio_frequency(rf_frequence);
            }
            if(gr_changed) {
                gr_changed = 0;
                gain_changed = true;
                emit level_gain(gain_db);
            }
            len_buffer += len_out_device;
            ptr_i_bubber += len_out_device;
            ptr_q_buffer += len_out_device;
        }

        if(mutex_out->try_lock()) {

            frame->get_signal_estimate(change_frequency, frequency_offset, change_gain, gain_offset);

            // coarse frequency setting
            if(change_frequency) {
                float correct = -frequency_offset / static_cast<float>(rf_frequence);
                frame->correct_resample(correct);
                rf_frequence += static_cast<double>(frequency_offset);

                sdrplay_api_DeviceParamsT* params;
                sdrplay_api_GetDeviceParams(selected_device.dev, &params);
                params->rxChannelA->tunerParams.rfFreq.rfHz = rf_frequence;
                err = sdrplay_api_Update(selected_device.dev, selected_device.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
                if(err) emit status(err);

                frequency_changed = false;
            }
            // AGC
            if(agc && change_gain) {
                gain_db -= gain_offset;

                sdrplay_api_DeviceParamsT* params;
                sdrplay_api_GetDeviceParams(selected_device.dev, &params);
                params->rxChannelA->tunerParams.gain.gRdB = gain_db;
                err = sdrplay_api_Update(selected_device.dev, selected_device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                if(err) emit status(err);

                gain_changed = false;
            }
            if(swap_buffer) {
                swap_buffer = false;
                emit execute(len_buffer, i_buffer_a, q_buffer_a, frequency_changed, gain_changed);
                mutex_out->unlock();
                ptr_i_bubber = i_buffer_b;
                ptr_q_buffer = q_buffer_b;
            }
            else {
                swap_buffer = true;
                emit execute(len_buffer, i_buffer_b, q_buffer_b, frequency_changed, gain_changed);
                mutex_out->unlock();
                ptr_i_bubber = i_buffer_a;
                ptr_q_buffer = q_buffer_a;
            }
            len_buffer = 0;
            if(blocks > norm_blocks) blocks -= decrease_blocks;
        } else {
            blocks += 1;
            int remain = max_len_out - len_buffer;
            int need = len_out_device * blocks;
            if(need > remain){
                len_buffer = 0;
                fprintf(stderr, "reset buffer blocks: %d\n", blocks);
                if(swap_buffer) {
                    ptr_i_bubber = i_buffer_a;
                    ptr_q_buffer = q_buffer_a;
                } else {
                    ptr_i_bubber = i_buffer_b;
                    ptr_q_buffer = q_buffer_b;
                }
            }
        }
    }

    err = sdrplay_api_Uninit(selected_device.dev);
    if(err) emit status(err);

    err = sdrplay_api_ReleaseDevice(&selected_device);
    if(err) emit status(err);

    emit stop_demodulator();
    if(thread->isRunning()) thread->wait();
    delete [] i_buffer_a;
    delete [] q_buffer_a;
    delete [] i_buffer_b;
    delete [] q_buffer_b;
    emit finished();
}

void rx_sdrplay::stop()
{
    done = false;
}
