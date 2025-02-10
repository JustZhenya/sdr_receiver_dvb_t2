#include "rx_plutosdr.h"
#include "rx_base.cpp"

#include <QFile>
#include <errno.h>
#include <chrono>
#include <sys/stat.h>

#include "libssh/libssh.h"

#define DEFAULT_SAMPLE_RATE 9200000
//enum plutosdr_error
//{
//    PLUTOSDR_SUCCESS                = 0,
//    PLUTOSDR_TRUE                   = 1,
//    PLUTOSDR_ERROR_INVALID_PARAM    = -2,
//    PLUTOSDR_ERROR_ACCESS           = -3,
//    PLUTOSDR_ERROR_NO_DEVICE        = -4,
//    PLUTOSDR_ERROR_NOT_FOUND        = -5,
//    PLUTOSDR_ERROR_BUSY             = -6,
//    PLUTOSDR_ERROR_NO_MEM           = -11,
//    PLUTOSDR_ERROR_LIBUSB           = -99,
//    PLUTOSDR_ERROR_THREAD           = -1001,
//    PLUTOSDR_ERROR_STREAMING_THREAD_ERR = -1002,
//    PLUTOSDR_ERROR_STREAMING_STOPPED    = -1003,
//    PLUTOSDR_ERROR_OTHER            = -99,
//};
//#define RETING ;
#if defined(_WIN32)
#define S_IRWXU 0x777
#endif

//-------------------------------------------------------------------------------------------
rx_plutosdr::rx_plutosdr(QObject *parent) : rx_base(parent)
{
    len_out_device = 128 * 1024 * 4;
    max_blocks = 32 * 8;
    GAIN_MAX = 71;
    GAIN_MIN = 0;
    blocking_start = false;
    conv.init(1, 1.0f / (1 << 11), 0.04f, 0.02f);
}
//-------------------------------------------------------------------------------------------
rx_plutosdr::~rx_plutosdr()
{
}
//-------------------------------------------------------------------------------------------
std::string rx_plutosdr::error (int _err)
{
    switch (_err) {
    case PLUTOSDR_SUCCESS:
        return "Success";
    case PLUTOSDR_TRUE:
        return "True";
    case PLUTOSDR_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case PLUTOSDR_ERROR_ACCESS:
        return "Error access";
    case PLUTOSDR_ERROR_NOT_FOUND:
        return "Not found";
    case PLUTOSDR_ERROR_BUSY:
        return "Busy";
    case PLUTOSDR_ERROR_NO_MEM:
        return "No memory";
    case PLUTOSDR_ERROR_LIBUSB:
        return "Error libusb";
    case PLUTOSDR_ERROR_THREAD:
        return "Error thread";
    case PLUTOSDR_ERROR_STREAMING_THREAD_ERR:
        return "Streaming thread error";
    case PLUTOSDR_ERROR_STREAMING_STOPPED:
        return "Streaming stopped";
    case PLUTOSDR_ERROR_OTHER:
        return "Unknown error";
    default:
        return std::to_string(_err);
    }
}
//-------------------------------------------------------------------------------------------
int rx_plutosdr::get(std::string &_ser_no, std::string &_hw_ver)
{
    pluto_kernel_patch();
    int err = 0;
    plutosdr_info_t info;
    info.samples_type = IQ;
    err = plutosdr_open(&device, 0, &info);
    if(err == PLUTOSDR_SUCCESS){
        len_out_device = info.len_out;
        for(int i = 0; i < info.serial_number_len; ++i){
            _ser_no += reinterpret_cast<char&>(info.serial_number[i]);
        }
    }

    QThread::sleep(2);

    return err;
}
//-------------------------------------------------------------------------------------------
int rx_plutosdr::hw_init(uint32_t _rf_frequence_hz, int _gain)
{
    int err;
    sample_rate = DEFAULT_SAMPLE_RATE;
    // set this first!
    err = plutosdr_set_rfbw(device, 8000000);
    if(err < 0) return err;
    err = plutosdr_set_sample_rate(device, sample_rate);
    if(err < 0) return err;
    err = plutosdr_set_rxlo(device, rf_frequency);
    if(err < 0) return err;
    err = plutosdr_set_gainctl_manual(device);
    if(err < 0) return err;
    err = plutosdr_set_gain_mdb(device, gain * 1000);
    if(err < 0) return err;
    //
    err = plutosdr_buffer_channel_enable(device, 0, 1);
    if(err < 0) return err;
    err = plutosdr_buffer_channel_enable(device, 1, 1);
    if(err < 0) return err;
    err = plutosdr_bufstream_enable(device, 1);
    if(err < 0) return err;

    return PLUTOSDR_SUCCESS;
}
//-------------------------------------------------------------------------------------------
int rx_plutosdr::hw_start()
{
    int err = plutosdr_start_rx(device, plutosdr_callback, this);
    fprintf(stderr, "plutosdr start rx %d\n", err);
    return err;
}

