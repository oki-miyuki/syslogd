#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <fstream>
#include <iomanip>

class syslog_daemon {
public:
  syslog_daemon(
    boost::asio::io_service& io_service,
	  short port = 514
  ) :
    io_service_(io_service),
    socket_(
      io_service, 
      boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)
    )
  {
		logtime_	=	boost::posix_time::second_clock::local_time();
    invoke_recieve();
  }

	bool run() {
		if( listener_.get() )	return true;
		try {
			listener_.reset( 
				new boost::thread( boost::bind( &syslog_daemon::run_listen, this ) )
			);
		} catch( ... ) {
			return false;
		}
		return true;
	}

	void stop() {
		if( !listener_.get() )	return;
		io_service_.stop();
		listener_->join();
		listener_.reset();
	}

private:
	void run_listen() {
		io_service_.run();
	}
	void handle_receive_from(
    const boost::system::error_code& error,
    size_t bytes_recvd
  ) {
    if( error ) {
    } else {
      if( bytes_recvd > 0 ) {
        std::string log;
        log.assign( &data_[0], &data_[bytes_recvd] );
				if( open_log() ) {
					logfs_ << log.c_str() << "\r\n";
				}
				
      }
      invoke_recieve();
    }
  }
  void invoke_recieve() {
    socket_.async_receive_from(
      boost::asio::buffer( data_, buffer_length ),
      sender_endpoint_,
      boost::bind(
        &syslog_daemon::handle_receive_from,
        this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred
      )
    );
  }
	bool open_log() {
		if( logfs_.is_open() ) {
		  boost::posix_time::ptime ptime = boost::posix_time::second_clock::local_time();
			if( logtime_.date() != ptime.date() ) {
				logfs_.close();
				logfs_.clear();
				logtime_	=	ptime;
			} else {
				return true;
			}
		}
		try {
			std::string logname = to_iso_extended_string( logtime_.date() );
			logname += ".log";
			logfs_.open( logname.c_str(), std::ios::binary | std::ios::app );
		} catch(...) {
			return false;
		}
		return true;
	}

private:
  enum { buffer_length = 4096, };
  char data_[ buffer_length ];
  boost::asio::io_service&        io_service_;
  boost::asio::ip::udp::socket    socket_;
  boost::asio::ip::udp::endpoint  sender_endpoint_;
	boost::posix_time::ptime				logtime_;
	std::ofstream										logfs_;
	std::auto_ptr<boost::thread>		listener_;
};

