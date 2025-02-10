#ifndef RX_PLUTOSDR_H
#define RX_PLUTOSDR_H

#include <QObject>
#include <vector>
#include "libplutosdr/plutosdr_hi_speed_rx.h"
#include "rx_base.h"

class rx_plutosdr : virtual public rx_base<int16_t>
{
    Q_OBJECT
public:
    explicit rx_plutosdr(QObject* parent = nullptr);
    virtual ~rx_plutosdr();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    const QString dev_name() override
    {
        return "PlutoSDR";
    }
    const QString thread_name() override
    {
        return "rx_plutosdr";
    }
    void reboot();


private:
    const char* port_name;
    void pluto_kernel_patch();
    bool done = true;

    plutosdr_device_t* device;

    static int plutosdr_callback(plutosdr_transfer* _transfer);
    void rx_execute(int16_t *_rx_i, int16_t *_rx_q);

    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // RX_PLUTOSDR_H
