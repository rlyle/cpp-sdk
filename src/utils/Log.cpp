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

#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#include <windows.h>
#endif

#include <boost/filesystem.hpp>
#include <string>
#include <list>
#include <stdio.h>
#include <time.h>

#include "Log.h"
#include "Time.h"
#include "StringUtil.h"
#include "ThreadPool.h"
#include "WatsonException.h"

void ConsoleReactor::Process(const LogRecord & a_Record)
{
	if (a_Record.m_Level >= m_MinLevel)
	{
#ifdef _WIN32
		HANDLE h = NULL;
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (a_Record.m_Level >= LL_WARNING)
		{
			h = GetStdHandle(STD_OUTPUT_HANDLE);
			GetConsoleScreenBufferInfo(h, &csbi);
			if ( a_Record.m_Level >= LL_ERROR )
				SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY );
			else
				SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY );
		}
#endif
		printf("[%s][%s][%s] %s\n",
			a_Record.m_Time.c_str(),
			Log::LevelText(a_Record.m_Level),
			a_Record.m_SubSystem.c_str(),
			a_Record.m_Message.c_str());

#ifdef _WIN32
		if (h)
			SetConsoleTextAttribute(h, csbi.wAttributes );
#endif
	}
}

FileReactor::FileReactor(const char * a_pLogFile, LogLevel a_MinLevel /*= DEBUG*/, int a_LogHistory /*= 5*/) :
	m_LogFile(a_pLogFile),
	m_MinLevel(a_MinLevel),
	m_pThread( NULL ),
	m_bStopThread(false),
	m_bThreadStopped(false)
{
	// rotate log files...
	try {
		boost::filesystem::remove( StringUtil::Format( "%s.%d", a_pLogFile, a_LogHistory - 1 ).c_str() );
		for(int i=a_LogHistory - 1;i>0;--i)
		{
			std::string src = StringUtil::Format( "%s.%d", a_pLogFile, i - 1);
			if ( boost::filesystem::exists( src ) )
			{
				std::string dst = StringUtil::Format( "%s.%d", a_pLogFile, i );
				boost::filesystem::rename( src.c_str(), dst.c_str() );
			}
		}

		if ( boost::filesystem::exists( a_pLogFile ) )
			boost::filesystem::rename( a_pLogFile, StringUtil::Format( "%s.%d", a_pLogFile, 0 ) );
	}
	catch( const std::exception & )
	{}

	// start our thread for writing to files..
	m_pThread = new boost::thread( boost::bind( &FileReactor::WriteThread, this ) );
}

FileReactor::~FileReactor()
{
	if ( m_pThread != NULL )
	{
		m_bStopThread = true;
		while(! m_bThreadStopped )
			boost::this_thread::yield();

		delete m_pThread;
		m_pThread = NULL;
	}
}

void FileReactor::Process(const LogRecord & a_Record)
{
	if (a_Record.m_Level >= m_MinLevel)
	{
		std::string log( StringUtil::Format( "[%s][%s][%s] %s\n", 
			a_Record.m_Time.c_str(), Log::LevelText( a_Record.m_Level ), a_Record.m_SubSystem.c_str(), a_Record.m_Message.c_str() ) );

		m_OutputLock.lock();
		m_Output.push_back( log );
		m_OutputLock.unlock();
	}
}

void FileReactor::WriteThread()
{
	while(! m_bStopThread )
	{
		boost::this_thread::sleep( boost::posix_time::milliseconds( 250 ) );

		m_OutputLock.lock();
		if ( m_Output.begin() != m_Output.end() )
		{
			// transfer data into a local list so we don't block the main thread for long..
			LogList output;
			output.splice( output.begin(), m_Output );
			m_OutputLock.unlock();

			FILE * f = fopen(m_LogFile.c_str(), "a+");
			if ( f != NULL )
			{
				for( LogList::const_iterator iLog = output.begin(); 
					iLog != output.end(); ++iLog )
				{
					const std::string & log = *iLog;
					fwrite( log.c_str(), sizeof(char), log.size(), f );
				}

				fclose( f );
			}
		}
		else
		{
			// nothing to do..
			m_OutputLock.unlock();
		}
	}

	m_bThreadStopped = true;
}

