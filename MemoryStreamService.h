/*
 * MemoryStreamService.h
 *
 *  Created on: Nov 16, 2013
 *      Author: janoc
 */

#ifndef MEMORYSTREAMSERVICE_H_
#define MEMORYSTREAMSERVICE_H_

#include <cstddef>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>

#include <boost/thread/mutex.hpp>

class memory_stream_impl
{
    public:
        memory_stream_impl() :
            m_read_data_available(false),
            m_write_data_available(false)
        {};

        virtual ~memory_stream_impl() {};

        virtual void destroy() {}

        template <typename MutableBufferSequence>
        std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec)
        {
            // wait for new data to arrive
            boost::unique_lock<boost::mutex> lock(m_read_mutex);
            while(!m_read_data_available)
                m_cond_read.wait(lock);

            char *buf_pos = &m_read_buffer[0];
            std::size_t remains = m_read_buffer.size();
            std::size_t read    = 0;

            // fill the buffers
            typename MutableBufferSequence::const_iterator it;
            for(it = buffers.begin(); it != buffers.end(); ++it)
            {
                std::size_t b_size = boost::asio::buffer_size(*it);
                if(b_size < remains)
                {
                    // fill as much as possible
                    boost::asio::buffer_copy((*it), boost::asio::mutable_buffer((void *)buf_pos, b_size));
                    read += b_size;
                    buf_pos += b_size;
                    remains -= b_size;
                } else
                {
                    // fill the remaining bit, we will block next time because the buffer is now empty
                    boost::asio::buffer_copy((*it), boost::asio::mutable_buffer((void *)buf_pos, remains));
                    read += remains;
                    remains = 0;
                    m_read_data_available = false;
                    break;
                }
            }
            m_read_buffer.erase(0, read);

            ec = boost::system::error_code();
            return read;
        }

        /// Write the given data to the stream.
        template<typename ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers,
                               boost::system::error_code& ec)
        {
            std::size_t written = 0;

            // copy buffers into our write buffer
            {
                boost::lock_guard<boost::mutex> lock(m_write_mutex);

                typename ConstBufferSequence::const_iterator it;
                for(it = buffers.begin(); it != buffers.end(); ++it)
                {
                    unsigned char *buf = boost::asio::buffer_cast<unsigned char*>(*it);
                    std::size_t len = boost::asio::buffer_size(*it);
                    m_write_buffer.insert(m_write_buffer.end(), buf, buf + len);
                    written += len;
                }

                m_write_data_available = true;
            }

            // signal whoever may be waiting for data
            m_cond_write.notify_one();

            ec = boost::system::error_code();
            return written;
        }

        void push_read_buffer(const std::string &data)
        {
            // fill buffer
            {
                boost::lock_guard<boost::mutex> lock(m_read_mutex);
                m_read_buffer.insert(m_read_buffer.end(), data.begin(), data.end());
                m_read_data_available = true;
            }

            // signal whoever may be waiting for data
            m_cond_read.notify_one();
        }

        std::string pop_write_buffer()
        {
            boost::unique_lock<boost::mutex> lock(m_write_mutex);

            // block until we have something to send out
            while(!m_write_data_available)
                m_cond_write.wait(lock);

            std::string tmp = m_write_buffer;
            m_write_buffer.clear();
            m_write_data_available = false;
            return tmp;
        }

    private:
        bool m_read_data_available;
        bool m_write_data_available;

        boost::mutex m_read_mutex;
        boost::mutex m_write_mutex;

        boost::condition_variable m_cond_read;
        boost::condition_variable m_cond_write;

        std::string m_read_buffer;
        std::string m_write_buffer;
};

