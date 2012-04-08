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
Semantics (exception-safe guaranteed. And it's thread-safe, unless your <socket>/buffer is not thread-safe.):

void resolve_extra_header(
	// socket, supports <boost::asio::ip::tcp::socket> obly.
	boost::asio::ip::tcp::socket& socket,

	// socket receive buffer. eg. <char[1024]>.
	AnyType buffer[buffer_capacity],

	// success callback, must be `copy-construct-able`.
	// all of <func-pointer>, <boost::bind(...)>, <member-pointer> are ok.
	// NOTE: pass in as:    <const SuccessCb&>.
	void (SuccessCb*) (boost::system::error_code& error),

	// error callback, must be `copy-construct-able`.
	// all of <func-pointer>, <boost::bind(...)>, <member-pointer> are ok.
	// NOTE: pass in as:    <const ErrorCb&>.
	void (ErrorCb*) (boost::system::error_code& error, const std::size_t bytes_buffered),

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
 * start() => receive_header() => auth_header() => SuccessCb.
 */

#pragma once

#include <cstddef>
//#include <cstdlib>
#include <string>
#include <boost/cstdint.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/static_assert.hpp>
#include <boost/enable_shared_from_this.hpp>

#ifndef GOL_DEBUG
#	if defined(DEBUG) && !defined(NDEBUG)
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

#if GOL_EXTRA_HEADER_CONST
#	include <boost/lexical_cast.hpp>
#	ifndef GOL_EXTRA_HEADER_VALIDATE
#		define GOL_EXTRA_HEADER_VALIDATE 0
#	endif
#else
#	include <boost/algorithm/string/find.hpp>
#	ifndef GOL_EXTRA_HEADER_MAX_LENGTH
#		define GOL_EXTRA_HEADER_MAX_LENGTH 80
#	endif
#endif

#if defined(GOL_OC_BLACK) || defined(GOL_OC_RED) || defined(GOL_OC_GREEN) ||	\
	defined(GOL_OUT) || defined(GOL_SAY) || defined(GOL_ERR) ||					\
	defined(GOL_BLIKELY) || defined(GOL_BUNLIKELY) || defined(GOL_DEPRECATED) ||\
	defined(GOL_STRLEN) ||														\
	defined(GOL_EXTRA_HEADER_TAIL) ||											\
	defined(GOL_EXTRA_HEADER_MIN_LENGTH) ||										\
	defined(GOL_VER_IDEC) || defined(GOL_VER_IDEC_LENGTH) ||					\
	defined(GOL_VER_PREFER_OLD)
#	error "macro name GOL_xxx... is taken."
#endif

#define GOL_OC_BLUE(val)	"\033[32;34;5m" << val << "\033[0m"
#define GOL_OC_RED(val)		"\033[32;31;5m" << val << "\033[0m"
#define GOL_OC_GREEN(val)	"\033[32;49;5m" << val << "\033[0m"
#define GOL_STRLEN(s) 	(sizeof(s) / sizeof(char) - 1)
#define GOL_MIN(a, b) ((a) <= (b) ? (a) : (b))

#define GOL_EXTRA_HEADER_TAIL		"\r\n\r\n"
#if !GOL_EXTRA_HEADER_CONST
#	define GOL_EXTRA_HEADER_MIN_LENGTH	(GOL_STRLEN("GET / HTTP/1.1\r\nHost:x.xx") + GOL_STRLEN(GOL_EXTRA_HEADER_TAIL))
#endif

#if GOL_OLD_VER_COMPATIBLE
#	define GOL_VER_IDEC					"GET "
#	define GOL_VER_IDEC_LENGTH			4
BOOST_STATIC_ASSERT(GOL_STRLEN(GOL_VER_IDEC) == GOL_VER_IDEC_LENGTH);
#endif

#if GOL_DEBUG
#	include <iostream>
#	if GOL_DEBUG > 1
#		define GOL_OUT(ostream, ...) \
			std::cout <<  GOL_OC_BLUE(__FILE__) << ":" << GOL_OC_BLUE(__LINE__) \
			<< ":\t" << __VA_ARGS__ << std::endl;
#	else
#		define GOL_OUT(ostream, ...)	ostream << __VA_ARGS__ << std::endl;
#	endif
#else
#	define GOL_OUT(ostream, ...)
#endif

#define GOL_SAY(...)	GOL_OUT(std::cout, __VA_ARGS__)
#define GOL_ERR(...) 	GOL_OUT(std::cerr, GOL_OC_RED(__VA_ARGS__))
#define GOL_DUMP(...)	GOL_OUT(std::cout, GOL_OC_BLUE(#__VA_ARGS__) << ": " << GOL_OC_GREEN(__VA_ARGS__))

#ifdef __GNUC__
#	define GOL_BLIKELY(expr)		__builtin_expect((expr), 1)
#	define GOL_BUNLIKELY(expr)		__builtin_expect((expr), 0)
#	define GOL_ALWAYS_INLINE		__attribute__((always_inline))
#	define GOL_DEPRECATED			__attribute__((deprecated))
#	define GOL_CEIL_DIV(val, base)	static_cast<typeof(val)>((val + base - 1) / base)
#else
#	define GOL_BLIKELY(expr) 	(expr)
#	define GOL_BUNLIKELY(expr)	(expr)
#	define GOL_ALWAYS_INLINE	inline
#	define GOL_DEPRECATED
#	define GOL_CEIL_DIV(val, base)	static_cast<std::size_t>((val + base - 1) / base)
#endif

#if GOL_OLD_VER_COMPATIBLE == 1		// prefer new-version to old-version.
#	define GOL_VER_PREFER_OLD GOL_BUNLIKELY
#elif GOL_OLD_VER_COMPATIBLE == 2
#	define GOL_VER_PREFER_OLD GOL_BLIKELY
#endif

namespace gonline {
namespace tgw {

namespace bap = ::boost::asio::placeholders;
typedef boost::asio::ip::tcp::socket Sock;
typedef uint16_t	port_t;
typedef std::size_t	buf_size_t;
typedef ssize_t		buf_ssize_t;
typedef char		byte_t;		// typeof(`element of socket stream`)

/**
 * <class ExtraHeaderResolver>
 * copy, copy/move-construct, [move]operator= are all available.
 */
template<typename BufElem, buf_size_t _buffer_capacity,
	typename SuccessCb, typename ErrorCb>
class ExtraHeaderResolver
	: public boost::enable_shared_from_this<ExtraHeaderResolver<BufElem,
	  	  _buffer_capacity, SuccessCb, ErrorCb> >
{
public:
	typedef ExtraHeaderResolver<BufElem, _buffer_capacity, SuccessCb, ErrorCb> type;
	typedef BufElem Buffer[_buffer_capacity];

	ExtraHeaderResolver(Sock& _sock, BufElem (&_buffer)[_buffer_capacity],
		const SuccessCb& _success_cb, const ErrorCb& _error_cb)
		: sock(_sock), buffer(reinterpret_cast<byte_t*>(_buffer)), bytes_buffered(0),
		  success_cb(_success_cb), error_cb(_error_cb)
	{
		GOL_SAY("resolving extra-header for client " << GOL_OC_GREEN(sock.remote_endpoint().address()))
	}

	void start()
	{
		receive_header();
	}

	// stop by timeout
	void stop()
	{
		GOL_SAY("calling " << GOL_OC_BLUE("<gonline::tgw::ExtraHeaderResolver>::" << __FUNCTION__))
		sock.cancel();
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
		if (GOL_BLIKELY(!error))
		{
			bytes_buffered += bytes_transferred;
#if GOL_OLD_VER_COMPATIBLE
			if (GOL_VER_PREFER_OLD(is_old_version()))
			{
				if (GOL_BLIKELY(bytes_buffered >= GOL_STRLEN(GOL_VER_IDEC)))
				{
					GOL_ERR("received old-version-protocol packet, forwarding to "
						<< GOL_OC_BLUE("success_cb(error, ")
						<< GOL_OC_RED(bytes_buffered) << GOL_OC_BLUE(")"))
					return static_cast<void>(success_cb(error, bytes_buffered));
				}
			}
#endif

#if GOL_EXTRA_HEADER_CONST
			if (GOL_BLIKELY(bytes_buffered >= extra_header.size()))	// extra_header is complete
#else
			GOL_DUMP(bytes_buffered)
			GOL_DUMP(extra_header_lpos)
			if (GOL_BLIKELY(bytes_buffered >= extra_header_lpos))
#endif
			{
				buf_ssize_t extra_header_len;
				if (GOL_BLIKELY((extra_header_len = resolve_extra_header()) >= 0))	// extra_header is correct.
				{
					// we'll remove extra_header from `data_`,
					// so that you can use this `data_` as you used to do.
					GOL_DUMP(bytes_buffered)
					GOL_DUMP(extra_header_len)
					// received bytes are exactly the `extra_header`.
					if (GOL_BLIKELY(bytes_buffered == static_cast<buf_size_t>(extra_header_len)))
					{
						GOL_SAY(GOL_OC_GREEN("extra-header is exactly matched. forwarding to "
							<< GOL_OC_BLUE("success_cb(error, 0)")))
						return static_cast<void>(success_cb(error, bytes_buffered = 0));
					}
					else	// some additional bytes behind `extra_header`.
					{
						bytes_buffered -= extra_header_len;
						if (GOL_BLIKELY(bytes_buffered <= static_cast<buf_size_t>(extra_header_len)))
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
						GOL_SAY(GOL_OC_GREEN("extra-header is correct, forwarding to "
							<< GOL_OC_BLUE("success_cb(error, ") << GOL_OC_RED(bytes_buffered) << GOL_OC_BLUE(")")))
						return static_cast<void>(success_cb(error, bytes_buffered));	// handle additional bytes.
					}
				}
#if !GOL_EXTRA_HEADER_CONST
				else if (bytes_buffered < extra_header_rpos)
				{
					GOL_ERR("received incomplete extra-header, cumulative length: "
						<< bytes_buffered << ", continue with receive.")
					return static_cast<void>(receive_header());
				}
#endif
				else	// received extra_header is wrong, just disconnect.
				{
					GOL_ERR("received wrong extra-header, forwarding to "
						<< GOL_OC_BLUE("error_cb(" << GOL_OC_RED(error) << ")"));
					return static_cast<void>(error_cb(error));
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
			GOL_ERR("socket error occured: " << error.message() << ", forwarding to "
				<< GOL_OC_BLUE("error_cb(") << GOL_OC_RED(error) << GOL_OC_BLUE(")"));
			return static_cast<void>(error_cb(error));
		}
	}

#if GOL_OLD_VER_COMPATIBLE
	GOL_ALWAYS_INLINE bool is_old_version()
	{
#	if GOL_VER_IDEC_LENGTH == 4
		return *reinterpret_cast<int32_t*>(buffer) != *reinterpret_cast<const int32_t*>(GOL_VER_IDEC);
#	else
		return std::memcmp(GOL_VER_IDEC, buffer, GOL_STRLEN(GOL_VER_IDEC));
#	endif
	}
#endif	// GOL_OLD_VER_COMPATIBLE

	/**
	 * validate `received extra_header`
	 * @return `received extra_header`.length, if it's correct, else -1.
	 */
	GOL_ALWAYS_INLINE buf_ssize_t resolve_extra_header() const
	{
#if GOL_EXTRA_HEADER_CONST
#	if GOL_EXTRA_HEADER_VALIDATE == 1
		return GOL_BLIKELY(*reinterpret_cast<int32_t*>(buffer +
				(extra_header.size() - GOL_STRLEN(GOL_EXTRA_HEADER_TAIL))) ==
			*reinterpret_cast<const int32_t*>(GOL_EXTRA_HEADER_TAIL)) ?
			extra_header.size() : -1;
#	elif GOL_EXTRA_HEADER_VALIDATE == 2
		return GOL_BLIKELY(extra_header.compare(0, extra_header.size(),
			buffer, extra_header.size()) == 0) ?
			extra_header.size() : -1;
#	else
		return extra_header.size();
#	endif
#else
		// in this case, we just find the first position of "\r\n\r\n" in `buffer_`:
		// length of the shortest `extra_header`
//* The first implemention:
		return recheck(
			boost::find_first<const boost::iterator_range<const byte_t*>, const char*>(
				boost::make_iterator_range(
					buffer + extra_header_lpos,
					buffer + std::min(bytes_buffered, static_cast<const buf_size_t>(extra_header_rpos))
				),
				GOL_EXTRA_HEADER_TAIL
			).begin() - buffer
		);
// end The first implemention */

/* The second implemention:
		// this is another implemention which does not require boost.
		BOOST_STATIC_ASSERT(sizeof(int32_t) == GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
		for (buf_size_t i = extra_header_lpos,
			end = std::min(bytes_buffered - sizeof(int32_t),
				static_cast<const buf_size_t>(extra_header_rpos));
			i < end; ++i)
		{
			if (GOL_BUNLIKELY(buffer[i] == '\r'))
			{
				// buffer out-of-index is prevented by `end`.
				if (GOL_BLIKELY(*reinterpret_cast<int32_t*>(buffer + i) ==
					*reinterpret_cast<const int32_t*>(GOL_EXTRA_HEADER_TAIL)))
				{
					return i + GOL_STRLEN(GOL_EXTRA_HEADER_TAIL);
				}
			}
		}
		return -1;
// The second implemention */
#endif
	}

#if GOL_EXTRA_HEADER_CONST
	static void reset_extra_header(const std::string& domain, const port_t port)
	{
		extra_header.clear();
		extra_header.reserve(GOL_STRLEN("GET / HTTP/1.1\r\nHost: " ":" GOL_EXTRA_HEADER_TAIL "65535") + domain.size());
		extra_header.append("GET / HTTP/1.1\r\nHost: ");
		extra_header.append(domain);
		extra_header.append(":");
		extra_header.append(boost::lexical_cast<std::string>(port));
		extra_header.append(GOL_EXTRA_HEADER_TAIL);
		assert(extra_header.size() <= buffer_capacity);
	}
#endif

#if !GOL_EXTRA_HEADER_CONST
	// assist `tpl-expr` to make auth_header() contains no temporary-variables, so that auth_header() can be inlined.
	GOL_ALWAYS_INLINE buf_ssize_t recheck(const buf_size_t pos) const
	{
		return pos < bytes_buffered ? (pos + GOL_STRLEN(GOL_EXTRA_HEADER_TAIL)) : -1;
	}
#endif

protected:
	Sock& sock;
	byte_t* buffer;		// member-wise copyable
	buf_size_t bytes_buffered;	// count of elements which are already inside <data_>.

	SuccessCb		success_cb;
	ErrorCb		error_cb;

public:
	static const buf_size_t buffer_capacity = GOL_CEIL_DIV(_buffer_capacity * sizeof(BufElem), sizeof(byte_t));
	BOOST_STATIC_ASSERT(buffer_capacity >= GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));

#if GOL_EXTRA_HEADER_CONST
	static std::string extra_header;	// content of extra_header.
#else
	BOOST_STATIC_ASSERT(buffer_capacity >= GOL_EXTRA_HEADER_MIN_LENGTH);
	BOOST_STATIC_ASSERT(buffer_capacity >= GOL_STRLEN(GOL_VER_IDEC));
	static const buf_size_t extra_header_lpos = GOL_EXTRA_HEADER_MIN_LENGTH;
	static const buf_size_t extra_header_rpos = GOL_MIN(GOL_EXTRA_HEADER_MAX_LENGTH, buffer_capacity - GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
	BOOST_STATIC_ASSERT(extra_header_rpos <= buffer_capacity - GOL_STRLEN(GOL_EXTRA_HEADER_TAIL));
	BOOST_STATIC_ASSERT(extra_header_lpos <= extra_header_rpos);
#endif
};	// end class ExtraHeaderResolver

#if GOL_EXTRA_HEADER_CONST
template<typename BufElem, buf_size_t _buffer_capacity,
	typename SuccessCb, typename ErrorCb>
std::string ExtraHeaderResolver<BufElem, _buffer_capacity,
	SuccessCb, ErrorCb>::extra_header;
#endif

template<typename BufElem, buf_size_t buffer_capacity,
	typename SuccessCb, typename ErrorCb>
GOL_ALWAYS_INLINE ExtraHeaderResolver<BufElem, buffer_capacity,
	SuccessCb, ErrorCb>*
make_resolver(
	Sock& sock, BufElem (&buffer)[buffer_capacity],
	const SuccessCb& scb, const ErrorCb& ecb)
{
	return new ExtraHeaderResolver<BufElem, buffer_capacity,
		SuccessCb, ErrorCb>(sock, buffer, scb, ecb);
}

template<typename BufElem, buf_size_t buffer_capacity,
	typename SuccessCb, typename ErrorCb>
inline void resolve_extra_header(
	Sock& sock, BufElem (&buffer)[buffer_capacity],
	const SuccessCb& scb, const ErrorCb& ecb)
{
	typedef ExtraHeaderResolver<BufElem, buffer_capacity, SuccessCb, ErrorCb> EHR;
	boost::shared_ptr<EHR> ehr(make_resolver(sock, buffer, scb, ecb));
	ehr->start();
}

template<typename BufElem, buf_size_t buffer_capacity,
	typename SuccessCb, typename ErrorCb>
inline void resolve_extra_header(
	Sock& sock, BufElem (&buffer)[buffer_capacity],
	const SuccessCb& scb, const ErrorCb& ecb,
	boost::asio::deadline_timer& timer)
{
	typedef ExtraHeaderResolver<BufElem, buffer_capacity, SuccessCb, ErrorCb> EHR;
	boost::shared_ptr<EHR> ehr(make_resolver(sock, buffer, scb, ecb));
	timer.async_wait(boost::bind(&EHR::stop, ehr));
	ehr->start();
}

// for use dynamic buffer.
template<typename BufElem, typename SuccessCb, typename ErrorCb>
GOL_DEPRECATED inline void resolve_extra_header(
	Sock& sock, BufElem* buf_ptr,
	const SuccessCb& scb, const ErrorCb& ecb)
{
	typedef ExtraHeaderResolver<BufElem, GOL_CEIL_DIV(GOL_EXTRA_HEADER_MAX_LENGTH * sizeof(byte_t),
		sizeof(BufElem)), SuccessCb, ErrorCb> EHR;
	GOL_ERR("NOTE: not sure whether your `socket buffer` is as wide as `" << EHR::buffer_capacity << " bytes` or not.")
	boost::shared_ptr<EHR> ehr(make_resolver(sock, *reinterpret_cast<typename EHR::Buffer*>(buf_ptr), scb, ecb));
	ehr->start();
}

template<typename BufElem, typename SuccessCb, typename ErrorCb>
GOL_DEPRECATED inline void resolve_extra_header(
	Sock& sock, BufElem* buf_ptr,
	const SuccessCb& scb, const ErrorCb& ecb,
	boost::asio::deadline_timer& timer)
{
	typedef ExtraHeaderResolver<BufElem, GOL_CEIL_DIV(GOL_EXTRA_HEADER_MAX_LENGTH * sizeof(byte_t),
		sizeof(BufElem)), SuccessCb, ErrorCb> EHR;
	GOL_ERR("NOTE: not sure whether your `socket buffer` is as wide as `" << EHR::buffer_capacity << " bytes` or not.")
	boost::shared_ptr<EHR> ehr(make_resolver(sock, *reinterpret_cast<typename EHR::Buffer*>(buf_ptr), scb, ecb));
	timer.async_wait(boost::bind(&EHR::stop, ehr));
	ehr->start();
}

}	// end namespace tgw
}	// end namespace gonline
