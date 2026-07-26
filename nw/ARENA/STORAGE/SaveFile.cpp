#define LAST_H__VIEW
#include "game.h"
#include "storage/h/savefile.h"
#include "kernel\h\echo.h"
#include <limits.h>


const char CheckString[] = "Next Worlds";

PIN_SaveFile::PIN_SaveFile()
{
	memset((PIN_SaveFileHeader*)this,0,sizeof(PIN_SaveFileHeader));
	m_data = 0;
	m_counter = 0;
	m_length = 0;
	m_hFileMapping = INVALID_HANDLE_VALUE;
	m_hFile = INVALID_HANDLE_VALUE;
	strcpy(m_Check, CheckString);
}

char * PIN_SaveFile::GetCurrentData()
{
	if (m_data == NULL || m_counter < 0 || m_counter >= m_length)
		return NULL;
	ASSERT(m_counter < m_length);
	return & m_data[m_counter];
}

int  PIN_SaveFile::Shift(int size)
{
	if (size < 0 || m_counter < 0 || size > m_length-m_counter)
		return false;
	m_counter += size;
	ASSERT(m_counter <= m_length);
	return m_counter;
}

int  PIN_SaveFile::GetNextDataItemType()
{
	const int prefixOffset = (int)sizeof(int);
	const int needed = prefixOffset+(int)sizeof(PIN_SaveItemPrefix);
	if (m_data == NULL || m_counter < 0 || m_counter > m_length-needed)
		return PIN_SaveItemPrefix::IP_ERROR;
	PIN_SaveItemPrefix prefix;
	memcpy(&prefix,&m_data[m_counter+prefixOffset],sizeof(prefix));

	if (memcmp(prefix.m_Check,PIN_check,sizeof(PIN_check)) != 0)
	{
		echo("Wrong check");
		return PIN_SaveItemPrefix::IP_ERROR;
	}

	return prefix.m_Type;
}

int  PIN_SaveFile::GetData(char * __data, int size)
{
	const int prefixSize = (int)sizeof(int);
	if (__data == NULL || size < 0 || m_counter < 0 ||
		m_counter > m_length-prefixSize ||
		size > m_length-m_counter-prefixSize)
		return false;

	int controlSize;
	memcpy(&controlSize,&m_data[m_counter],sizeof(controlSize));

	if (controlSize != size)
		return false;

	m_counter += prefixSize;

	memcpy(__data, &m_data[m_counter], size);
	m_counter += size;

	return m_counter;
}



bool PIN_SaveFile::OpenWrite (char * filename)
{
	if (filename == NULL)
		return false;
	m_hFile = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}


	if (!WriteHeader())
		return false;

	return true;
}


void PIN_SaveFile::Close()
{

	if (m_data)
	{
		if (!UnmapViewOfFile(m_data))
		 echo("Error UnMapping");
	}

	m_data = 0;

	if (m_hFileMapping != NULL && m_hFileMapping != INVALID_HANDLE_VALUE)
	{
		if (!CloseHandle(m_hFileMapping))
			echo("Error closing Mapping");
	}

	if (m_hFile != INVALID_HANDLE_VALUE)
		CloseHandle(m_hFile);

	m_hFileMapping  = INVALID_HANDLE_VALUE;
	m_hFile		= INVALID_HANDLE_VALUE;
	m_counter = 0;
	m_length = 0;
}


bool PIN_SaveFile::WriteData(char * __data, int size)
{
	if (m_hFile == INVALID_HANDLE_VALUE || __data == NULL || size < 0)
		return false;

	DWORD written;


	bool result = WriteFile(m_hFile, (char *) & size, sizeof(int) ,&written, NULL);

	if (!result || written != sizeof(int))
	{
		return false;
	}


	result = WriteFile(m_hFile, __data, size,&written, NULL);

	if (!result || written != (DWORD)size)
		return false;

	return true;
}

bool PIN_SaveFile::WriteHeader()
{
	/*DWORD written;

	bool result = WriteFile(m_hFile, this, sizeof(PIN_SaveFileHeader),&written, NULL);

	if (!result || written != sizeof(PIN_SaveFileHeader))
		return false;*/

	return WriteData(m_Check, sizeof(PIN_SaveFileHeader) );
}


bool PIN_SaveFile::OpenRead(char * filename)
{
	if (filename == NULL)
		return false;

	m_counter = 0;
	m_length  = 0;

	m_hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);

	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	m_hFileMapping = CreateFileMapping(m_hFile, NULL, PAGE_READONLY, 0, 0, NULL);

	if (!m_hFileMapping)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
		m_hFileMapping = INVALID_HANDLE_VALUE;
		return false;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(m_hFile,&fileSize) || fileSize.QuadPart < 0 ||
		fileSize.QuadPart > INT_MAX)
	{
		Close();
		return false;
	}
	m_length = (int)fileSize.QuadPart;

	m_data = (char * ) MapViewOfFile(m_hFileMapping, FILE_MAP_READ, 0, 0, 0);

	if (m_data == NULL)
	{
		Close();
		return false;
	}

	//memcpy(this, m_data, sizeof(PIN_SaveFileHeader)); // to fill out check structures.

	if (!GetData(m_Check, sizeof(PIN_SaveFileHeader) ) ||
		  memcmp(m_Check,CheckString,sizeof(CheckString)) != 0)
	{
		Close();
		return false;
	}

	return true;
}
