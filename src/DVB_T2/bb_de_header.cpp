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
#include "bb_de_header.h"

#include <QMessageBox>
#include <memory>
#include <qmutex.h>
#include <qscopedpointer.h>
#include <qudpsocket.h>

//#include <QDebug>

#define CRC_POLY 0xAB
#define CRC_POLYR 0xD5
#define TRANSPORT_ERROR_INDICATOR 0x80
#define BIT_PACKET_LENGTH (TRANSPORT_PACKET_LENGTH * 8)

//------------------------------------------------------------------------------------------
bb_de_header::bb_de_header(QWaitCondition *_signal_in, QMutex *_mutex_in, QObject *parent) :
    QObject(parent),
    signal_in(_signal_in),
    mutex_in(_mutex_in),
    mutex_out(new QMutex())
{
    init_crc8_table();
}
//------------------------------------------------------------------------------------------
bb_de_header::~bb_de_header()
{
    delete mutex_out;
}
//------------------------------------------------------------------------------------------
void bb_de_header::init_crc8_table()
{
    int r, crc;
    for (int i = 0; i < 256; ++i) {
        r = i;
        crc = 0;
        for (int j = 7; j >= 0; --j) {
            if ((r & (1 << j) ? 1 : 0) ^ ((crc & 0x80) ? 1 : 0)) crc = (crc << 1) ^ CRC_POLYR;
            else  crc <<= 1;
        }
        crc_table[i] = static_cast<uint8_t>(crc);
    }
}
//------------------------------------------------------------------------------------------
uint8_t bb_de_header::check_crc8_mode(uint8_t *_in, int _len_in)
{
    uint8_t crc = 0;
    uint8_t b;
    int len_in = _len_in;
    uint8_t* in = _in;
    for (int i = 0; i < len_in; ++i) {
        b = in[i] ^ (crc & 0x01);
        crc >>= 1;
        if (b) crc ^= CRC_POLY;
    }
    return crc;
}
//------------------------------------------------------------------------------------------
template<typename T> bool unpack(T & out, const int bits, uint8_t * &in, const uint8_t * last)
{
    for (int i = bits - 1; (i >= 0) && (in < last); --i) {
      out |= *in++ << i;
    }
    return in < last;
}
//------------------------------------------------------------------------------------------
void bb_de_header::execute(int _plp_id, l1_postsignalling _l1_post, int _len_in, uint8_t* _in)
{
    mutex_in->lock();
    signal_in->wakeOne();

//                mutex_in->unlock();
//                return;

    l1_postsignalling &l1_post = _l1_post;
    uint8_t* in = _in;
    uint8_t* last = _in + _len_in;
    dvbt2_inputmode_t mode;
    int errors = 0;
    int len_split = 0;
    uint8_t temp;
    uint8_t* ptr_error_indicator = nullptr;
    switch(check_crc8_mode(in, BB_HEADER_LENGTH_BITS)){
    case 0:
        mode = INPUTMODE_NORMAL;
        break;
    case CRC_POLY:
        mode = INPUTMODE_HIEFF;
        break;
    default:
        info_already_set = false;
        emit ts_stage("Baseband header CRC8 error.");
        mutex_in->unlock();
        return;
    }
    bb_header header;
    header.ts_gs = *in++ << 1;
    header.ts_gs |= *in++;
    header.sis_mis = *in++;
    header.ccm_acm = *in++;
    header.issyi = *in++;
    header.npd = *in++;
    header.ext = *in++ << 1;
    header.ext |= *in++;
    header.isi = 0;
    if (header.sis_mis == 0) {
        if(!unpack(header.isi, 8, in, last))
        {
error_lengh:
            emit ts_stage("Baseband header length error.");
            mutex_in->unlock();
            return;
        }
    }
    else {
        in += 8;
    }
    header.upl = 0;
    header.dfl = 0;
    header.sync = 0;
    header.syncd = 0;

    if(!info_already_set) set_info(_plp_id, l1_post, mode, header);

    if(!unpack(header.upl, 16, in, last))
        goto error_lengh;
    if(!unpack(header.dfl, 16, in, last))
        goto error_lengh;
//    if(header.dfl > len_in - 80) {
//        qDebug() << "bb_de_header::execute" << "header.dfl=" << header.dfl << "len_in=" << len_in;
//        return;
//    }
    if(!unpack(header.sync, 8, in, last))
        goto error_lengh;
    if(!unpack(header.syncd, 16, in, last))
        goto error_lengh;
    if(header.syncd == 65535) {
        mutex_in->unlock();
        return;
    }
    in += 8;

    plp_context& ctx = plp_contexts[_plp_id];

    if (mode == INPUTMODE_NORMAL) {
        if(ctx.split){
            ctx.split = false;
            if(ctx.len_out<len)
            {
                *(ctx.out++) = ctx.buffer[0];
                ++ctx.len_out;
            }
            ptr_error_indicator = ctx.out;
            for (int i = 1; (i < ctx.idx_buffer) && (ctx.len_out < len); ++i) {
                *(ctx.out++) = ctx.buffer[i];
                ++ctx.len_out;
            }
            len_split = TRANSPORT_PACKET_LENGTH - ctx.idx_packet;
            int syncd_byte = header.syncd / 8;
            if(len_split == syncd_byte){
                for (int i = 0; i < len_split; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    if(ctx.len_out<len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
                temp = 0;
                unpack(temp, 8, in, last);
                if(temp != ctx.crc){
                    ++errors;
                    if(ptr_error_indicator != nullptr)
                        *ptr_error_indicator |= TRANSPORT_ERROR_INDICATOR;
                }
                ctx.crc = 0;
            }
            else if(len_split < syncd_byte){
                for (int i = 0; i < syncd_byte; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    if(ctx.len_out<len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
                temp = 0;
                unpack(temp, 8, in, last);
                if(temp != ctx.crc){
                    ++errors;
                    if(ptr_error_indicator != nullptr)
                        *ptr_error_indicator |= TRANSPORT_ERROR_INDICATOR;
                }
                ctx.crc = 0;
                emit ts_stage(QString("Baseband header resynchronizing, N %1 < %2.").arg(len_split).arg(syncd_byte));
            }
            else{
                for (int i = 0; i < syncd_byte; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(ctx.len_out<len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                    }
                    ++ctx.idx_packet;
                }
                int dump = len_split - syncd_byte;
                for (int i = 0; (i < dump) && (ctx.len_out < len); ++i) {
                    *(ctx.out++) = 0xF0;
                    ++ctx.len_out;
                    ++ctx.idx_packet;
                }
                ++errors;
                if(ptr_error_indicator != nullptr)
                    *ptr_error_indicator |= TRANSPORT_ERROR_INDICATOR;
                emit ts_stage(QString("Baseband header resynchronizing, N %1 > %2.").arg(len_split).arg(syncd_byte));
            }
        }
        else {
            in += header.syncd + 8;
        }
        header.dfl -= header.syncd + 8;
//        if(len < (len_out + header.dfl / 8)) {
//            mutex_in->unlock();
//            emit ts_stage("Baseband header error.");
//            return;
//        }
        while (header.dfl > 0) {
            if (header.dfl < BIT_PACKET_LENGTH) {
                ctx.split = true;
                len_split = header.dfl / 8;
                ctx.idx_buffer = 0;
                for (int i = 0; i < len_split; ++i) {
                    if(ctx.idx_packet >= TRANSPORT_PACKET_LENGTH) {
                        ctx.idx_packet = 0;
                        temp = 0;
                        unpack(temp, 8, in, last);
                        if(temp != ctx.crc){
                            ++errors;
                            if(ptr_error_indicator != nullptr)
                                *ptr_error_indicator |= TRANSPORT_ERROR_INDICATOR;
                        }
                        ctx.crc = 0;
                        ctx.buffer[ctx.idx_buffer++] = 0x47;//static_cast<unsigned char>(header.sync);
                        ++ctx.idx_packet;
                    }
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    ctx.buffer[ctx.idx_buffer++] = temp;
                    ++ctx.idx_packet;
                }
                header.dfl = 0;
            }
            else{
                if(ctx.idx_packet >= TRANSPORT_PACKET_LENGTH){
                    ctx.idx_packet = 0;
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(temp != ctx.crc){
                        ++errors;
                        if(ptr_error_indicator != nullptr)
                            *ptr_error_indicator |= TRANSPORT_ERROR_INDICATOR;
                    }
                    ctx.crc = 0;
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = 0x47;//static_cast<unsigned char>(header.sync);
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    ptr_error_indicator = ctx.out;
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    header.dfl -= 8;
                }
                else if(ctx.idx_packet == 0){
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = 0x47;//static_cast<unsigned char>(header.sync);
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    ptr_error_indicator = ctx.out;
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    header.dfl -= 8;
                }
                else{
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.crc = crc_table[temp ^ ctx.crc];
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    header.dfl -= 8;
                }
            }
        }
    }
    else {
        if(ctx.split){
            ctx.split = false;
            for (int i = 0; (i < ctx.idx_buffer) && (ctx.len_out < len); ++i) {
                *(ctx.out++) = ctx.buffer[i];
                ++ctx.len_out;
            }
            len_split = TRANSPORT_PACKET_LENGTH - ctx.idx_packet;
            int syncd_byte = header.syncd / 8;
            if(len_split == syncd_byte) {
                for (int i = 0; i < len_split; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
            }
            else if(len_split < syncd_byte){
                for (int i = 0; i < len_split; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
                in += header.syncd - len_split * 8;
                emit ts_stage(QString("Baseband header resynchronizing, %1 < %2.").arg(len_split).arg(syncd_byte));
            }
            else{
                for (int i = 0; i < syncd_byte; ++i) {
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
                int dump = len_split - syncd_byte;
                for (int i = 0; (i < dump) && (ctx.len_out < len); ++i) {
                    *(ctx.out++) = 0xF0;
                    ++ctx.len_out;
                    ++ctx.idx_packet;
                }
                emit ts_stage(QString("Baseband header resynchronizing, %1 > %2.").arg(len_split).arg(syncd_byte));
            }
        }
        else {
            in += header.syncd;
        }
        header.dfl -= header.syncd;

        while (header.dfl > 0) {
            if (header.dfl < BIT_PACKET_LENGTH) {
                ctx.split = true;
                len_split = header.dfl / 8;
                ctx.idx_buffer = 0;
                for (int i = 0; i < len_split; ++i) {
                    if(ctx.idx_packet == TRANSPORT_PACKET_LENGTH) {
                        ctx.idx_packet = 0;
                        ctx.buffer[ctx.idx_buffer++] = 0x47;//static_cast<unsigned char>(header.sync);
                        ++ctx.idx_packet;
                    }
                    temp = 0;
                    unpack(temp, 8, in, last);
                    ctx.buffer[ctx.idx_buffer++] = temp;
                    ++ctx.idx_packet;
                }
                header.dfl = 0;
            }
            else{
                if(ctx.idx_packet >= TRANSPORT_PACKET_LENGTH || ctx.idx_packet == 0){
                    ctx.idx_packet = 0;
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = 0x47;//static_cast<unsigned char>(header.sync);
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                }
                else{
                    temp = 0;
                    unpack(temp, 8, in, last);
                    if(ctx.len_out < len)
                    {
                        *(ctx.out++) = temp;
                        ++ctx.len_out;
                        ++ctx.idx_packet;
                    }
                    header.dfl -= 8;
                }
            }
        }
    }
    ctx.out = ctx.begin_out;

    mutex_out->lock();
    for(const auto& device: out_devices)
    {
        if(device.first != _plp_id)
            continue;

        if(device.second.out_type == id_out::out_file)
        {
            device.second.stream_ptr->writeRawData((char*) ctx.out, sizeof(uint8_t) * static_cast<unsigned long>(ctx.len_out));
        }
        else if(device.second.out_type == id_out::out_network)
        {
            const QHostAddress& addr = out_params[_plp_id].udp_addr;
            const qint16 port = out_params[_plp_id].udp_port;

            device.second.socket_ptr->writeDatagram((char*) ctx.out, sizeof(uint8_t) * static_cast<unsigned long>(ctx.len_out), addr, port);
        }
    }
    mutex_out->unlock();

    ctx.len_out = 0;

    if(errors != 0) emit ts_stage("TS error.");

    mutex_in->unlock();
}
//_____________________________________________________________________________________________
void bb_de_header::set_info(int _plp_id, l1_postsignalling &_l1_post,
                            dvbt2_inputmode_t mode, bb_header header)
{
    if(_plp_id != next_plp_info) return;

    QString temp;
    info += "PLP :\t" + QString::number(_plp_id) + "\n";
    if(mode == INPUTMODE_HIEFF) temp = "HEM";
    else temp = "NM";
    info += "Mode\t\t" + temp + "\n";
    switch(header.ts_gs){
    case 0:
        temp = "GFPS(not supported)";
    break;
    case 1:
        temp = "GCS(not supported)";
    break;
    case 2:
        temp = "GSE(not supported)";
    break;
    case 3:
        temp = "TS";
    break;
    default:
        temp = "unknow";
    break;
    }
    info += "TS/GS\t\t" + temp + "\n";
    if(header.sis_mis) temp = "single";
    else temp = "multiple";
    info += "SIS/MIS\t\t " + temp + "\n";
    if(header.issyi) temp = "yes";
    else temp = "no";
    info += "ISSYI\t\t" + temp + "\n";
    if(header.npd)temp = "yes";
    else temp = "no";
    info += "NDP\t\t" + temp;

    ++next_plp_info;
    if(next_plp_info == _l1_post.num_plp) {
        next_plp_info = 0;
        info_already_set = true;
        emit ts_stage(info);
        info = "";
    }
    else{
        info += "\n";
    }
}
//_____________________________________________________________________________________________
void bb_de_header::set_out(std::map<int, plp_out_params> new_out_params)
{
    mutex_out->lock();

    out_params = new_out_params;

    // will close all files and sockets automatically
    out_devices.clear();

    for(const auto& params: out_params)
    {
        if(params.second.out_type == id_out::out_file)
        {
            std::unique_ptr<QFile> new_file_ptr(new QFile(params.second.filename));
            std::unique_ptr<QDataStream> new_stream_ptr(new QDataStream());

            if(new_file_ptr->open(QIODevice::WriteOnly))
            {
                new_stream_ptr->setDevice(new_file_ptr.get());
            }
            else
            {
                QMessageBox::information(nullptr, "Error", new_file_ptr->errorString());
                continue;
            }

            out_devices[params.first].out_type = id_out::out_file;
            out_devices[params.first].file_ptr.swap(new_file_ptr);
            out_devices[params.first].stream_ptr.swap(new_stream_ptr);
        }
        else if(params.second.out_type == id_out::out_network)
        {
            std::unique_ptr<QUdpSocket> new_socket_ptr(new QUdpSocket());

            out_devices[params.first].out_type = id_out::out_network;
            out_devices[params.first].socket_ptr.swap(new_socket_ptr);
        }
        else
        {
            // throw something
        }
    }

    mutex_out->unlock();
}
//_____________________________________________________________________________________________
void bb_de_header::stop()
{
    emit finished();
}
//_____________________________________________________________________________________________
