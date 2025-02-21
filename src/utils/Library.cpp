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

#include "Library.h"
#include "Log.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#endif

Library::Library() : m_pLibrary( NULL )
{}

Library::Library( const Library & a_Copy ) : m_pLibrary( NULL )
{
	Load( a_Copy.m_Lib );
}

Library::Library( const std::string & a_Lib ) : m_pLibrary( NULL )
{
	Load( a_Lib );
}

Library::~Library()
{
	Unload();
}

void Library::Load( const std::string & a_Lib )
{
	m_Lib = a_Lib;

#if defined(_WIN32)
	const std::string POSTFIX(".dll");
	m_pLibrary = LoadLibrary( (m_Lib + POSTFIX).c_str() );
#elif defined(__APPLE__)
    const std::string POSTFIX(".dylib");
	const std::string PREFIX("lib");
	std::string  libFile = PREFIX + m_Lib + POSTFIX;
	m_pLibrary = dlopen( libFile.c_str(), RTLD_LAZY );
	if ( m_pLibrary == NULL )
		Log::Error( "Library", "dlerror: %s", dlerror() );
#else
	const std::string PREFIX("lib");
	const std::string POSTFIX(".so");
	std::string  libFile = PREFIX + m_Lib + POSTFIX;
	m_pLibrary = dlopen( libFile.c_str(), RTLD_LAZY );
	if ( m_pLibrary == NULL )
		Log::Error( "Library", "dlerror: %s", dlerror() );
#endif

	if ( m_pLibrary == NULL )
		Log::Warning( "Library", "Failed to load dynamic library %s.", m_Lib.c_str() );
}

void Library::Unload()
{
	if ( m_pLibrary != NULL )
	{
#if defined(_WIN32)
		FreeLibrary( (HINSTANCE)m_pLibrary );
#else
		dlclose( m_pLibrary );
#endif
		m_pLibrary = NULL;
	}

	m_Lib.clear();
}

