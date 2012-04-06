//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

/* Usage:

g++ -Wall -g3 -lboost_system -pthread async_tcp_echo_server.cpp -o async_tcp_echo_server
./async_tcp_echo_server 8000

///// and then test it with:
./echo_client localhost 8000

*/

#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

// **************** import <gonline::tgw::ExtraHeaderResolver>: ****************

// optional debug level. can be 0,1,2. if not specified, we will deduce a level by check NDEBUG/DEBUG.
#define GOL_DEBUG	2		// set debug-level to 2.

/*
 * optional flag, indicates whether we should be compatible with old-version-protocol or not.
 * optional value:
 * 0 :	incompatible. allows only ``new version``.
 * 1 :	compatible, but optimize for ``new version``. this is default.
 * 2 :	compatible, and optimize for ``old version``.
 */
#define GOL_OLD_VER_COMPATIBLE	1		// make old-version-protocol compatible.

// optional, indicates max-length of `extra-header`, default is 100. for optimize performance. default is 200.
#define GOL_EXTRA_HEADER_MAX_LENGTH	80		// tell <gonline::tgw::ExtraHeaderResolver> that your `extra-header` is not longer than 100 bytes.

#include "extra_header_resolver.hpp"

// ************** end import <gonline::tgw::ExtraHeaderResolver> ***************

using boost::asio::ip::tcp;

class session
{
public:
	session(boost::asio::io_service& io_service)
		: socket_(io_service), timer(io_service)
	{
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		/*
		 * instead of immediately enter  receive()/send()  loops,
		 * we firstly call gonline::tgw::resolve_extra_header() here.
		 */
		//* usage 1:
		gonline::tgw::resolve_extra_header(
			socket_, data_,
			// success-callback:
		    boost::bind(&session::handle_read, this,
		        boost::asio::placeholders::error,
		        boost::asio::placeholders::bytes_transferred),
		    // error-callback:
			boost::bind(&session::on_extrea_header_error, this,
				boost::asio::placeholders::error)
			// optional timer:
			// ,timer
		);
		// end usage 1 */

		/* usage 2: (call on_extrea_header_error after timerout).
		timer.expires_from_now(boost::posix_time::millisec(800));

		gonline::tgw::resolve_extra_header(
			socket_, data_,
			boost::bind(&session::handle_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred),
			boost::bind(&session::on_extrea_header_error, this,
				boost::asio::placeholders::error),
			timer
		);
		// end usage 2 */
	}

	void on_extrea_header_error(const boost::system::error_code& error)
	{
		GOL_ERR(GOL_OC_BLUE(__FUNCTION__) << " called, error: " << error)
		socket_.cancel();
		delete this;
	}

	void handle_read(const boost::system::error_code& error,
	    size_t bytes_transferred)
	{
		GOL_SAY(GOL_OC_BLUE(__FUNCTION__) << " called, error: " << error)
		if (!error)
		{
			boost::asio::async_write(
			    socket_,
			    boost::asio::buffer(data_, bytes_transferred),
			    boost::bind(&session::handle_write, this,
			        boost::asio::placeholders::error));
		}
		else
		{
			delete this;
		}
	}

	void handle_write(const boost::system::error_code& error)
	{
		if (!error)
		{
			socket_.async_read_some(
			    boost::asio::buffer(data_, max_length),
			    boost::bind(&session::handle_read, this,
			        boost::asio::placeholders::error,
			        boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			delete this;
		}
	}

private:
	tcp::socket socket_;
	boost::asio::deadline_timer timer;
	static const std::size_t max_length = 1024;
	char data_[max_length];
};

class server
{
public:
	server(boost::asio::io_service& io_service, short port)
		: io_service_(io_service), acceptor_(io_service,
		    tcp::endpoint(tcp::v4(), port))
	{
		session* new_session = new session(io_service_);
		acceptor_.async_accept(
		    new_session->socket(),
		    boost::bind(&server::handle_accept, this, new_session,
		        boost::asio::placeholders::error));
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
			    boost::bind(&server::handle_accept, this, new_session,
			        boost::asio::placeholders::error));
		}
		else
		{
			delete new_session;
		}
	}

private:
	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return 1;
		}

		boost::asio::io_service io_service;

		using namespace std;
		// For atoi.
		server s(io_service, atoi(argv[1]));

		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
