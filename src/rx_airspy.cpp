#include "rx_airspy.h"
#include "rx_base.cpp"

//-------------------------------------------------------------------------------------------
rx_airspy::rx_airspy(QObject *parent) : rx_base(parent)
{
    len_out_device = 65536 * 2;
    max_blocks = 256 * 4;
    GAIN_MAX = 21;
    GAIN_MIN = 0;
    blocking_start = false;
    conv.init(2, 1.0f / (1 << 12), 0.04f, 0.02f);
}
//-------------------------------------------------------------------------------------------
rx_airspy::~rx_airspy()
{
}
//-------------------------------------------------------------------------------------------
std::string rx_airspy::error (int err)
{
    switch (err) {
    case AIRSPY_SUCCESS:
        return "Success";
    case AIRSPY_TRUE:
        return "True";
    case AIRSPY_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case AIRSPY_ERROR_NOT_FOUND:
        return "Not found";
    case AIRSPY_ERROR_BUSY:
        return "Busy";
    case AIRSPY_ERROR_NO_MEM:
        return "No memory";
    case AIRSPY_ERROR_LIBUSB:
        return "error libusb";
    case AIRSPY_ERROR_THREAD:
        return "error thread";
    case AIRSPY_ERROR_STREAMING_THREAD_ERR:
        return "Streaming thread error";
    case AIRSPY_ERROR_STREAMING_STOPPED:
        return "Streaming stopped";
    case AIRSPY_ERROR_OTHER:
        return "Unknown error";
    default:
        return std::to_string(err);
    }
}
//-------------------------------------------------------------------------------------------
int rx_airspy::get(std::string &_ser_no, std::string &_hw_ver)
{
    int err;

    int count = 1;
    err = airspy_list_devices(serials, count);

    if( err < 0 ) return err;

    _ser_no = std::to_string(serials[0]);

    err = airspy_open_sn(&device, serials[0]);

    if( err < 0 ) return err;

    const uint8_t len = 128;
    char version[len];
    err = airspy_version_string_read(device, version, len);

    if( err < 0 ) return err;

    for(int i = 6; i < len; ++i){
        if(version[i] == '\u0000') break;
        _hw_ver += version[i];
    }

    return err;
}
//-------------------------------------------------------------------------------------------
int rx_airspy::hw_init(uint32_t _rf_frequency_hz, int _gain)
{
    int err;

    err = airspy_set_sample_type(device, AIRSPY_SAMPLE_INT16_IQ);

    if( err < 0 ) return err;

    sample_rate = 1.0e+7f;

    err = airspy_set_samplerate(device, static_cast<uint32_t>(sample_rate));

    if( err < 0 ) return err;

    uint8_t biast_val = 0;
    err = airspy_set_rf_bias(device, biast_val);

    if( err < 0 ) return err;

    if(!agc)
    {
        err =  airspy_set_sensitivity_gain(device, static_cast<uint8_t>(gain));

        if( err < 0 ) return err;
    }

    return err;
}
//-------------------------------------------------------------------------------------------
void rx_airspy::set_biastee(const bool state)
{
    airspy_set_rf_bias(device, state);
}
//-------------------------------------------------------------------------------------------
int rx_airspy::hw_start()
{
    return airspy_start_rx(device, rx_callback, this);
}
//-------------------------------------------------------------------------------------------
int rx_airspy::hw_set_frequency()
{
    return airspy_set_freq(device, rf_frequency);
}
//-------------------------------------------------------------------------------------------
void rx_airspy::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_airspy::hw_set_gain()
{
    return airspy_set_sensitivity_gain(device, static_cast<uint8_t>(gain));
}
//-------------------------------------------------------------------------------------------
void rx_airspy::on_gain_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_airspy::rx_callback(airspy_transfer_t* transfer)
{
    if(!transfer) return 0;

    int len_out_device;
    int16_t* ptr_rx_buffer;
    len_out_device = transfer->sample_count  * 2;
    ptr_rx_buffer = static_cast<int16_t*>(transfer->samples);
    rx_airspy* ctx;
    ctx = static_cast<rx_airspy*>(transfer->ctx);
    ctx->rx_execute(ptr_rx_buffer, len_out_device);
    if(transfer->dropped_samples > 0) {
        fprintf(stderr, "dropped_samples: %ld\n", transfer->dropped_samples);
    }

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_airspy::rx_execute(int16_t* _ptr_rx_buffer, int _len_out_device)
{
    int len_out_device = _len_out_device;
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,len_out_device / 2, &_ptr_rx_buffer[0], &_ptr_rx_buffer[1], ptr_buffer, level_detect,signal);
    rx_base::rx_execute(len_out_device / 2, level_detect);
}
//-------------------------------------------------------------------------------------------
void rx_airspy::hw_stop()
{
    airspy_set_rf_bias(device, 0);
    airspy_close(device);
}
//-------------------------------------------------------------------------------------------

