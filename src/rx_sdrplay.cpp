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
#include <algorithm>
#include <sdrplay_api.h>

static constexpr size_t samples_wanted = 128 * 1024 * 4;
static std::deque<short> g_i_queue, g_q_queue;
static std::mutex g_m;
static std::condition_variable g_cv;

void rx_sdrplay::stream_cb(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if(reset)
        fprintf(stderr, "rx_sdrplay::stream_cb reset\n");

    //fprintf(stderr, "rx_sdrplay::stream_cb %u\n", numSamples);

    {
        std::unique_lock lock(g_m);
        g_i_queue.insert(g_i_queue.end(), xi, xi + numSamples);
        g_q_queue.insert(g_q_queue.end(), xq, xq + numSamples);
    }

    g_cv.notify_one();
}

void rx_sdrplay::event_cb(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    rx_sdrplay* rx_sdrplay_ptr = (rx_sdrplay*) cbContext;

    switch(eventId)
    {
        case sdrplay_api_GainChange:
            printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n",
                "sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A":
                "sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB,
                params->gainParams.currGain);
            break;
        case sdrplay_api_PowerOverloadChange:
            printf("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
                (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
                (params->powerOverloadParams.powerOverloadChangeType ==
                sdrplay_api_Overload_Detected)? "sdrplay_api_Overload_Detected":
                "sdrplay_api_Overload_Corrected");
            // Send update message to acknowledge power overload message received
            sdrplay_api_Update(rx_sdrplay_ptr->selected_device.dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
            break;
        case sdrplay_api_RspDuoModeChange:
            printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
                "sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)?
                "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)?
                "sdrplay_api_MasterInitialised":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)?
                "sdrplay_api_SlaveAttached":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)?
                "sdrplay_api_SlaveDetached":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)?
                "sdrplay_api_SlaveInitialised":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)?
                "sdrplay_api_SlaveUninitialised":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)?
                "sdrplay_api_MasterDllDisappeared":
                (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDllDisappeared)?
                "sdrplay_api_SlaveDllDisappeared": "unknown type");
            //if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
            //    masterInitialised = 1;
            //if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
            //    slaveUninitialised = 1;
            break;
        case sdrplay_api_DeviceRemoved:
            printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
            break;
        default:
            printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
            break;
    }
}

