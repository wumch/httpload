/**
 * sample for implements Tencent's TGW(Tencent Gateway) protocol.
 *
 * usage:
 * g++ -Wall -g3 -pthread -lboost_system echo_server.cpp -o echo_server
 * ./echo_server 8000
 *
 * the `extra_header` concept in TGW:
 * 	   extra_header is a http-protocol-like header which required Tencent's TGW-server.
 * 	   It must be sent by game-client once connection is setup.
 * 	   It should looks like "GET / HTTP/1.1\r\nHost: app26745-1.qzoneapp.com:8000\r\n\r\n".
 * 	   Usually, the content of extra_header is stationary for each server-process.
 *
 * The goal of this sample is to receive, authorize, and then remove extra_header
 * from received data, then provide your game-server with available data.
 *
 * All of majar changes are inside <class session>.
 * The work follow of <session> is:
 * start() => read_header() => auth_header() => receive/send loops.
 */

#include <cstddef>
#include <cstdlib>
#include <string>
#include <iostream>
#include <boost/cstdint.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

// flag indicates whether the content of `extra_header` is constant in run-time or not.
#ifndef GOL_EXTRA_HEADER_CONST
#	define GOL_EXTRA_HEADER_CONST 0
#else
#	error "macro name GOL_EXTRA_HEADER_CONST is taken."
#endif

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
#	include <sstream>
#else
#	include <boost/algorithm/string/find.hpp>
#	include <boost/static_assert.hpp>
#	define EXTRA_HEADER_MAX_LENGTH 500
#endif

#ifndef GOL_STRLEN
#	define GOL_STRLEN(s) (sizeof(s) / sizeof(char) - 1)
#else
#	error "macro name GOL_STRLEN is taken."
#endif

namespace {

namespace bap = ::boost::asio::placeholders;
typedef uint16_t	port_t;
typedef std::size_t	buf_size_t;
typedef ssize_t		buf_ssize_t;
typedef char		byte_t;		// typeof(`element of socket stream`)

template<typename SuccessCallback, typename ErrorCallback>
class ExtraHeaderResolver
{
protected:
	typedef boost::asio::ip::tcp::socket Sock;
	typedef uint16_t	port_t;
	typedef std::size_t	buf_size_t;
	typedef ssize_t		buf_ssize_t;
	typedef char		byte_t;		// typeof(`element of socket stream`)

	Sock& sock;
	SuccessCallback&	success_callback;
	ErrorCallback&		error_callback;

	byte_t* buffer;
	static const std::size_t buffer_cap;
	buf_size_t bytes_buffered;	// count of elements which are already inside <data_>.

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
	static std::string extra_header;	// content of extra_header.
#else
	static const buf_size_t extra_header_lpos = GOL_STRLEN("GET / HTTP/1.1\r\nHost:x.xx");
	static const buf_size_t extra_header_rpos = extra_header_lpos + 500;
	BOOST_STATIC_ASSERT(extra_header_rpos <= buffer_cap - GOL_STRLEN("\r\n\r\n"));
	BOOST_STATIC_ASSERT(extra_header_lpos <= extra_header_rpos);
#endif

public:
	template<typename Buffer, buf_size_t _buffer_capacity>
	ExtraHeaderResolver(Sock& _sock, Buffer& _buffer,
		SuccessCallback _success_callback, ErrorCallback& _error_callback)
	{

	}

};

class session
{
protected:
	boost::asio::ip::tcp::socket sock;

	// this sample requires that buffer_.size() >= extra_header.size()
	static const buf_size_t max_length = 1024;
	byte_t buffer_[max_length];		// socket receive buffer

