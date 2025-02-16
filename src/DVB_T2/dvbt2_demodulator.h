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
#ifndef DVBT2_DEMODULATOR_H
#define DVBT2_DEMODULATOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QMetaType>
#include <vector>

#include "DSP/interpolator_farrow.hh"
#include "DSP/filter_decimator.h"
#include "DSP/fast_fourier_transform.h"
#include "DSP/loop_filters.hh"
#include "dvbt2_definition.h"
#include "p1_symbol.h"
#include "pilot_generator.h"
#include "address_freq_deinterleaver.h"
#include "p2_symbol.h"
#include "data_symbol.h"
#include "fc_symbol.h"
#include "time_deinterleaver.h"

enum id_device_t{
    id_sdrplay = 0,
    id_airspy,
    id_plutosdr,
    id_hackrf,
    id_miri,
    id_usrp,
};

struct signal_estimate{
    bool change_frequency = false;
    double coarse_freq_offset = 0.0;
    bool frequency_changed = true;
    bool change_gain = false;
    int gain_offset = 0;
    bool gain_changed = true;
    double correct_resample = 0.0;
    bool reset = false;
    bool p1_reset = false;
    bool agc = false;
};

template<typename T> struct convert_iq
{
    void init(int _convert_input, float _scale, float _max, float _min)
    {
        convert_input = _convert_input;
        short_to_float = _scale;
        level_max = _max;
        level_min = _min;
    }
    void execute(int idx_in, int _len_in, T* _i_in, T* _q_in, complex * out, float & level_detect, signal_estimate & signal_)
    {

        int j = 0;
        for(int i = 0; i < _len_in; ++i) {
            j = (i + idx_in) * convert_input;
            complex tmp;
            if(std::is_same<T, float>())
                tmp = complex(_i_in[j], _q_in[j]);
            else
                tmp = complex(_i_in[j] * short_to_float, _q_in[j] * short_to_float);
            //___DC offset remove____________
            tmp -= exp_avg_dc(tmp);
            // remove spurs if any
            if(anti_spur_en)
            {
                anti_spur *= anti_spur_inc;
                anti_spur += (tmp - anti_spur) * anti_spur_alfa;
                tmp -= anti_spur;
            }
            //___IQ imbalance remove_________
            est_1_bit_quantization(tmp.real(), tmp.imag());
            float real = tmp.real() * c2;
            tmp = complex(real, tmp.imag() + c1 * real);
            out[i]=tmp;
            //_____________________________
        }
        //___IQ imbalance estimations___
        c1 = -theta1 / theta2;
        float c_temp = theta3 / theta2;
        c2 = sqrtf(c_temp * c_temp - c1 * c1);
        //___level gain estimation___
        level_detect = theta2 * theta3;
        if(signal_.gain_changed && signal_.agc) {
            if(level_detect < level_min) {
                signal_.gain_offset = 1;
                signal_.change_gain = true;
            }
            else if(level_detect > level_max) {
                signal_.gain_offset = -1;
                signal_.change_gain = true;
            }
            else {
                signal_.gain_offset = 0;
                signal_.change_gain = false;
            }
        }
    }
    void reset()
    {
        exp_avg_dc.reset();
    }
    void set_anti_spur(complex incr)
    {
        anti_spur_inc = incr;
        anti_spur_en = true;
    }
    void enable_anti_spur(bool state)
    {
        anti_spur_en = state;
    }
private:
    int convert_input = 1;
    float short_to_float = 1.0f/32768.0f;
    static constexpr float dc_ratio = 1.0e-6f;//1.0e-5f
    exponential_averager<complex, float, dc_ratio> exp_avg_dc;
    complex anti_spur{};
    complex anti_spur_inc{};
    static constexpr float anti_spur_alfa = 1.e-5f;
    bool anti_spur_en = false;

    float c1 = 0.0f;
    float c2 = 1.0f;
    float level_max=0.4f;
    float level_min=0.2f;
    float theta1 = 0.0f, theta2 = 0.0f, theta3 = 0.0f;
    static constexpr float theta_alfa = dc_ratio * 25.;
    inline void est_1_bit_quantization(float _real, float _imag)
    {
        float sgn;
        sgn = _real < 0 ? -1.0f : 1.0f;
        theta1 += (_imag * sgn - theta1)*theta_alfa;
        theta2 += (_real * sgn - theta2)*theta_alfa;
        sgn = _imag < 0 ? -1.0f : 1.0f;
        theta3 += (_imag * sgn -theta3)*theta_alfa;
    }

};

#include <QFile>
#include <QIODevice>
#include <QDateTime>

typedef std::vector<complex> file_sink_buf;
Q_DECLARE_METATYPE(file_sink_buf);

