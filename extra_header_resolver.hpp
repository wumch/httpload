/**
 * <class gonline::tgw::ExtraHeaderResolver>
 * implements Tencent's TGW(Tencent Gateway) protocol.
 *
 * include (header only, requires boost):
// optional debug level. can be 0,1,2. if not specified, we will deduce a level by check NDEBUG/DEBUG.
#define GOL_DEBUG	2
// optional flag, indicates whether we should be compatible with old-version-protocol or not. default is 1.
#define GOL_OLD_VER_COMPATIBLE	0
// optional, indicates max-length of `extra-header`, default is 200. for optimize performance. default is 200.
#define GOL_EXTRA_HEADER_MAX_LENGTH	200
#include "extra_header_resolver.hpp"

 *
 * compile (requires -lboost_system -pthread, DEBUG=0,1,>=2 supported):
 * g++ -DDEBUG=2 -DGOL_OLD_VER_COMPATIBLE=0 -Wall -g3 -lboost_system -pthread your_program.cpp -o your_program
 *
Semantics (exception-safe guaranteed):

void resolve_extra_header(
	// socket, supports <boost::asio::ip::tcp::socket> obly.
	boost::asio::ip::tcp::socket& socket,

	// socket receive buffer. eg. <char[1024]>.
	AnyType buffer[buffer_capacity],

	// success callback, must be `copy-construct-able`.
	// all of <func-pointer>, <boost::bind(...)>, <member-pointer> are ok.
	// NOTE: pass in as:    <const SuccessCallback&>.
	void (SuccessCallback*) (boost::system::error_code& error),

	// error callback, must be `copy-construct-able`.
	// all of <func-pointer>, <boost::bind(...)>, <member-pointer> are ok.
	// NOTE: pass in as:    <const ErrorCallback&>.
	void (ErrorCallback*) (boost::system::error_code& error, const std::size_t bytes_buffered),

	// optional timer, indicates timeout-waiter.
	// once wait timeout, we will do:
	// 	   socket.cancel();	// NOTE: we will NOT disconnect.
	// 	   delete <ExtraHeaderResolver>;
	boost::asio::deadline_timer& timer
);

// *********************** call usage 1: ***********************
gonline::tgw::resolve_extra_header(
	socket_, data_,
	boost::bind(&session::handle_read, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred),
	boost::bind(&session::on_extrea_header_error, this,
		boost::asio::placeholders::error)
);
// *********************** end call usage 1 *********************** /

// ********** call usage 2: (exits extra-header-resolver after timerout).*************
boost::asio::deadline_timer timer(<boost::asio::io_service>);
timer.expires_from_now(boost::posix_time::millisec(3000));
gonline::tgw::resolve_extra_header(
	socket_, data_,
	boost::bind(&session::handle_read, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred),
	boost::bind(&session::on_extrea_header_error, this,
		boost::asio::placeholders::error),
	timer
);
// *********************** end call usage 2 *********************** /
 *
 * the `extra_header` concept in TGW:
 * 	   extra_header is a http-protocol-like ASCII header which required by Tencent's TGW-server.
 * 	   It must be sent by game-client once connection is built up.
 * 	   Content of extra_header should looks like "GET / HTTP/1.1\r\nHost: app26745-1.qzoneapp.com:8000\r\n\r\n".
 * 	   Usually, the content of extra_header is runtime-constant for each server-process.
 *
 * The purpose of <gonline::tgw::ExtraHeaderResolver> is to
 * ``implements TGW-protocol by only adds one-line-of-code (or fews more), and zero-line-of-code need modification``.
 * So, <gonline::tgw::ExtraHeaderResolver> will receives, authorizes, and then removes extra_header
 * from received data, then provides you with actually available data.
 *
 * The work follow of <gonline::tgw::ExtraHeaderResolver>:
 * start() => receive_header() => auth_header() => SuccessCallback.
 */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <string>
#include <boost/cstdint.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/enable_shared_from_this.hpp>

#if !defined(GOL_DEBUG) && !defined(NDEBUG)
#	if defined(DEBUG)
#		define GOL_DEBUG	DEBUG
#	else
#		define GOL_DEBUG	0
#	endif
#endif

/*
 * flag indicates whether we should be compatible with old-version-protocol or not.
 * optional value:
 * 0 :	incompatible. allows only ``new version``.
 * 1 :	compatible, but optimize for ``new version``. this is default.
 * 2 :	compatible, and optimize for ``old version``.
 */
#ifndef GOL_OLD_VER_COMPATIBLE
#	define GOL_OLD_VER_COMPATIBLE 1
#endif

// flag indicates whether the content of `extra_header` is constant in run-time or not.
#ifndef GOL_EXTRA_HEADER_CONST
#	define GOL_EXTRA_HEADER_CONST 0
#endif

