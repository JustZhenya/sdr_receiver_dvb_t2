#ifndef FILTER_DECIMATOR_H
#define FILTER_DECIMATOR_H

//#include <QDebug>
#include <complex>
#include <immintrin.h>
#include <string.h>

typedef std::complex<float> complex;

#define DECIMATION_STEP 2

class filter_decimator
{
private:
    complex* buffer;
    unsigned int idx_buffer = 0;
    float* h_complex;
    constexpr static int MAX_TAPS = 64;
    int selected_fir = 0;
    constexpr static int DEFAULT_FIR = 2;
    unsigned len_fir_filter = 0;
    struct tap_t
    {
        const char * name;
        unsigned len;
        double taps[MAX_TAPS];
    };
    constexpr static tap_t h_fir[]=
    {
        {
            // Equiripple low-pass
            // Sample rate = 19
            // Gain = 1
            // Pass band = 3.7
            // Stop band = 5.4
            // Stop band attenuation = 34
            // Pass band ripple = 1
            .name = "Soft",
            .len = 16,
            .taps = {
                -0.018701320635354134, 0.004934910607844416, 0.04723822866101708, 0.025256800124934517,
                -0.0672801895588082, -0.05991127980233717, 0.16656930534107722, 0.42859044506915356,
                0.42859044506915356, 0.16656930534107722, -0.05991127980233717, -0.0672801895588082,
                0.025256800124934517, 0.04723822866101708, 0.004934910607844416, -0.018701320635354134
            }
        },
        {
            // Equiripple low-pass
            // Sample rate = 19
            // Gain = 1
            // Pass band = 4
            // Stop band = 5
            // Stop band attenuation = 45
            // Pass band ripple = 1
            .name = "Medium",
            .len = 32,
            .taps = {
                0.007017988989943076, 0.01925335533939853, 0.011602701686115451, -0.00921278865729551,
                -0.015394377450419524, 0.009050776507014496, 0.023090654014189874, -0.006122160629039453,
                -0.034766154400964984, -0.0010454617855244716, 0.051586187677569895, 0.017035326174651977,
                -0.0810256866479648, -0.05919634747334521, 0.17243891300003064, 0.4229398469686388,
                0.4229398469686388, 0.17243891300003064, -0.05919634747334521, -0.0810256866479648,
                0.017035326174651977, 0.051586187677569895, -0.0010454617855244716, -0.034766154400964984,
                -0.006122160629039453, 0.023090654014189874, 0.009050776507014496, -0.015394377450419524,
                -0.00921278865729551, 0.011602701686115451, 0.01925335533939853, 0.007017988989943076
            }
        },
        {
            .name = "Sharp",
            .len = 64,
            .taps = {
                9.1776e-04, -8.7999e-05, -1.5371e-03, -3.5994e-04,  2.2031e-03,  1.2190e-03,
                -2.7671e-03, -2.5573e-03,  3.0238e-03,  4.3827e-03, -2.7246e-03, -6.6208e-03,
                1.5959e-03,  9.0978e-03,  6.3727e-04, -1.1531e-02, -4.2324e-03,  1.3522e-02,
                9.4232e-03, -1.4551e-02, -1.6447e-02,  1.3930e-02,  2.5643e-02, -1.0675e-02,
                -3.7747e-02,  3.0430e-03,  5.4821e-02,  1.3260e-02, -8.4349e-02, -5.5651e-02,
                1.7580e-01,  4.1952e-01,  4.1952e-01,  1.7580e-01, -5.5651e-02, -8.4349e-02,
                1.3260e-02,  5.4821e-02,  3.0430e-03, -3.7747e-02, -1.0675e-02,  2.5643e-02,
                1.3930e-02, -1.6447e-02, -1.4551e-02,  9.4232e-03,  1.3522e-02, -4.2324e-03,
                -1.1531e-02,  6.3727e-04,  9.0978e-03,  1.5959e-03, -6.6208e-03, -2.7246e-03,
                4.3827e-03,  3.0238e-03, -2.5573e-03, -2.7671e-03,  1.2190e-03,  2.2031e-03,
                -3.5994e-04, -1.5371e-03, -8.7999e-05,  9.1776e-04
            }
        },
        {
            // Equiripple low-pass
            // Sample rate = 19
            // Gain = 1
            // Pass band = 3.7
            // Stop band = 5.4
            // Stop band attenuation = 31
            // Pass band ripple = 1
            .name = "Test1",
            .len = 16,
            .taps = {
                -0.022734422334839785, -0.0012881908423919947, 0.04385114017308602, 0.02670300516408754,
                -0.06546733084584877, -0.06074709562825404, 0.1666961461503596, 0.43259019216028566,
                0.43259019216028566, 0.1666961461503596, -0.06074709562825404, -0.06546733084584877,
                0.02670300516408754, 0.04385114017308602, -0.0012881908423919947, -0.022734422334839785
            }
        },
        {
            // Equiripple low-pass
            // Sample rate = 19
            // Gain = 1
            // Pass band = 3.8
            // Stop band = 5.4
            // Stop band attenuation = 31
            // Pass band ripple = 1
            .name = "Test2",
            .len = 16,
            .taps = {
                -0.027270990951166058, -0.00714389169108699, 0.042686452204675825, 0.03056528671310898,
                -0.06428310636289203, -0.06519619626073676, 0.1642110520722837, 0.43703238462973476,
                0.43703238462973476, 0.1642110520722837, -0.06519619626073676, -0.06428310636289203,
                0.03056528671310898, 0.042686452204675825, -0.00714389169108699, -0.027270990951166058
            }
        },
    };
    constexpr static int N_FILTERS = sizeof(h_fir)/sizeof(h_fir[0]);;
    // for 8mHz bandwidht
    unsigned int len_filter_complex = 0;//for complex numbers
    const static unsigned int len_buffer = MAX_TAPS * 2;        // min 2 x len_fir_filter

public:
    filter_decimator()
    {
        h_complex = static_cast<float*>(_mm_malloc(MAX_TAPS * 2 * sizeof(float), 32));
        set_filter(DEFAULT_FIR);
        buffer = static_cast<complex*>(_mm_malloc(len_buffer * 2 * sizeof(complex), 32));
        for (unsigned int i = 0; i < len_buffer * 2; ++i) buffer[i] = {0.0f, 0.0f};
    }
    //-----------------------------------------------------------------------------------------------
    void set_filter(int n)
    {
        if(h_fir[selected_fir].len > MAX_TAPS){
            printf("Filter should not have more than %d taps /n", MAX_TAPS);
            exit(1);
        }
        selected_fir = n;
        len_fir_filter = h_fir[selected_fir].len;
        int j = 0;
        for (unsigned int i = 0; i < len_fir_filter; ++i)
        {
            h_complex[j++] = (i < h_fir[selected_fir].len)?static_cast<float>(h_fir[selected_fir].taps[i]):0.f;
            h_complex[j++] = (i < h_fir[selected_fir].len)?static_cast<float>(h_fir[selected_fir].taps[i]):0.f;
        }
        if(len_fir_filter & 0x0f)
        {
            len_fir_filter &= ~0x0f;
            len_fir_filter ++;
        }
        len_filter_complex = len_fir_filter * 2;
    }
    //-----------------------------------------------------------------------------------------------
    static void get_filter_names(std::vector<const char *> & out)
    {
        out.resize(N_FILTERS);
        for(int i = 0; i < N_FILTERS; i++)
            out[i]=h_fir[i].name;
    }
    //-----------------------------------------------------------------------------------------------
    ~filter_decimator()
    {
        _mm_free(h_complex);
        _mm_free(buffer);
    }
    //-----------------------------------------------------------------------------------------------
    void execute(int _len_in, complex* _in, int &_len_out, complex* _out)
    {
        int len_in = _len_in;
        float* ptr_buffer;
        int idx_out = 0;
        static int d = 0;
        __m256 v0, v1, v2, v3;          // input vectors
        __m256 h0, h1, h2, h3;          // coefficients vectors
        __m256 m0, m1, m2, m3;
        __m256 sum0, sum1, sumt, sum;
        float* st = static_cast<float*>(_mm_malloc(sizeof(float) * 8, 32));
        if(len_fir_filter == 16)
        {
            // load coefficients into register (aligned)
            h0 = _mm256_load_ps(h_complex);
            h1 = _mm256_load_ps(h_complex + 8);
            h2 = _mm256_load_ps(h_complex + 16);
            h3 = _mm256_load_ps(h_complex + 24);
            for (int x = 0; x < len_in; ++x) {
                buffer[idx_buffer] = buffer[idx_buffer + len_buffer] = _in[x];
                idx_buffer++;
                if(idx_buffer == len_buffer)
                    idx_buffer = 0;
                ptr_buffer = reinterpret_cast<float*>(buffer + idx_buffer);
                ++d;
                if(d == DECIMATION_STEP){
                    d = 0;
                    // load inputs into register (unaligned)
                    v0 = _mm256_loadu_ps(ptr_buffer);
                    v1 = _mm256_loadu_ps(ptr_buffer + 8);
                    v2 = _mm256_loadu_ps(ptr_buffer + 16);
                    v3 = _mm256_loadu_ps(ptr_buffer + 24);
                    // compute multiplication
                    m0 = _mm256_mul_ps(v0, h0);
                    m1 = _mm256_mul_ps(v1, h1);
                    m2 = _mm256_mul_ps(v2, h2);
                    m3 = _mm256_mul_ps(v3, h3);
                    // parallel addition
                    sum0 = _mm256_add_ps(m0, m1);
                    sum1 = _mm256_add_ps(m2, m3);
                    sum = _mm256_add_ps(sum0, sum1);
                    _mm256_store_ps(st, sum);
                    float real_out = st[0] + st[2] + st[4] + st[6];
                    float imag_out = st[1] + st[3] + st[5] + st[7];
                    // set return value
                    _out[idx_out].real(real_out);
                    _out[idx_out].imag(imag_out);
                    ++idx_out;
                }
            }
        }else
            for (int x = 0; x < len_in; ++x) {
                buffer[idx_buffer] = buffer[idx_buffer + len_buffer] = _in[x];
                idx_buffer++;
                if(idx_buffer == len_buffer)
                    idx_buffer = 0;
                ptr_buffer = reinterpret_cast<float*>(buffer + idx_buffer);
                ++d;
                if(d == DECIMATION_STEP){
                    d = 0;
                    sum = _mm256_setzero_ps();
                    for (unsigned int i = 0; i < len_filter_complex; i += 32){
                        // load inputs into register (unaligned)
                        v0 = _mm256_loadu_ps(ptr_buffer + i);
                        v1 = _mm256_loadu_ps(ptr_buffer + i + 8);
                        v2 = _mm256_loadu_ps(ptr_buffer + i + 16);
                        v3 = _mm256_loadu_ps(ptr_buffer + i + 24);
                        // load coefficients into register (aligned)
                        h0 = _mm256_load_ps(h_complex + i);
                        h1 = _mm256_load_ps(h_complex + i + 8);
                        h2 = _mm256_load_ps(h_complex + i + 16);
                        h3 = _mm256_load_ps(h_complex + i + 24);
                        // compute multiplication
                        m0 = _mm256_mul_ps(v0, h0);
                        m1 = _mm256_mul_ps(v1, h1);
                        m2 = _mm256_mul_ps(v2, h2);
                        m3 = _mm256_mul_ps(v3, h3);
                        // parallel addition
                        sum0 = _mm256_add_ps(m0, m1);
                        sum1 = _mm256_add_ps(m2, m3);
                        sumt = _mm256_add_ps(sum0, sum1);
                        sum = _mm256_add_ps(sum, sumt);
                    }
                    _mm256_store_ps(st, sum);
                    float real_out = st[0] + st[2] + st[4] + st[6];
                    float imag_out = st[1] + st[3] + st[5] + st[7];
                    // set return value
                    _out[idx_out].real(real_out);
                    _out[idx_out].imag(imag_out);
                    ++idx_out;
                }
            }
        _len_out = idx_out;
    }
};

#endif // FILTER_DECIMATOR_H
