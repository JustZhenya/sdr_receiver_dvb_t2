#ifndef RX_AIRSPY_H
#define RX_AIRSPY_H

#include <QObject>
#include <QTimer>
#include <string>
#include <vector>

#include "rx_base.h"
#include "libairspy/src/airspy.h"

class rx_airspy : virtual public rx_base<int16_t>
{
public:
    explicit rx_airspy(QObject *parent = nullptr);
    virtual ~rx_airspy();

    std::string error (int err) override;
    int get(std::string &_ser_no, std::string &_hw_ver) override;
    const QString dev_name() override
    {
        return "AirSpy";
    }
    const QString thread_name() override
    {
        return "rx_airspy";
    }

private:
    uint64_t serials[10];
    struct airspy_device* device = nullptr;

    void rx_execute(int16_t *_ptr_rx_buffer, int _len_out_device);
    static int rx_callback(airspy_transfer_t* transfer);
    int hw_init(uint32_t _rf_frequency_hz, int _gain) override;
    int hw_set_frequency() override;
    void on_frequency_changed() override;
    int hw_set_gain() override;
    void on_gain_changed() override;
    //void update_gain_frequency() override;
    void hw_stop() override;
    int hw_start() override;
};

#endif // RX_AIRSPY_H