Log::ReactorList & Log::GetReactorList()
{
	static ReactorList reactors;
	return reactors;
}

boost::recursive_mutex & Log::GetReactorLock()
{
	static boost::recursive_mutex lock;
	return lock;
}

void Log::RegisterReactor(ILogReactor * a_pReactor)
{
	boost::lock_guard<boost::recursive_mutex> lock( GetReactorLock() );
	GetReactorList().push_back(a_pReactor);
}

void Log::RemoveReactor(ILogReactor * a_pReactor, bool a_bDelete /*= true */)
{
	boost::lock_guard<boost::recursive_mutex> lock( GetReactorLock() );
	GetReactorList().remove(a_pReactor);
	if ( a_bDelete )
		delete a_pReactor;
}

void Log::RemoveAllReactors( bool a_bDelete /*= true*/ )
{
	boost::lock_guard<boost::recursive_mutex> lock( GetReactorLock() );
	ReactorList & reactors = GetReactorList();

	if ( a_bDelete )
	{
		for (ReactorList::iterator iReactor = reactors.begin(); iReactor != reactors.end(); )
		{
			ILogReactor * pReactor = *iReactor++;
			delete pReactor;
		}
	}

	reactors.clear();
}

void Log::DoLog(LogLevel a_Level, const char * a_pSub, const char * a_pFormat, va_list args )
{
	char buffer[1024 * 32];

	LogRecord rec;
	rec.m_Level = a_Level;
	rec.m_SubSystem = a_pSub;

	Time now;
	rec.m_Time = now.GetFormattedTime( "%x %X" ) + StringUtil::Format(".%0.3d", (int)now.GetMilliseconds()); 
	rec.m_TimeEpoch = now.GetTime();

	vsnprintf(buffer, sizeof(buffer) - 1, a_pFormat, args);
	rec.m_Message = buffer;

	ProcessRecord(rec);
}

void Log::ProcessRecord(const LogRecord & rec)
{
	boost::lock_guard<boost::recursive_mutex> lock( GetReactorLock() );
	for (ReactorList::iterator iReactor = GetReactorList().begin();
		iReactor != GetReactorList().end(); ++iReactor)
	{
		(*iReactor)->Process(rec);
	}
}

void Log::DebugLow(const char * a_pSub, const char * a_pFormat, ...)
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog(LL_DEBUG_LOW, a_pSub, a_pFormat, args);
	va_end(args);
}

void Log::DebugMed(const char * a_pSub, const char * a_pFormat, ...)
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog(LL_DEBUG_MED, a_pSub, a_pFormat, args);
	va_end(args);
}

void Log::DebugHigh(const char * a_pSub, const char * a_pFormat, ...)
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog(LL_DEBUG_HIGH, a_pSub, a_pFormat, args);
	va_end(args);
}

void Log::Debug( const char * a_pSub, const char * a_pFormat, ... )
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog( LL_DEBUG, a_pSub, a_pFormat, args );
	va_end(args);
}

void Log::Status( const char * a_pSub, const char * a_pFormat, ... )
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog( LL_STATUS, a_pSub, a_pFormat, args );
	va_end(args);
}

void Log::Warning( const char * a_pSub, const char * a_pFormat, ... )
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog( LL_WARNING, a_pSub, a_pFormat, args );
	va_end(args);
}

void Log::Error( const char * a_pSub, const char * a_pFormat, ... )
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog( LL_ERROR, a_pSub, a_pFormat, args );
	va_end(args);
}

void Log::Critical( const char * a_pSub, const char * a_pFormat, ... )
{
	va_list args;
	va_start(args, a_pFormat);
	DoLog( LL_CRITICAL, a_pSub, a_pFormat, args );
	va_end(args);
}

const char * Log::LevelText( LogLevel a_Level )
{
	static const char * LEVELS[] = 
	{
		"DEBL",
		"DEBM",
		"DEBH",
		"STAT",
		"WARN",
		"ERRO",
		"CRIT"
	};

	return LEVELS[ a_Level ];
}