//-------------------------------------------------------------------------------------------
int rx_plutosdr::hw_set_frequency()
{
    return plutosdr_set_rxlo(device, rf_frequency);
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::on_frequency_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_plutosdr::hw_set_gain()
{
    return plutosdr_set_gain_mdb(device, gain * 1000);
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::on_gain_changed()
{
}
//-------------------------------------------------------------------------------------------
int rx_plutosdr::plutosdr_callback(plutosdr_transfer* _transfer)
{
    if(!_transfer) return 0;

    int16_t *ptr_rx_i, *ptr_rx_q;
    ptr_rx_i = _transfer->i_samples;
    ptr_rx_q = _transfer->q_samples;
    rx_plutosdr *ctx;
    ctx = static_cast<rx_plutosdr*>(_transfer->ctx);

    ctx->rx_execute(ptr_rx_i, ptr_rx_q);

    if(!ctx->done) return -1;

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::rx_execute(int16_t* _rx_i, int16_t* _rx_q)
{
    float level_detect=std::numeric_limits<float>::max();
    conv.execute(0,len_out_device, _rx_i, _rx_q,ptr_buffer,level_detect,signal);
    rx_base::rx_execute(len_out_device, level_detect);
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::hw_stop()
{
    done = false;
    reboot();
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::reboot()
{
    plutosdr_reboot(device);
    plutosdr_close(device);
    QThread::sleep(2);
    fprintf(stderr, "plutosdr close \n");
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr::pluto_kernel_patch()
{
    ssh_session session = ssh_new();
    if (session == nullptr) {
        fprintf(stderr, "Unable to create SSH session \n");
        exit(-1);
    }
    ssh_options_set (session, SSH_OPTIONS_HOST, "root@192.168.3.1" );
    int verbosity = SSH_LOG_PROTOCOL;
    ssh_options_set (session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    int port = 22;
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    int rc;
    rc = ssh_connect(session);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error connecting to localhost: %s\n", ssh_get_error(session));
        exit(-1);
    }
    rc = ssh_userauth_password(session, NULL, "analog");
    if (rc != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Error authenticating with password: %s\n", ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    ssh_scp scp;
    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, "../");
    if (scp == NULL) {
        fprintf(stderr, "Error allocating scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    rc = ssh_scp_init(scp);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error initializing scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }

    rc = ssh_scp_push_directory(scp, "plutousbgadget", S_IRWXU);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't create remote directory: %s\n", ssh_get_error(session));
        exit(-1);
    }
    char* buffer;
    int size_file;
    QFile file_script(":/runme.sh");
    if(file_script.isOpen()) file_script.close();
    if (!file_script.open(QIODevice::ReadOnly)) exit(-1);
    size_file = file_script.size();
    rc = ssh_scp_push_file(scp, "runme.sh", size_file, S_IRWXU);
    if (rc != SSH_OK) {
        file_script.close();
        fprintf(stderr, "Can't open remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    buffer = (char*)malloc(size_file);
    file_script.read(buffer, size_file);
    file_script.close();
    rc = ssh_scp_write(scp, buffer, size_file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    QFile file_blob(":/plutousbgadget.ko");
    if(file_blob.isOpen()) file_blob.close();
    if (!file_blob.open(QIODevice::ReadOnly)) {
        qDebug() << "!file_blob.open(QIODevice::ReadOnly)";
        exit(-1);
    }
    size_file = file_blob.size();
    rc = ssh_scp_push_file(scp, "plutousbgadget.ko", size_file, S_IRWXU);
    if (rc != SSH_OK) {
        file_blob.close();
        fprintf(stderr, "Can't open remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    buffer = (char*)malloc(size_file);
    file_blob.read(buffer, size_file);
    file_blob.close();
    rc = ssh_scp_write(scp, buffer, size_file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    ssh_channel channel;

/*
    channel = ssh_channel_new(session);
    if (channel == NULL) exit(-1);
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK)
    {
        ssh_channel_free(channel);
        exit(-1);
    }
    rc = ssh_channel_request_exec(channel,
                                  "echo 1 > /sys/bus/iio/devices/iio:device4/scan_elements/in_voltage0_en");
    if (rc != SSH_OK)
    {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        exit(-1);
    }
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    channel = ssh_channel_new(session);
    if (channel == NULL) exit(-1);
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK)
    {
        ssh_channel_free(channel);
        exit(-1);
    }
    rc = ssh_channel_request_exec(channel,
                                  "echo 1 > /sys/bus/iio/devices/iio:device4/scan_elements/in_voltage1_en");
    if (rc != SSH_OK)
    {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        exit(-1);
    }
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
*/

    channel = ssh_channel_new(session);
    if (channel == NULL) exit(-1);
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        exit(-1);
    }
    rc = ssh_channel_request_exec(channel, "/plutousbgadget/runme.sh");
    if (rc != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        exit(-1);
    }
    QThread::sleep(3);
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);

    fprintf(stderr, "PlutoSDR kernel patch: Ok \n");
}
//-------------------------------------------------------------------------------------------
