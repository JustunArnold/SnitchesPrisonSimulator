///////////////////////////////////////////////////////////////////////
//
//  *** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with
//  the terms of that agreement.
//
//      Copyright (c) 2003-2017 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////
//	CArray::CArray

template<typename T>
ST_INLINE CArray<T>::CArray(void) :
	m_pData(NULL)
{
}


///////////////////////////////////////////////////////////////////////
//	CArray::IsEmpty

template<typename T>
ST_INLINE st_bool CArray<T>::IsEmpty(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return (*(reinterpret_cast<st_uint32*>(m_pData)) > 0);
}


///////////////////////////////////////////////////////////////////////
//	CArray::Count

template<typename T>
ST_INLINE st_uint32 CArray<T>::Count(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return *(reinterpret_cast<st_uint32*>(m_pData));
}


///////////////////////////////////////////////////////////////////////
//	CArray::Data

template<typename T>
ST_INLINE const T* CArray<T>::Data(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return reinterpret_cast<T*>(m_pData + sizeof(st_uint32));
}


///////////////////////////////////////////////////////////////////////
//	CArray::operator[]

template<typename T>
ST_INLINE const T& CArray<T>::operator[](st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	return (Data( )[uiIndex]);
}


///////////////////////////////////////////////////////////////////////
//	CString::IsEmpty

ST_INLINE st_bool CString::IsEmpty(void) const
{
	return (Count( ) < 2);
}


///////////////////////////////////////////////////////////////////////
//	CString::Length

ST_INLINE st_uint32 CString::Length(void) const
{
	return (Count( ) - 1);
}


///////////////////////////////////////////////////////////////////////
//	CTable::CTable

ST_INLINE CTable::CTable(void) :
	m_pData(NULL)
{
}


///////////////////////////////////////////////////////////////////////
//	CTable::Count

ST_INLINE st_uint32 CTable::Count(void) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	return *(reinterpret_cast<st_uint32*>(m_pData));
}


///////////////////////////////////////////////////////////////////////
//	CTable::GetValue

template <typename T>
ST_INLINE const T& CTable::GetValue(st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	st_uint32 uiDataIndex = *(reinterpret_cast<st_uint32*>(m_pData) + uiIndex + 1);
	return *(reinterpret_cast<T*>(m_pData + uiDataIndex));
}


///////////////////////////////////////////////////////////////////////
//	CTable::GetContainer

template <typename T>
ST_INLINE T CTable::GetContainer(st_uint32 uiIndex) const
{
	st_assert(m_pData != NULL, "Data is NULL");
	st_assert(uiIndex < Count( ), "Index out of range");
	st_uint32 uiDataIndex = *(reinterpret_cast<st_uint32*>(m_pData) + uiIndex + 1);
	T tReturn;
	tReturn.m_pData = m_pData + uiDataIndex;
	return tReturn;
}


///////////////////////////////////////////////////////////////////////
//	CTableArray::operator[]

template<typename T>
ST_INLINE T CTableArray<T>::operator[](st_uint32 uiIndex) const
{
	return GetContainer<T>(uiIndex);
}


///////////////////////////////////////////////////////////////////////
//	CReader::CReader

ST_INLINE CReader::CReader(void) :
	m_pFileData(NULL),
	m_bOwnsData(false)
{
}


///////////////////////////////////////////////////////////////////////
//	CReader::~CReader

ST_INLINE CReader::~CReader(void)
{
	Clear( );
}


///////////////////////////////////////////////////////////////////////
//	CReader::CReader

ST_INLINE st_bool CReader::Valid(void)
{
	return (m_pData != NULL);
}


///////////////////////////////////////////////////////////////////////
//	CReader::Clear

ST_INLINE void CReader::Clear(void)
{
	if (m_bOwnsData && m_pFileData != NULL)
	{
		delete [] m_pFileData;
	}

	m_pFileData = NULL;
	m_bOwnsData = false;
	m_pData = NULL;
}


///////////////////////////////////////////////////////////////////////
//	CReader::LoadFile

ST_INLINE st_bool CReader::LoadFile(st_char* pFilename)
{
	st_bool bReturn = false;
	Clear( );

	FILE* pFile = NULL;
	#ifdef WIN32
		fopen_s(&pFile, pFilename, "rb");
	#else
		pFile = fopen(pFilename, "rb");
	#endif

	if (pFile != NULL)
	{
		fseek(pFile, 0L, SEEK_END);
		st_int32 iNumBytes = ftell(pFile);
		st_int32 iErrorCode = fseek(pFile, 0L, SEEK_SET);
		if (iNumBytes > 0 && iErrorCode >= 0)
		{
			m_pFileData = new st_byte[iNumBytes];
			st_int32 iNumBytesRead = st_int32(fread(m_pFileData, 1, iNumBytes, pFile));
			if (iNumBytesRead == iNumBytes)
			{
				const st_char* pToken = FileToken( );
				st_int32 iTokenLength = st_int32(strlen(pToken));
				if (iTokenLength < iNumBytesRead)
				{
					m_pData = m_pFileData + iTokenLength;
					bReturn = true;
					for (st_int32 i = 0; (i < iTokenLength) && bReturn; ++i)
					{
						if (pToken[i] != m_pFileData[i])
						{
							bReturn = false;
						}
					}
				}
			}
		}

		fclose(pFile);
	}

	if (!bReturn)
	{
		Clear( );
	}

	return bReturn;
}


///////////////////////////////////////////////////////////////////////
//	CReader::LoadFromData

ST_INLINE st_bool CReader::LoadFromData(const st_byte* pData, st_int32 iSize)
{
	bool bReturn = false;
	Clear( );

	m_pFileData = const_cast<st_byte*>(pData);
	m_bOwnsData = false;

	const st_char* pToken = FileToken( );
	st_int32 iTokenLength = st_int32(strlen(pToken));
	if (iTokenLength < iSize)
	{
		m_pData = m_pFileData + iTokenLength;
		bReturn = true;
		for (st_int32 i = 0; (i < iTokenLength) && bReturn; ++i)
		{
			if (pToken[i] != m_pFileData[i])
			{
				bReturn = false;
			}
		}
	}

	if (!bReturn)
	{
		Clear( );
	}

	return bReturn;
}