//-------------------------------------------------------------------------------------------
rx_sdrplay::rx_sdrplay(QObject *parent) : rx_base(parent)
{
    fprintf(stderr, "rx_sdrplay::rx_sdrplay\n");

    len_out_device = samples_wanted;
    max_blocks = 256;

    size_t max_len_out = len_out_device * max_blocks;
    buffer_a.resize(max_len_out);
    buffer_b.resize(max_len_out);

    GAIN_MIN = 0;
    GAIN_MAX = 42;
    blocking_start = false;
    conv.init(1, 1.0f / (1 << 14), 0.04f, 0.02f);

    sdrplay_api_Open();
}
//-------------------------------------------------------------------------------------------
rx_sdrplay::~rx_sdrplay()
{
    fprintf(stderr, "rx_sdrplay::~rx_sdrplay\n");
    sdrplay_api_Close();
}
//-------------------------------------------------------------------------------------------
std::string rx_sdrplay::error (int err)
{
    return (std::string) sdrplay_api_GetErrorString((sdrplay_api_ErrT) err);
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::get(std::string& _ser_no, std::string& _hw_ver)
{
    fprintf(stderr, "rx_sdrplay::get\n");

    sdrplay_api_ErrT err;

    sdrplay_api_DeviceT devices[4];
    unsigned int numDevs;
    err = sdrplay_api_GetDevices(&devices[0], &numDevs, 4);
    if(err != sdrplay_api_Success) return err;

    _ser_no = devices[0].SerNo;
    _hw_ver = devices[0].hwVer;
    devices[0].tuner = sdrplay_api_Tuner_A;
    err = sdrplay_api_SelectDevice(&devices[0]);
    if(err != sdrplay_api_Success) return err;

    selected_device = devices[0];

    //sdrplay_api_UnlockDeviceApi();
    //sdrplay_api_DebugEnable(selected_device.dev, sdrplay_api_DbgLvl_Message);

    return err;
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_init(uint32_t rf_frequence, int gain_db)
{
    fprintf(stderr, "rx_sdrplay::hw_init\n");

    sample_rate = 10000000.0f; // max for 10bit (10000000.0f for 8bit)

    sdrplay_api_DeviceParamsT* params;
    sdrplay_api_ErrT err = sdrplay_api_GetDeviceParams(selected_device.dev, &params);
    if(err != sdrplay_api_Success) return err;

    params->rxChannelA->tunerParams.gain.LNAstate = 2;
    params->rxChannelA->tunerParams.gain.gRdB = std::clamp(GAIN_MAX - gain, GAIN_MIN, GAIN_MAX);
    params->rxChannelA->tunerParams.rfFreq.rfHz = static_cast<double>(rf_frequency);
    params->rxChannelA->tunerParams.bwType = sdrplay_api_BW_8_000;
    params->rxChannelA->tunerParams.ifType = sdrplay_api_IF_Zero;
    params->devParams->fsFreq.fsHz = sample_rate;
    params->rxChannelA->ctrlParams.dcOffset.DCenable = false;
    params->rxChannelA->ctrlParams.dcOffset.IQenable = false;
    params->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;

    return 0;
}

//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_set_frequency()
{
    fprintf(stderr, "rx_sdrplay::hw_set_frequency %u\n", rf_frequency);

    sdrplay_api_ErrT err;
    sdrplay_api_DeviceParamsT* params;
    err = sdrplay_api_GetDeviceParams(selected_device.dev, &params);
    if(err != sdrplay_api_Success) return err;

    params->rxChannelA->tunerParams.rfFreq.rfHz = static_cast<double>(rf_frequency);
    if(is_sdrplay_initialized)
    {
        err = sdrplay_api_Update(selected_device.dev, selected_device.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
        return err;
    }
    
    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_set_gain()
{
    fprintf(stderr, "rx_sdrplay::hw_set_gain %d\n", gain);

    sdrplay_api_ErrT err;
    sdrplay_api_DeviceParamsT* params;
    err = sdrplay_api_GetDeviceParams(selected_device.dev, &params);
    if(err != sdrplay_api_Success) return err;

    params->rxChannelA->tunerParams.gain.gRdB = std::clamp(GAIN_MAX-gain, GAIN_MIN, GAIN_MAX);
    if(is_sdrplay_initialized)
    {
        err = sdrplay_api_Update(selected_device.dev, selected_device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
        return err;
    }

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::on_gain_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_sdrplay::hw_start()
{
    fprintf(stderr, "rx_sdrplay::hw_start\n");

    sdrplay_api_CallbackFnsT callbacks = {
        .StreamACbFn = stream_cb,
        .StreamBCbFn = stream_cb,
        .EventCbFn = event_cb
    };

    sdrplay_api_ErrT err = sdrplay_api_Init(selected_device.dev, &callbacks, this);
    is_sdrplay_initialized = true;

    std::vector<short> tmp_i(samples_wanted), tmp_q(samples_wanted);

    while(done)
    {
        {
            std::unique_lock lock(g_m);
            g_cv.wait(lock, [] { return g_i_queue.size() > samples_wanted && g_q_queue.size() > samples_wanted; });

            std::copy(g_i_queue.begin(), g_i_queue.begin() + samples_wanted, tmp_i.data());
            g_i_queue.erase(g_i_queue.begin(), g_i_queue.begin() + samples_wanted);

            std::copy(g_q_queue.begin(), g_q_queue.begin() + samples_wanted, tmp_q.data());
            g_q_queue.erase(g_q_queue.begin(), g_q_queue.begin() + samples_wanted);
        }

        float level_detect = std::numeric_limits<float>::max();
        conv.execute(0, samples_wanted, tmp_i.data(), tmp_q.data(), ptr_buffer, level_detect, signal);
        rx_base::rx_execute(samples_wanted, level_detect);
    }

    return err;
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::hw_stop()
{
    fprintf(stderr, "rx_sdrplay::hw_stop\n");

    sdrplay_api_Uninit(selected_device.dev);
    sdrplay_api_ReleaseDevice(&selected_device);

    is_sdrplay_initialized = false;
    done = false;
}
//-------------------------------------------------------------------------------------------