	buf_size_t bytes_buffered;	// count of elements which are already inside <data_>.

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
	static std::string extra_header;	// content of extra_header.
#else
	static const buf_size_t extra_header_lpos = GOL_STRLEN("GET / HTTP/1.1\r\nHost:x.xx");
	static const buf_size_t extra_header_rpos = extra_header_lpos + 500;
	BOOST_STATIC_ASSERT(extra_header_rpos <= max_length - GOL_STRLEN("\r\n\r\n"));
	BOOST_STATIC_ASSERT(extra_header_lpos <= extra_header_rpos);
#endif

public:
	session(boost::asio::io_service& io_service)
		: sock(io_service), bytes_buffered(0)
	{
	}

	boost::asio::ip::tcp::socket& socket()
	{
		return sock;
	}

	void start()
	{
		receive_header();
	}

	// test only dtor.
	~session()
	{
		std::cout << "disconnecting..." << std::endl;
	}

protected:
	void send(const boost::system::error_code& error, const buf_size_t bytes_transferred)
	{
		std::cout << "received [" << *reinterpret_cast<uint32_t*>(buffer_) << "]" << std::endl;
		if (!error)
		{
			boost::asio::async_write(sock,
			    boost::asio::buffer(buffer_, bytes_transferred),
			    boost::bind(&session::receive, this, bap::error)
			);
		}
		else
		{
			delete this;
		}
	}

	void receive(const boost::system::error_code& error)
	{
		std::cout << "attempt to receive data..." << std::endl;
		if (!error)
		{
			sock.async_read_some(
			    boost::asio::buffer(buffer_, max_length),
			    boost::bind(&session::send, this, bap::error, bap::bytes_transferred)
			);
		}
		else
		{
			delete this;
		}
	}

	/**
	 * wait for `extra_header` from client.
	 * we should call this once connection is built.
	 */
	void receive_header()
	{
		sock.async_read_some(
			boost::asio::buffer(buffer_ + bytes_buffered, max_length - bytes_buffered),
			boost::bind(&session::auth_header, this, bap::error, bap::bytes_transferred)
		);
	}

	/**
	 * validate the received `extra_header`,
	 * and then forward to `receive()` if `extra_header` is correct.
	 */
	void auth_header(const boost::system::error_code& error, const buf_size_t bytes_transferred)
	{
		if (!error)
		{
			bytes_buffered += bytes_transferred;
#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
			if (bytes_buffered >= extra_header.size())	// extra_header is complete
#else
			if (bytes_buffered >= extra_header_lpos)
#endif
			{
				buf_ssize_t extra_header_len;
				if ((extra_header_len = resolve_extra_header()) >= 0)	// extra_header is correct.
				{
					// we'll remove extra_header from `data_`,
					// so that you can use this `data_` as you used to do.
					if (bytes_buffered == static_cast<buf_size_t>(extra_header_len))		// received bytes are exactly the `extra_header`.
					{
						bytes_buffered = 0;
						receive(error);
					}
					else	// some additional bytes behind `extra_header`.
					{
						bytes_buffered -= extra_header_len;
						if (bytes_buffered <= static_cast<buf_size_t>(extra_header_len))
						{
							std::memcpy(buffer_, buffer_ + extra_header_len, bytes_buffered);
						}
						else
						{
							// these memcpy() calls can be optimized out if your `buffer_` has `offset` supported:
							byte_t tmp_data[max_length];
							std::memcpy(tmp_data, buffer_ + extra_header_len, bytes_buffered);
							std::memcpy(buffer_, tmp_data, bytes_buffered);
						}
						std::cout << "header is correct, " << bytes_buffered << " bytes remains in buffer..." << std::endl;
						send(error, bytes_buffered);	// handle additional bytes.
					}
				}
				else	// received extra_header is wrong, just disconnect.
				{
					std::cerr << "received wrong extra_header, disconnecting." << std::endl;
					delete this;
				}
			}
			else	// extra_header is incomplete, continue with receive_header().
			{
				std::cerr << "received incomplete header, cumulative length: "
					<< bytes_buffered << ", continue to receive." << std::endl;
				receive_header();
			}
		}
		else	// socket error
		{
			std::cerr << "socket error, disconnecting." << std::endl;
			delete this;
		}
	}

