#ifndef RX_BASE_H
#define RX_BASE_H

#include "rx_interface.h"

template<typename T> class rx_base : virtual public rx_interface
{
public:
    explicit rx_base(QObject *parent = nullptr): rx_interface(parent)
    {
    }
    virtual ~rx_base()
    {
    }

    int init(uint32_t _rf_frequency_hz, int _gain) override;
    void start() override;
    void stop() override;
    int gain_min() override;
    int gain_max() override;

protected:
    int GAIN_MAX = 0;
    int GAIN_MIN = 0;
    int blocking_start = false;
    int gain = 0;
    uint32_t rf_frequency = 0;
    uint32_t ch_frequency = 0;
    float sample_rate = 0.f;
    bool agc = false;
    signal_estimate signal{};
    float frequency_offset = 0.0f;
    bool change_frequency = false;
    bool frequency_changed = true;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    int gain_offset = 0;
    bool change_gain = false;
    bool gain_changed = true;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;
    convert_iq<T> conv{};
    int len_out_device = 128*1024*4;
    int max_blocks = 256;
    std::vector<complex> buffer_a{};
    std::vector<complex> buffer_b{};
    complex* ptr_buffer = nullptr;
    int  blocks = 1;
    int len_buffer = 0;
    bool swap_buffer = true;

    void reset();
    void set_rf_frequency();
    void set_gain();
    void set_gain_db(int _gain) override;
    virtual void rx_execute(int nsamples, float level_detect);
    virtual int hw_init(uint32_t _rf_frequency_hz, int _gain) = 0;
    virtual int hw_set_frequency() = 0;
    virtual void on_frequency_changed() = 0;
    virtual int hw_set_gain() = 0;
    virtual void on_gain_changed() = 0;
    virtual void update_gain_frequency();
    virtual void hw_stop() = 0;
    virtual int hw_start() = 0;
private:
    QThread* thread;
};

#endif // RX_BASE_H
