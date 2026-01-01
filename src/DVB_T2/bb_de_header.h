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
#ifndef BB_DE_HEADER_H
#define BB_DE_HEADER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QUdpSocket>
#include <QTextStream>
#include <QDataStream>
#include <QFile>

#include "dvbt2_definition.h"

#define BB_HEADER_LENGTH_BITS 80
#define TS_GS_TRANSPORT          3
#define TS_GS_GENERIC_PACKETIZED 0
#define TS_GS_GENERIC_CONTINUOUS 1
#define TS_GS_RESERVED           2
#define SIS_MIS_SINGLE   1
#define SIS_MIS_MULTIPLE 0
#define CCM 1
#define ACM 0
#define ISSYI_ACTIVE     1
#define ISSYI_NOT_ACTIVE 0
#define NPD_ACTIVE       1
#define NPD_NOT_ACTIVE   0

#define TRANSPORT_PACKET_LENGTH 188

class bb_de_header : public QObject
{
    Q_OBJECT
public:
    explicit bb_de_header(QWaitCondition* _signal_in, QMutex* _mutex_in, QObject *parent = nullptr);
    ~bb_de_header();

    enum class id_out {
        out_network,
        out_file,
    };

    struct plp_out_params
    {
        id_out out_type = id_out::out_network;

        // out_network
        QHostAddress udp_addr;
        qint16 udp_port;

        // out_file
        QString filename;
    };

    struct plp_out_device
    {
        id_out out_type;

        // out_network
        std::unique_ptr<QUdpSocket> socket_ptr;

        // out_file
        std::unique_ptr<QFile> file_ptr;
        std::unique_ptr<QDataStream> stream_ptr;
    };

signals:
    void finished();
    void ts_stage(QString _info);

public slots:
    void execute(int _plp_id, l1_postsignalling _l1_post, int _len_in, uint8_t* _in);
    void set_out(std::map<int, plp_out_params> new_out_params);
    void stop();

private:
    QWaitCondition* signal_in;
    QMutex* mutex_in;
    
    uint8_t crc_table[256];
    void init_crc8_table();
    static uint8_t check_crc8_mode(uint8_t *_in, int _len_in);

    static constexpr int len = 53840 / 8 + TRANSPORT_PACKET_LENGTH * 2; //split tail ?
    
    struct bb_header{
        int ts_gs;
        int sis_mis;
        int ccm_acm;
        int issyi;
        int npd;
        int ext;
        int isi;
        int upl;
        int dfl;
        int sync;
        int syncd;
    };

    struct plp_context
    {
        uint8_t crc = 0;
        int idx_packet = 0;
        int idx_buffer = 0;
        bool split = false;
        uint8_t buffer[TRANSPORT_PACKET_LENGTH];
        uint8_t begin_out[len];
        uint8_t* out = begin_out;
        int len_out = 0;
    };

    std::map<int, plp_context> plp_contexts;

    QMutex* mutex_out;
    std::map<int, plp_out_params> out_params;
    std::map<int, plp_out_device> out_devices;

    bool info_already_set = false;
    QString info = "";
    int next_plp_info = 0;
    void set_info(int _plp_id, l1_postsignalling &_l1_post, dvbt2_inputmode_t mode, bb_header header);
};

#endif // BB_DE_HEADER_H