	/**
	 * validate `received extra_header`
	 * @return `received extra_header`.length, if it's correct, else -1.
	 */
	__attribute__((always_inline)) buf_ssize_t resolve_extra_header() const
	{
#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
		return extra_header.compare(0, extra_header.size(),
			buffer_, extra_header.size()) == 0 ? extra_header.size() : -1;
#else
		// in this case, we just find the first position of "\r\n\r\n" in `buffer_`:
		// length of the shortest `extra_header`
//* The first implemention:
		return recheck(
			boost::find_first<const boost::iterator_range<const byte_t*>, const char*>(
				boost::make_iterator_range(buffer_ + extra_header_lpos, buffer_ + std::min(bytes_buffered, extra_header_rpos)),
				"\r\n\r\n"
			).begin() - buffer_
		);
// end The first implemention */

/* The second implemention:
		// this is another implemention which not requires boost.
#	ifndef GOL_UNLIKELY
#		ifdef __GNUC__
#			define GOL_LIKELY(expr)		__builtin_expect((expr), 1)
#			define GOL_UNLIKELY(expr)	__builtin_expect((expr), 0)
#		else
#			define GOL_LIKELY(expr) 	(expr)
#			define GOL_UNLIKELY(expr)	(expr)
#		endif
#	endif
		for (buf_size_t i = extra_header_lpos, end = std::min(bytes_buffered, extra_header_rpos);
			i < end; ++i)
		{
			if (GOL_UNLIKELY(buffer_[i] == '\r'))
			{
				if (GOL_LIKELY(buffer_[i + 1] == '\n' && buffer_[i + 2] == '\r' && buffer_[i + 3] == '\n'))
				{
					return i + 4;
				}
			}
		}
		return -1;
// The second implemention */
#endif
	}

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
	static void reset_extra_header(const std::string& domain, const port_t port)
	{
		extra_header.clear();
		// string(port).size() <= 5
		extra_header.reserve((GOL_STRLEN("GET / HTTP/1.1\r\nHost: " ":" "\r\n\r\n") + 5) + domain.size());
		extra_header.append("GET / HTTP/1.1\r\nHost: ");
		extra_header.append(domain);
		extra_header.append(":");
		extra_header.append(boost::lexical_cast<std::string>(port));
		extra_header.append("\r\n\r\n");
	}
#endif

#if !(defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST)
	// to make auth_header() contains no temporary-variables, so that auth_header() can be inlined.
	__attribute__((always_inline)) buf_ssize_t recheck(const buf_size_t pos) const
	{
		return pos > max_length ? -1 : (pos + GOL_STRLEN("\r\n\r\n"));
	}
#endif
};	// end class session

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
std::string session::extra_header;
#endif

// <server>, for accepts in-coming connections, and constructs <session>s.
class server
{
private:
	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;

public:
	// NOTE: TGW requires that we must bind to 0.0.0.0, specified interface is not allowed.
	server(boost::asio::io_service& io_service, const port_t port)
		: io_service_(io_service),
		  acceptor_(io_service,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
	{
		session* new_session = new session(io_service_);
		acceptor_.async_accept(
		    new_session->socket(),
		    boost::bind(&server::handle_accept, this, new_session, bap::error)
		);
	}

	void handle_accept(session* new_session,
	    const boost::system::error_code& error)
	{
		if (!error)
		{
			new_session->start();
			new_session = new session(io_service_);
			acceptor_.async_accept(
			    new_session->socket(),
			    boost::bind(&server::handle_accept, this, new_session, bap::error)
			);
		}
		else
		{
			delete new_session;
		}
	}
};	// end class server

}	// end namespace

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
			return 1;
		}

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
		session::reset_extra_header("app26745-4.qzoneapp.com", boost::lexical_cast<port_t>(argv[1]));
#endif

		boost::asio::io_service io_service;
		server s(io_service, boost::lexical_cast<port_t>(argv[1]));

		io_service.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