class file_sink : public QObject
{
    Q_OBJECT

public:
    explicit file_sink(long center, long sr)
    {
        QString dt = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss");
        fd.setFileName(QString("gqrx_%1_%2_%3_16.raw")
            .arg(dt)
            .arg(center)
            .arg(sr));
        fd.open(QIODevice::WriteOnly);
    };
    ~file_sink()
    {
        fd.close();
    };
    signals:
        void finished();
    public slots:
    void execute(std::vector<complex> data, unsigned len)
    {
        if(buf.size()<len*2)
            buf.resize(len*2);
        for(unsigned k=0;k<len;k++)
        {
            buf[k*2]=data[k].real()*scale;
            buf[k*2+1]=data[k].imag()*scale;
        }
        fd.write((const char *)&buf[0],len*2);
    }
    void stop()
    {
        emit finished();
    }
    private:
    static int reg()
    {
        qRegisterMetaType<file_sink_buf>();
        return 1;
    }
    static int reg_res;
    QFile fd;
    std::vector<int16_t> buf{};
    constexpr static float scale{32767.f};
};


class dvbt2_demodulator : public QObject
{
    Q_OBJECT

public:
    explicit dvbt2_demodulator(id_device_t _id_device, float _sample_rate, QObject *parent = nullptr);
    ~dvbt2_demodulator();
    void enable_display(bool mode)
    {
        enabled_display = mode;
    }

    QMutex* mutex;
    p1_symbol p1_demodulator{};
    p2_symbol p2_demodulator{};
    data_symbol data_demodulator{};
    fc_symbol fc_demod{};
    time_deinterleaver* deinterleaver = nullptr;

signals:
    void replace_null_indicator(const float _b1, const float _b2, const float _b3);
    void l1_dyn_execute(l1_postsignalling _l1_post);
    void amount_plp(int _num_plp);
    void data();
    void stop_deinterleaver();
    void finished();
    void dump(file_sink_buf buf, int len);

public slots:
    void execute(int _len_in, complex* _q_in, float _level_estimate, signal_estimate* signal_);
    void stop();
    void set_fir(int idx);

private:
    QThread* thread = nullptr;
    QMutex* mutex_out;
    QWaitCondition* signal_out;
    QThread* thread2 = nullptr;
    file_sink* dump0 = nullptr;

    id_device_t id_device;

    int remain = 0;
    int chunk = 0;
    int est_chunk = 0;

    constexpr static int samplerate_hz = SAMPLE_RATE;

    float phase_est_filtered = 0.0f;
    float phase_prev = 0.f;
    constexpr static float damping_ratio_ = 0.3f;
    constexpr static int bw_hz_ = 200000;
    proportional_integral_loop_filter<float, float, damping_ratio_, bw_hz_,
                                      samplerate_hz> loop_filter_phase_offset;
    float frequency_est_filtered = 0.0f;
    float frequency_est_coarse = 0.0f;
    constexpr static float damping_ratio = 0.7f;//0.7f
    constexpr static int bw_hz = 4000000;
    proportional_integral_loop_filter<float, float, damping_ratio, bw_hz,
                                      samplerate_hz> loop_filter_frequency_offset;

    double sample_rate_est_filtered = 0.0;

    std::vector<complex> out_derotate_sample;
    constexpr static uint upsample = DECIMATION_STEP;
    float sample_rate;
    double resample;
    double max_resample, min_resample;

    complex* out_interpolator{};
    std::vector<complex> out_decimator{};
    interpolator_farrow<complex, float> interpolator{};
    filter_decimator decimator{};

    dvbt2_parameters dvbt2;
    bool p2_init = false;
    void reset();
    void init_dvbt2();

    int symbol_size = P1_LEN;
    int idx_buffer_sym = 0;
    std::vector<complex> buffer_sym{};
    fast_fourier_transform fft{};
    complex* in_fft = nullptr;
    complex* ofdm_cell;
    complex nco = 1.f;

    pilot_generator pilot{};
    address_freq_deinterleaver fq_deinterleaver{};
    int next_symbol_type = SYMBOL_TYPE_P1;
    bool demodulator_init = false;
    bool deint_start = false;
    int idx_symbol = 0;
    bool crc32_l1_pre = false;
    l1_presignalling l1_pre;
    l1_postsignalling l1_post;

    bool frame_closing_symbol = false;
    int end_data_symbol = 1;

    bool change_frequency = false;
    float coarse_freq_offset = 0.0f;

    bool change_gain = false;
    int gain_offset = 0;
    bool enabled_display = false;

    void symbol_acquisition(int _len_in, complex* _in, signal_estimate *signal_);
    void set_guard_interval();
    void set_guard_interval_by_brute_force ();
    float level_detect = std::numeric_limits<float>::max();

};

#endif // DVBT2_DEMODULATOR_H