#if !(defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST)
#	include <boost/algorithm/string/find.hpp>
#	include <boost/static_assert.hpp>
#	ifndef GOL_EXTRA_HEADER_MAX_LENGTH
//#		define GOL_EXTRA_HEADER_MAX_LENGTH ULONG_LONG_MAX
#		define GOL_EXTRA_HEADER_MAX_LENGTH 200
#	endif
#endif

#if defined(OC_BLACK) || defined(GOL_OC_RED) || defined(GOL_OC_GREEN) ||	\
	defined(GOL_OUT) || defined(GOL_SAY) || defined(GOL_ERR) ||				\
	defined(GOL_LIKELY) || defined(GOL_UNLIKELY) ||							\
	defined(GOL_MIN) ||														\
	defined(GOL_STRLEN) ||													\
	defined(GOL_EXTRA_HEADER_TAIL) ||										\
	defined(GOL_EXTRA_HEADER_MIN_LENGTH) ||									\
	defined(GOL_VER_IDEC) || defined(GOL_VER_PREFER_OLD)
#	error "macro name GOL_xxx... is taken."
#endif

#define GOL_OC_BLUE(val)	"\033[32;34;5m" << val << "\033[0m"
#define GOL_OC_RED(val)		"\033[32;31;5m" << val << "\033[0m"
#define GOL_OC_GREEN(val)	"\033[32;49;5m" << val << "\033[0m"
#define GOL_STRLEN(s) 	(sizeof(s) / sizeof(char) - 1)
#define GOL_MIN(a,b) 	((a) <= (b) ? (a) : (b))
#define GOL_EXTRA_HEADER_TAIL		"\r\n\r\n"
#define GOL_EXTRA_HEADER_MIN_LENGTH (GOL_STRLEN("GET / HTTP/1.1\r\nHost:x.xx") + GOL_STRLEN(GOL_EXTRA_HEADER_TAIL))
#define GOL_VER_IDEC				"GET "

#if GOL_DEBUG > 1
#define GOL_OUT(ostream, msg) \
	std::cout <<  GOL_OC_BLUE(__FILE__) << ":" << GOL_OC_BLUE(__LINE__) \
	<< ":\t" << msg << std::endl;
#elif GOL_DEBUG
#	define GOL_OUT(ostream, msg)	ostream << msg << std::endl;
#else
#	define GOL_OUT(ostream, msg)
#endif

#define GOL_SAY(msg)	GOL_OUT(std::cout, msg)
#define GOL_ERR(msg) 	GOL_OUT(std::cerr, GOL_OC_RED(msg))
#define GOL_DUMP(var)	GOL_OUT(std::cout, GOL_OC_BLUE(#var) << ": " << GOL_OC_GREEN(var))

#ifdef __GNUC__
#	define GOL_LIKELY(expr)		__builtin_expect((expr), 1)
#	define GOL_UNLIKELY(expr)	__builtin_expect((expr), 0)
#else
#	define GOL_LIKELY(expr) 	(expr)
#	define GOL_UNLIKELY(expr)	(expr)
#endif

#if defined(GOL_OLD_VER_COMPATIBLE) && GOL_OLD_VER_COMPATIBLE
#	if GOL_OLD_VER_COMPATIBLE != 1
#		define GOL_VER_PREFER_OLD GOL_LIKELY
#	else
#		define GOL_VER_PREFER_OLD GOL_UNLIKELY
#	endif
#endif