template <typename Service>
class basic_memory_stream
    : public boost::asio::basic_io_object<Service>
{
    public:
        explicit basic_memory_stream(boost::asio::io_service &io_service) : boost::asio::basic_io_object<Service>(io_service) {};

        void pushReadBuffer(const std::string &data)
        {
            this->get_service().push_read_buffer(this->get_implementation(), data);
        }

        std::string popWriteBuffer(void)
        {
            return this->get_service().pop_write_buffer(this->get_implementation());
        }

        template <typename MutableBufferSequence>
        std::size_t read_some(const MutableBufferSequence& buffers,
                              boost::system::error_code& ec)
        {
            return this->get_service().read_some(this->get_implementation(), buffers, ec);
        }

        template <typename MutableBufferSequence, typename ReadHandler>
        void async_read_some(const MutableBufferSequence& buffers,
                             BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
        {
            // If you get an error on the following line it means that your handler does
            // not meet the documented type requirements for a ReadHandler.
            //BOOST_ASIO_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

            this->get_service().async_read_some(this->get_implementation(),
                                                buffers, BOOST_ASIO_MOVE_CAST(ReadHandler)(handler));
        }

        template<typename ConstBufferSequence, typename WriteHandler>
        void async_write_some(const ConstBufferSequence& buffers,
                              BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
        {
            // If you get an error on the following line it means that your handler does
            // not meet the documented type requirements for a WriteHandler.
            //BOOST_ASIO_WRITE_HANDLER_CHECK(WriteHandler, handler) type_check;

            this->get_service().async_write_some(this->get_implementation(),
                                                 buffers, BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
        }
};

template <typename MemoryStreamImplementation = memory_stream_impl>
class memory_stream_service
  : public boost::asio::io_service::service
{
  public:
        /*
         * mandatory io_service API
         */
        static boost::asio::io_service::id id;

        explicit memory_stream_service(boost::asio::io_service &io_service) :
                        boost::asio::io_service::service(io_service),
                        async_work_(new boost::asio::io_service::work(async_io_service_)),
                        async_thread_(boost::bind(&boost::asio::io_service::run, &async_io_service_))
        {
        }

        ~memory_stream_service()
        {
            async_work_.reset();
            async_io_service_.stop();
            async_thread_.join();
        }

        typedef boost::shared_ptr<MemoryStreamImplementation> implementation_type;

        void construct(implementation_type &impl)
        {
            impl.reset(new MemoryStreamImplementation);
        }

        void destroy(implementation_type &impl)
        {
            impl->destroy();
            impl.reset();
        }

        /*
         * sync and async read API
         */
        template <typename MutableBufferSequence>
        std::size_t read_some(implementation_type& impl,
                              const MutableBufferSequence& buffers, boost::system::error_code& ec)
        {
            return impl->read_some(buffers, ec);
        }

        // async read operation functor
        template<typename ReadHandler, typename MutableBufferSequence>
        class read_operation
        {
            public:
                read_operation(implementation_type &impl,
                               boost::asio::io_service &io_service,
                               const MutableBufferSequence& buffers,
                               ReadHandler handler) :
                                impl_(impl),
                                io_service_(io_service),
                                work_(io_service),
                                buffers_(buffers), handler_(handler)
                {
                }

                void operator()() const
                {
                    implementation_type impl = impl_.lock();
                    if (impl)
                    {
                        boost::system::error_code ec;
                        std::size_t transferred = impl->read_some(buffers_, ec);
                        this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec, transferred));
                    }
                    else
                    {
                        this->io_service_.post(boost::asio::detail::bind_handler(handler_,
                                                                                 boost::asio::error::operation_aborted, 0));
                    }
                }

            private:
                boost::weak_ptr<memory_stream_impl> impl_;
                boost::asio::io_service &io_service_;
                boost::asio::io_service::work work_;
                const MutableBufferSequence buffers_;
                ReadHandler handler_;
        };

        template <typename MutableBufferSequence, typename ReadHandler>
        void async_read_some(implementation_type& impl,
                             const MutableBufferSequence& buffers,
                             BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
        {
            this->async_io_service_.post(read_operation<ReadHandler, MutableBufferSequence>(impl, this->get_io_service(), buffers, handler));
        }

        /*
         * sync and async write API
         */
        template<typename ConstBufferSequence>
        std::size_t write_some(implementation_type& impl,
                               const ConstBufferSequence& buffers,
                               boost::system::error_code& ec)
        {
            return impl->write_some(buffers, ec);
        }


        // async write operation functor
        template<typename WriteHandler, typename ConstBufferSequence>
        class write_operation
        {
            public:
                write_operation(implementation_type &impl,
                                boost::asio::io_service &io_service,
                                const ConstBufferSequence& buffers,
                                WriteHandler handler) :
                                impl_(impl),
                                io_service_(io_service),
                                work_(io_service),
                                buffers_(buffers), handler_(handler)
                {
                }

                void operator()() const
                {
                    implementation_type impl = impl_.lock();
                    if (impl)
                    {
                        boost::system::error_code ec;
                        std::size_t transferred = impl->write_some(buffers_, ec);
                        this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec, transferred));
                    }
                    else
                    {
                        this->io_service_.post(boost::asio::detail::bind_handler(handler_,
                                                                                 boost::asio::error::operation_aborted, 0));
                    }
                }

            private:
                boost::weak_ptr<memory_stream_impl> impl_;
                boost::asio::io_service &io_service_;
                boost::asio::io_service::work work_;
                const ConstBufferSequence buffers_;
                WriteHandler handler_;
        };

        template<typename ConstBufferSequence, typename WriteHandler>
        void async_write_some(implementation_type& impl,
                              const ConstBufferSequence& buffers,
                              BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
        {
            this->async_io_service_.post(write_operation<WriteHandler, ConstBufferSequence>(impl, this->get_io_service(), buffers, handler));
        }

        // test api, dummy data source/sink
        void push_read_buffer(implementation_type& impl, const std::string &data)
        {
            impl->push_read_buffer(data);
        }

        std::string pop_write_buffer(implementation_type& impl)
        {
            return impl->pop_write_buffer();
        }

  private:
        void shutdown_service()
        {
        }

        boost::asio::io_service async_io_service_;
        boost::scoped_ptr<boost::asio::io_service::work> async_work_;
        boost::thread async_thread_;

};

template <typename MemoryStreamImplementation>
boost::asio::io_service::id memory_stream_service<MemoryStreamImplementation>::id;

typedef basic_memory_stream<memory_stream_service<memory_stream_impl> > MemoryStream;

#endif /* MEMORYSTREAMSERVICE_H_ */
