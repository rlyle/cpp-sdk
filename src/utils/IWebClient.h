/**
* Copyright 2016 IBM Corp. All Rights Reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#ifndef WDC_IWEBCLIENT_H
#define WDC_IWEBCLIENT_H

#include <map>

#include "boost/atomic.hpp"

#include "RTTI.h"
#include "Delegate.h"
#include "StringUtil.h"
#include "URL.h"
#include "IWebSocket.h"
#include "WDCLib.h"		
#include "Factory.h"

//! Abstract interface for a web client class
class WDC_API IWebClient : public IWebSocket
{
public:
	RTTI_DECL();

	//! Stats
	static boost::atomic<unsigned int>		sm_RequestsSent;
	static boost::atomic<unsigned int>		sm_BytesSent;
	static boost::atomic<unsigned int>		sm_BytesRecv;

	//! Types
	typedef std::map< std::string, std::string, StringUtil::ci_less >	Headers;
	typedef std::multimap< std::string, std::string >					Cookies;

	typedef boost::shared_ptr<IWebClient>								SP;
	typedef boost::weak_ptr<IWebClient>									WP;

	struct RequestData
	{
		RequestData() : m_StatusCode(0), m_bDone( false )
		{}
		RequestData( const RequestData & a_Copy ) : 
			m_Version( a_Copy.m_Version ),
			m_StatusCode( a_Copy.m_StatusCode ),
			m_StatusMessage( a_Copy.m_StatusMessage ),
			m_SetCookies( a_Copy.m_SetCookies ),
			m_Headers( a_Copy.m_Headers ),
			m_bDone( false )
		{}

		std::string		m_Version;
		unsigned int	m_StatusCode;
		std::string		m_StatusMessage;
		Cookies			m_SetCookies;
		Headers			m_Headers;
		std::string		m_Content;
		bool			m_bDone;			// set to true if the socket has been closed and this is the last RequestData object
	};


	// states for the internal socket object
	enum SocketState
	{
		CONNECTING,		// trying to establish a connection
		CONNECTED,		// connection established, ready to send/receive data
		CLOSING,		// set when Close() is invoked and before it's really closed
		CLOSED,			// connection has been closed gracefully
		RETRY,			// something failed, we are retrying to sends
		DISCONNECTED	// connection has been lost
	};

	typedef std::list< SP >								ConnectionList;
	typedef std::map< std::string, ConnectionList >		ConnectionMap;

	//! Static function for creating a concrete WebClient class.
	static ConnectionMap & GetConnectionMap();
	static Factory<IWebClient> & GetFactory();
	static SP Create( const URL & a_URL );
	static void Free( const SP & a_spClient );

	//! Destruction
	virtual ~IWebClient()
	{}

	//! This returns the current state of the internal socket
	virtual SocketState GetState() const = 0;
	virtual const URL & GetURL() const = 0;
	virtual const Headers & GetHeaders() const = 0;

	//! Set the connection target
	virtual void SetURL(const URL & a_URL) = 0;
	//! This delegate will be invoked when this Connection changes state
	virtual void SetStateReceiver(Delegate<IWebClient *> a_StateReceiver) = 0;
	//! provide a delegate for receiving the raw data
	virtual void SetDataReceiver(Delegate<RequestData *> a_DataReceiver) = 0;

	//! The following functions only apply not TCP connection types
	virtual void SetHeader(const std::string & a_Key,
		const std::string & a_Value) = 0;
	virtual void SetHeaders(const Headers & a_Headers, bool a_bMerge = false) = 0;
	virtual void SetRequestType(const std::string & a_ReqType) = 0;		// GET, POST, DELETE
	virtual void SetBody(const std::string & a_Body) = 0;
	//! Send a request, this should be the last call.
	virtual bool Send() = 0;
	//! Close this connection.
	virtual bool Close() = 0;
	//! This shutdowns down this client, it will block until fully closed.
	virtual bool Shutdown() = 0;

	//! High-level interface for making a single HTTP/HTTPS request.
	static SP Request(const URL & a_URL,
		const Headers & a_Headers,
		const std::string & a_RequestType,
		const std::string & a_Body,
		Delegate<RequestData *> a_DataReceiver,
		Delegate<IWebClient *> a_StateReceiver)
	{
		SP spClient = IWebClient::Create( a_URL );
		spClient->SetHeaders( a_Headers );
		spClient->SetRequestType( a_RequestType );
		spClient->SetBody( a_Body );
		spClient->SetDataReceiver( a_DataReceiver );
		spClient->SetStateReceiver( a_StateReceiver );
		spClient->Send();

		return spClient;
	}

	SP shared_from_this()
	{
		return boost::static_pointer_cast<IWebClient>(IWebSocket::shared_from_this());
	}

	static void SetClientId( const std::string & a_ClientId )
	{
		sm_ClientId = a_ClientId;
	}

	static const std::string & GetClientId()
	{
		return sm_ClientId;
	}
protected:
	//! Data
	static std::string		sm_ClientId;
};

#endif

