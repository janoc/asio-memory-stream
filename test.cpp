
#include <stdlib.h>
#include <iostream>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include <MemoryStreamService.h>

namespace ba = boost::asio;

#define BUF_SIZE 20

class RandomDataGenerator
{
    public:
        RandomDataGenerator(MemoryStream &svc) : m_svc(svc), m_packet_cnt(0) {};

        void run()
        {
            boost::random::mt19937 gen;

            std::string chars(
                    "abcdefghijklmnopqrstuvwxyz"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "1234567890"
                    "!@#$%^&*()"
                    "`~-_=+[{]{\\|;:'\",<.>/? ");

            boost::random::uniform_int_distribution<> dist(1, 200);
            boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

            while(true)
            {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(dist(gen)));

                std::stringstream ss;
                ss << m_packet_cnt++ << " " << ">| ";
                for(int i = 0; i < 8; ++i)
                {
                    ss << chars[index_dist(gen)];
                }
                ss << " |<" << std::endl;

                m_svc.pushReadBuffer(ss.str());
            }
        }

    private:
        std::size_t m_packet_cnt;
        MemoryStream &m_svc;

};

class AsyncReader
{
    public:
        AsyncReader(MemoryStream &svc) : m_svc(svc)
        {
            buf = new char[BUF_SIZE + 1];
            memset(buf, 0, BUF_SIZE + 1);
        }

        virtual ~AsyncReader()
        {
            delete[] buf;
        }

        void do_read()
        {
            ba::async_read(m_svc, ba::buffer(buf, BUF_SIZE),
                           boost::bind(&AsyncReader::read_done_handler, this, boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
        }

        void read_done_handler(const boost::system::error_code& ec,
                               std::size_t bytes_transferred)
        {
            std::cout << buf;

            do_read();
        }

    private:
        char *buf;
        MemoryStream &m_svc;
};

class AsyncWriter
{
    public:
        AsyncWriter(MemoryStream &svc) : m_svc(svc)
        {
            buf = "0123456789 ";
        }

        virtual ~AsyncWriter()
        {
        }

        void do_write()
        {
            ba::async_write(m_svc, ba::buffer((void *)buf.data(), buf.size()),
                            boost::bind(&AsyncWriter::write_done_handler, this, boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
        }

        void write_done_handler(const boost::system::error_code& ec,
                                std::size_t bytes_transferred)
        {
            do_write();
        }

    private:
        std::string buf;
        MemoryStream &m_svc;
};

int main()
{
    ba::io_service io_service;

    MemoryStream port(io_service);

    // spin up worker thread for the io_service
    boost::thread worker_thread(boost::bind(&boost::asio::io_service::run, &io_service));

    // spin up the dummy data generator simulating packets from server
    RandomDataGenerator data_gen(port);
    boost::thread worker_thread2(boost::bind(&RandomDataGenerator::run, &data_gen));

    // async reader & writer
    AsyncReader reader(port);
    AsyncWriter writer(port);
    reader.do_read();
    writer.do_write();

    while(true)
    {
        std::cerr << port.popWriteBuffer() << std::endl;
        //boost::this_thread::sleep_for(boost::chrono::seconds(1));
    }

}
