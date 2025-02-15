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
    unsigned int idx_buffer;
    unsigned int wrap_point;
    unsigned int begin_point;
    unsigned int len_copy;
    float* h_complex;
    // for 8mHz bandwidht
    #if 0
    constexpr static int len_fir_filter = 64;
    constexpr static double h_fir[len_fir_filter] =
    {
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
    };
    #else
    constexpr static int len_fir_filter = 16;
    constexpr static double h_fir[len_fir_filter] =
    {
    #if 0
    -0.019591155861806583, -0.010732715988660114, 0.04673831333542586, 0.05748382792312952, -0.04517585355278695, -0.0821555023822493, 0.1473190711816771, 0.45611773109592785, 0.45611773109592785, 0.1473190711816771, -0.0821555023822493, -0.04517585355278695, 0.05748382792312952, 0.04673831333542586, -0.010732715988660114, -0.019591155861806583
    #else
    -0.01083066929249986, 0.012657640751554573, 0.058410591321007295, 0.038631130510733344, -0.06429965619415155, -0.07190057928574271, 0.15823255849757947, 0.43691700148357343, 0.43691700148357343, 0.15823255849757947, -0.07190057928574271, -0.06429965619415155, 0.038631130510733344, 0.058410591321007295, 0.012657640751554573, -0.01083066929249986
    #endif
    };
    #endif
//    const double h_fir_odd = h_fir[0];
    const static unsigned int len_filter_complex = len_fir_filter * 2;//for complex numbers
    const static unsigned int len_buffer = len_fir_filter * 2;        // min 2 x len_fir_filter

public:
    filter_decimator()
    {
        if(len_fir_filter % 2 != 0 || len_fir_filter < 16){
            printf("Only even filter and minimum 16 taps /n");
            exit(1);
        }
        h_complex = static_cast<float*>(_mm_malloc(len_filter_complex * sizeof(float), 32));
        int j = 0;
        for (unsigned int i = 0; i < len_fir_filter; ++i) {
            h_complex[j++] = static_cast<float>(h_fir[i]);
            h_complex[j++] = static_cast<float>(h_fir[i]);
        }
        buffer = static_cast<complex*>(_mm_malloc(len_buffer * 2 * sizeof(complex), 32));
        for (unsigned int i = 0; i < len_buffer * 2; ++i) buffer[i] = {0.0f, 0.0f};
        wrap_point = len_buffer / 2 + 1;
        begin_point = len_buffer / 2 - 1;
        idx_buffer = begin_point;
        len_copy = sizeof(complex) * begin_point;
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
