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
#ifndef BUFFERS_HH
#define BUFFERS_HH

#include <cstring>
#include <cstdlib>
#include <vector>
#include <deque>

template<typename T, int LEN>
class sum_of_buffer
{
private:
    static const int len = LEN;
    T buffer[len];
    int idx = 0;
    T sum;

public:
    sum_of_buffer() {reset();}

    T operator()(const T &_in)
    {
        buffer[idx++] = _in;
        idx = idx % len;
        sum = sum - buffer[idx]  + _in;
        return sum /*/ static_cast<float>(len)*/;
    }

    void reset()
    {
        memset(buffer, 0, sizeof(T) * static_cast<unsigned int>(len));
        idx = 0;
        sum = 0;
    }
};

template<typename T, int DELAY>
class delay_buffer
{
private:
    static const int len = DELAY + 1;
    T buffer[len];
    unsigned int idx = 0;

public:
    delay_buffer() { reset(); }
    ~delay_buffer() {}

    T operator()(const T &_in)
    {
        buffer[idx++] = _in;
        idx = idx % len;
        return buffer[idx];
    }
    void reset(){
        idx = 0;
        memset(buffer, 0, sizeof(T) * len);
    }
};

template<typename T, int LEN>
class save_buffer
{
private:
    static constexpr int len = LEN;
    T buffer[LEN*2];
    int idx = 0;

public:
    save_buffer(){ reset();}

    void write(const T &_in)
    {
        buffer[idx] = _in;
        buffer[idx+len] = _in;
        idx++;
        idx = idx % len;
    }

    T* read()
    {
        return &buffer[idx];
    }
    void reset(){
        idx = 0;
        memset(buffer, 0, sizeof(T) * static_cast<unsigned int>(len*2));
    }
};

template<typename T>
class vector_fifo
{
private:
    std::deque<std::vector<T>> empty{};
    std::deque<std::vector<T>> queued{};

public:
    vector_fifo(){}

    void take(std::vector<T>& v)
    {
        if(empty.size()==0)
            v=std::vector<T>();
        else{
            v=std::move(empty.back());
            empty.pop_back();
        }
    }

    void release(std::vector<T>& v)
    {
        empty.emplace_back(std::move(v));
    }

    void push(std::vector<T>& v)
    {
        queued.emplace_back(std::move(v));
    }

    bool shift(std::vector<T>& v)
    {
        if(queued.size()==0)
            return false;
        v=std::move(queued.front());
        queued.pop_front();
        return true;
    }

    void reset()
    {
        queued.clear();
    }
};

#endif // BUFFERS_HH
