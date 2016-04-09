// syslog send sample
#include <string>
#include <boost/asio.hpp>
#include <boost/date_time/date_facet.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

//! syslog facility code
enum syslog_facility {
  kernel_messages                           =  0,
  user_level_messages                       =  1,
  mail_system_messages                      =  2,
  system_daemons                            =  3,
  security_authorization_messages           =  4,
  messages_generated_internally_by_syslogd  =  5,
  line_printer_subsystem                    =  6,
  network_news_subsystem                    =  7,
  subsystem                                 =  8,
  clock_daemon                              =  9,
  security_authorization_messages2          =  10,
  ftp_daemon                                =  11,
  ntp_daemon                                =  12,
  log_audit                                 =  13,
  log_alert                                 =  14,
  clock_daemon2                             =  15,
  local_use0                                =  16,
  local_use1                                =  17,
  local_use2                                =  18,
  local_use3                                =  19,
  local_use4                                =  20,
  local_use5                                =  21,
  local_use6                                =  22,
  local_use7                                =  23,
};

//! syslog severity code
enum syslog_severity {
  emergency        =  0,  // system is unusable
  alert            =  1,  // action must be taken immediately
  critical         =  2,  // critical conditions
  error            =  3,  // error conditions
  warning          =  4,  // warning conditions
  notice           =  5,  // normal but significant condition
  information      =  6,  // informational messages
  debug            =  7,  // debug-level messages
};

//! get syslog PRI part
/*!
@retval syslog PRI part string.
*/
std::string get_pri_part(
  syslog_facility facility,  //!< [in] facility code
  syslog_severity severity   //!< [in] severity code
) {
  std::ostringstream  oss;
  oss << "<" << (facility * 8 + severity) << ">";
  return oss.str();
}

//! get syslog HEAD part
/*!
@retval syslog HEAD part
*/
std::string get_head_part(
  const boost::posix_time::ptime& ptime,
  const boost::asio::ip::address_v4& address
) {
typedef boost::date_time::time_facet<boost::posix_time::ptime, char> ldt_facet;
  ldt_facet* facet = new ldt_facet("%b %d %H:%M:%S");
  std::ostringstream  oss;
  oss.imbue( std::locale( std::locale::classic(), facet ) );
  oss << ptime << " " << address << " ";
  return oss.str();
}

//! get syslog MSG part
/*!
@attention
@retval syslog MSG part
*/
std::string get_msg_part(
  const std::string& tag,  //!< [in] tag
  const std::string& msg  //!< [in] message
) {
  BOOST_ASSERT( tag.size() <= 32 );
  std::ostringstream  oss;
  oss << tag << ":" << msg;
  return oss.str();
}

class syslog_transfer {
public:
  syslog_transfer(
    boost::asio::io_service& io_service ) :
    io_service_(io_service),
    resolver_(io_service),
    socket_(
      io_service,
      boost::asio::ip::udp::endpoint( boost::asio::ip::udp::v4(), 0)
    )
  {
  }

void connect( const std::string& host_name, const std::string& port ) {
  boost::asio::ip::udp::resolver::query  query( boost::asio::ip::udp::v4(), host_name.c_str(), port.c_str() );
  iter_ = resolver_.resolve( query );
}

void transfer( const std::string& log ) {
  boost::asio::ip::udp::resolver::iterator iter = iter_;
  socket_.send_to( boost::asio::buffer( log, log.size() ), *iter );
}

private:
boost::asio::io_service&                  io_service_;
boost::asio::ip::udp::resolver            resolver_;
boost::asio::ip::udp::resolver::iterator  iter_;
boost::asio::ip::udp::socket              socket_;
};

#include <sstream>

void DebugLog( const char* pszMessage) {
	WSAData	wsaData;
	SOCKET sock;
	struct sockaddr_in addr;
	int res = WSAStartup( MAKEWORD(2,0), &wsaData);
	if( !res ) {
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if( sock != INVALID_SOCKET ) {
			addr.sin_family = AF_INET;
			addr.sin_port = htons(514);
			addr.sin_addr.S_un.S_addr = inet_addr("10.3.1.8");
			sendto(sock, pszMessage, strlen(pszMessage), 0, (struct sockaddr *)&addr, sizeof(addr));
			closesocket(sock);
		}
		WSACleanup();
	} else {
	  std::cout << res << std::endl;
	  std::cout << GetLastError() << std::endl;
	}
}

int main() {
  DebugLog( "hiho" );
  std::ostringstream oss;
  boost::posix_time::ptime ptime = boost::posix_time::second_clock::local_time();
  boost::asio::ip::address_v4  addr = boost::asio::ip::address_v4::from_string( "192.168.1.1" );
  oss << get_pri_part( log_audit, notice ) << get_head_part( ptime, addr )
    << get_msg_part( "tag", "message" );
  boost::asio::io_service  io_service;
  syslog_transfer  st( io_service );
  try {
    st.connect( "localhost", "514" );
    st.transfer( oss.str() );
  } catch( std::exception& e ) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}