namespace gonline {
namespace tgw {

namespace bap = ::boost::asio::placeholders;
typedef boost::asio::ip::tcp::socket Sock;
typedef uint16_t	port_t;
typedef std::size_t	buf_size_t;
typedef ssize_t		buf_ssize_t;
typedef char		byte_t;		// typeof(`element of socket stream`)

template<
	typename Buffer, buf_size_t _buffer_capacity,
	typename SuccessCallback, typename ErrorCallback
>
class ExtraHeaderResolver
	: public boost::enable_shared_from_this<ExtraHeaderResolver<Buffer, _buffer_capacity, SuccessCallback, ErrorCallback> >
{
public:
	ExtraHeaderResolver(Sock& _sock, Buffer (&_buffer)[_buffer_capacity],
		const SuccessCallback& _success_callback, const ErrorCallback& _error_callback)
		: sock(_sock), buffer(reinterpret_cast<byte_t*>(_buffer)), bytes_buffered(0),
		  success_callback(_success_callback), error_callback(_error_callback)
	{
	}

	void start()
	{
		receive_header();
	}

	// stop by timeout
	void stop()
	{
		sock.cancel();
//		delete this;
	}

#if GOL_DEBUG
	// test only dtor.
	~ExtraHeaderResolver()
	{
		GOL_SAY("exiting extra-header-resolver...")
	}
#endif

protected:
	/**
	 * wait for `extra_header` from client.
	 * we should call this once connection is built.
	 */
	void receive_header()
	{
		sock.async_read_some(
			boost::asio::buffer(buffer + bytes_buffered, buffer_capacity - bytes_buffered),
			boost::bind(&ExtraHeaderResolver::auth_header, this->shared_from_this(), bap::error, bap::bytes_transferred)
		);
	}

	/**
	 * validate the received `extra_header`,
	 * and then forward to `receive()` if `extra_header` is correct.
	 */
	void auth_header(const boost::system::error_code& error, const buf_size_t bytes_transferred)
	{
		if (GOL_LIKELY(!error))
		{
			bytes_buffered += bytes_transferred;
#if defined(GOL_OLD_VER_COMPATIBLE) && GOL_OLD_VER_COMPATIBLE
			if (GOL_VER_PREFER_OLD(!!std::memcmp(GOL_VER_IDEC, buffer, GOL_STRLEN(GOL_VER_IDEC))))
			{
				if (GOL_LIKELY(bytes_buffered >= GOL_STRLEN(GOL_VER_IDEC)))
				{
					GOL_ERR("received old-version-protocol packet, forwarding to " << GOL_OC_BLUE("success_callback(error, ") << GOL_OC_RED(bytes_buffered) << GOL_OC_BLUE(")"))
					return static_cast<void>(success_callback(error, bytes_buffered));
				}
			}
#endif
#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
			if (GOL_LIKELY(bytes_buffered >= extra_header.size()))	// extra_header is complete
#else
			GOL_DUMP(bytes_buffered)
			GOL_DUMP(extra_header_lpos)
			if (GOL_LIKELY(bytes_buffered >= extra_header_lpos))
#endif
			{
				buf_ssize_t extra_header_len;
				if (GOL_LIKELY((extra_header_len = resolve_extra_header()) >= 0))	// extra_header is correct.
				{
					// we'll remove extra_header from `data_`,
					// so that you can use this `data_` as you used to do.
					GOL_DUMP(bytes_buffered)
					GOL_DUMP(extra_header_len)
					if (GOL_LIKELY(bytes_buffered == static_cast<buf_size_t>(extra_header_len)))		// received bytes are exactly the `extra_header`.
					{
						GOL_SAY(GOL_OC_GREEN("extra-header is exactly matched. forwarding to " << GOL_OC_BLUE("success_callback(error, 0)")))
						return static_cast<void>(success_callback(error, bytes_buffered = 0));
					}
					else	// some additional bytes behind `extra_header`.
					{
						bytes_buffered -= extra_header_len;
						if (GOL_LIKELY(bytes_buffered <= static_cast<buf_size_t>(extra_header_len)))
						{
							std::memcpy(buffer, buffer + extra_header_len, bytes_buffered);
						}
						else
						{
							// these memcpy() calls can be optimized out if your `buffer_` has `offset` supported:
							byte_t tmp_data[bytes_buffered];
							std::memcpy(tmp_data, buffer + extra_header_len, bytes_buffered);
							std::memcpy(buffer, tmp_data, bytes_buffered);
						}
						GOL_SAY(GOL_OC_GREEN("extra-header is correct,  forwarding to " << GOL_OC_BLUE("success_callback(error, ") << GOL_OC_RED(bytes_buffered) << GOL_OC_BLUE(")")))
						return static_cast<void>(success_callback(error, (bytes_buffered)));	// handle additional bytes.
					}
				}
#if !(defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST)
				else if (bytes_buffered < extra_header_rpos)
				{
					GOL_ERR("received incomplete extra-header, cumulative length: " << bytes_buffered << ", continue with receive.")
					return static_cast<void>(receive_header());
				}
#endif
				else	// received extra_header is wrong, just disconnect.
				{
					GOL_ERR("received wrong extra-header, forwarding to error_callback(error).")
					return static_cast<void>(error_callback(error));
				}
			}
			else	// extra_header is incomplete, continue with receive_header().
			{
				GOL_ERR("extra-header is incomplete, cumulative length: " << bytes_buffered << ", continue with receive.")
				return static_cast<void>(receive_header());
			}
		}
		else	// socket error
		{
			GOL_ERR("socket error, forwarding to error_callback(" << error << ")");
			return static_cast<void>(error_callback(error));
		}
	}

	/**
	 * validate `received extra_header`
	 * @return `received extra_header`.length, if it's correct, else -1.
	 */
	__attribute__((always_inline)) buf_ssize_t resolve_extra_header()
	{
#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
		return extra_header.compare(0, extra_header.size(),
			buffer, extra_header.size()) == 0 ? extra_header.size() : -1;
#else
		// in this case, we just find the first position of "\r\n\r\n" in `buffer_`:
		// length of the shortest `extra_header`
//* The first implemention:
		return recheck(
			boost::find_first<const boost::iterator_range<const byte_t*>, const char*>(
				boost::make_iterator_range(buffer + extra_header_lpos, buffer + GOL_MIN(bytes_buffered, extra_header_rpos)),
				GOL_EXTRA_HEADER_TAIL
			).begin() - buffer
		);
// end The first implemention */

/* The second implemention:
		// this is another implemention which does not require boost.
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
		extra_header.reserve(GOL_STRLEN("GET / HTTP/1.1\r\nHost: " ":" GOL_EXTRA_HEADER_TAIL "65535") + domain.size());
		extra_header.append("GET / HTTP/1.1\r\nHost: ");
		extra_header.append(domain);
		extra_header.append(":");
		extra_header.append(boost::lexical_cast<std::string>(port));
		extra_header.append(GOL_EXTRA_HEADER_TAIL);
	}
#endif

#if !(defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST)
	// assist `tpl-expr` to make auth_header() contains no temporary-variables, so that auth_header() can be inlined.
	__attribute__((always_inline)) buf_ssize_t recheck(const buf_size_t pos) const
	{
		return pos < bytes_buffered ? (pos + GOL_STRLEN(GOL_EXTRA_HEADER_TAIL)) : -1;
	}
#endif

protected:
	Sock& sock;

	byte_t* buffer;
	static const std::size_t buffer_capacity = _buffer_capacity;
	BOOST_STATIC_ASSERT(buffer_capacity >= GOL_EXTRA_HEADER_MIN_LENGTH);
	BOOST_STATIC_ASSERT(buffer_capacity > GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
	BOOST_STATIC_ASSERT(buffer_capacity >= GOL_STRLEN(GOL_VER_IDEC));
	buf_size_t bytes_buffered;	// count of elements which are already inside <data_>.

	SuccessCallback	success_callback;
	ErrorCallback		error_callback;

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
	static std::string extra_header;	// content of extra_header.
#else
	static const std::size_t extra_header_lpos = GOL_EXTRA_HEADER_MIN_LENGTH;
	static const std::size_t extra_header_rpos = GOL_MIN(GOL_EXTRA_HEADER_MAX_LENGTH, buffer_capacity - GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
	BOOST_STATIC_ASSERT(extra_header_rpos <= buffer_capacity - GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
	BOOST_STATIC_ASSERT(extra_header_lpos <= extra_header_rpos);
#endif
};	// end class ExtraHeaderResolver

#if defined(GOL_EXTRA_HEADER_CONST) && GOL_EXTRA_HEADER_CONST
template<typename Buffer, buf_size_t _buffer_capacity,
	typename SuccessCallback, typename ErrorCallback>
std::string ExtraHeaderResolver<Buffer, _buffer_capacity,
	SuccessCallback, ErrorCallback>::extra_header;
#endif

template<typename Buffer, buf_size_t buffer_capacity,
	typename SuccessCallback, typename ErrorCallback>
inline ExtraHeaderResolver<Buffer, buffer_capacity,
	SuccessCallback, ErrorCallback>*
make_resolver(
	Sock& sock, Buffer (&buffer)[buffer_capacity],
	const SuccessCallback& scb, const ErrorCallback& ecb)
{
	return new ExtraHeaderResolver<Buffer, buffer_capacity,
		SuccessCallback, ErrorCallback>(sock, buffer, scb, ecb);
}

template<typename Buffer, buf_size_t buffer_capacity,
	typename SuccessCallback, typename ErrorCallback>
inline void resolve_extra_header(
	Sock& sock, Buffer (&buffer)[buffer_capacity],
	const SuccessCallback& scb, const ErrorCallback& ecb)
{
	typedef ExtraHeaderResolver<Buffer, buffer_capacity, SuccessCallback, ErrorCallback> EHR;
	boost::shared_ptr<EHR> ehr(make_resolver(sock, buffer, scb, ecb));
	ehr->start();
}

template<typename Buffer, buf_size_t buffer_capacity,
	typename SuccessCallback, typename ErrorCallback>
inline void resolve_extra_header(
	Sock& sock, Buffer (&buffer)[buffer_capacity],
	const SuccessCallback& scb, const ErrorCallback& ecb,
	boost::asio::deadline_timer& timer)
{
	typedef ExtraHeaderResolver<Buffer, buffer_capacity, SuccessCallback, ErrorCallback> EHR;
	boost::shared_ptr<EHR> ehr(make_resolver(sock, buffer, scb, ecb));
	timer.async_wait(boost::bind(&EHR::stop, ehr));
	ehr->start();
}


}	// end namespace tgw
}	// end namespace gonline
