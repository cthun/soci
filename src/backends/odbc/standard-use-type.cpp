// Copyright (C) 2004-2006 Maciej Sobczak, Stephen Hutton, David Courtney
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define SOCI_ODBC_SOURCE
#include <soci-platform.h>
#include "soci-odbc.h"
#include "mnsocistring.h"
#include "blob.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

using namespace soci;
using namespace soci::details;

#ifdef _MSC_VER
#pragma warning(disable:4996)
#define snprintf _snprintf
#endif


void* odbc_standard_use_type_backend::prepare_for_bind(
    SQLLEN &size, SQLSMALLINT &sqlType, SQLSMALLINT &cType)
{
    switch (type_)
    {
    // simple cases
    case x_short:
        sqlType = SQL_SMALLINT;
        cType = SQL_C_SSHORT;
        size = sizeof(short);
        break;
    case x_integer:
        sqlType = SQL_INTEGER;
        cType = SQL_C_SLONG;
        size = sizeof(int);
        break;
    case x_long_long:
        //if (use_string_for_bigint())
        //{
        //  sqlType = SQL_NUMERIC;
        //  cType = SQL_C_CHAR;
        //  size = max_bigint_length;
        //  buf_ = new char[size];
        //  snprintf(buf_, size, "%" LL_FMT_FLAGS "d",
        //           *static_cast<long long *>(data_));
        //  indHolder_ = SQL_NTS;
        //}
        //else // Normal case, use ODBC support.
        //{
        sqlType = SQL_INTEGER;
        cType = SQL_C_SLONG;
          size = sizeof(long long);
        //}
        break;
    case x_unsigned_long_long:
        //if (use_string_for_bigint())
        //{
        //  sqlType = SQL_NUMERIC;
        //  cType = SQL_C_CHAR;
        //  size = max_bigint_length;
        //  buf_ = new char[size];
        //  snprintf(buf_, size, "%" LL_FMT_FLAGS "u",
        //           *static_cast<unsigned long long *>(data_));
        //  indHolder_ = SQL_NTS;
        //}
        //else // Normal case, use ODBC support.
        //{
        sqlType = SQL_INTEGER;
        cType = SQL_C_SLONG;
          size = sizeof(unsigned long long);
        //}
        break;
    case x_double:
    {
        sqlType = SQL_DOUBLE;
        cType = SQL_C_DOUBLE;
        size = sizeof(double);

        static bool bIsDB2 = statement_.session_.get_database_product() == odbc_session_backend::prod_db2;
        if (bIsDB2)
        {
            mn_odbc_error_info err;
            SQLSMALLINT sqlDataType;
            SQLULEN colSize;
            SQLSMALLINT decDigits;
            SQLSMALLINT isNullable;

            if (statement_.describe_param(position_, err, sqlDataType, colSize, decDigits, isNullable))
            {
                if (sqlDataType == SQL_DECIMAL && decDigits > 0)
                {
                    sqlType = SQL_VARCHAR;
                    cType = SQL_C_CHAR;
                    size = colSize * 2;
                    buf_ = new char[size];
                    memset(buf_, 0x0, size);
                    double dVal = *((double*)data_);
                    snprintf(buf_, size, "%.*lf", decDigits, *static_cast<double*>(data_));
                    *indHolder_ = SQL_NTS;
                }
            }
        }
        break;
    }

    case x_char:
        sqlType = SQL_CHAR;
        cType = SQL_C_CHAR;
        size = 2;
        buf_ = new char[size];
        buf_[0] = *static_cast<char*>(data_);
        buf_[1] = '\0';
        *indHolder_ = SQL_NTS;
        break;
    case x_mnsocistring:
        sqlType = SQL_VARCHAR;
        cType = SQL_C_CHAR;
		size = ((MNSociString*)data_)->getSize();
        buf_ = &(((MNSociString*)data_)->m_ptrCharData[0]); //use the char* inside the odbc call!!
        *indHolder_ = SQL_NTS;
        break;
	case x_mnsocitext:
		sqlType = SQL_VARCHAR;
		cType = SQL_C_CHAR;
		size = ((MNSociText*)data_)->getSize();
		buf_ = &(((MNSociText*)data_)->m_ptrCharData[0]); //use the char* inside the odbc call!!
		*indHolder_ = SQL_NTS;
		break;
    case x_stdstring:
    {
        std::string* s = static_cast<std::string*>(data_);
        sqlType = SQL_VARCHAR;
        cType = SQL_C_CHAR;
        size = s->size();
        buf_ = new char[size+1];
        memcpy(buf_, s->c_str(), size);
        buf_[size++] = '\0';
        *indHolder_ = SQL_NTS;
    }
    break;
    //case x_stdtm:
    //{
    //    std::tm *t = static_cast<std::tm *>(data_);

    //    sqlType = SQL_TIMESTAMP;
    //    cType = SQL_C_TIMESTAMP;
    //    buf_ = new char[sizeof(TIMESTAMP_STRUCT)];
    //    size = 19; // This number is not the size in bytes, but the number
    //               // of characters in the date if it was written out
    //               // yyyy-mm-dd hh:mm:ss

    //    TIMESTAMP_STRUCT * ts = reinterpret_cast<TIMESTAMP_STRUCT*>(buf_);

    //    ts->year = static_cast<SQLSMALLINT>(t->tm_year + 1900);
    //    ts->month = static_cast<SQLUSMALLINT>(t->tm_mon + 1);
    //    ts->day = static_cast<SQLUSMALLINT>(t->tm_mday);
    //    ts->hour = static_cast<SQLUSMALLINT>(t->tm_hour);
    //    ts->minute = static_cast<SQLUSMALLINT>(t->tm_min);
    //    ts->second = static_cast<SQLUSMALLINT>(t->tm_sec);
    //    ts->fraction = 0;
    //}
    //break;
    case x_odbctimestamp:
    {
        sqlType = SQL_TIMESTAMP;
        cType = SQL_C_TIMESTAMP;
        size = sizeof(TIMESTAMP_STRUCT); 
        break;
    }

    case x_blob:
    {
        cType = SQL_C_BINARY;
        sqlType = SQL_LONGVARBINARY;
        size = (1 << (8*sizeof(size)-1)) - 1; // TODO determinate configured BLOB size at run-time
        if( indHolder_ ) *indHolder_ = 0;

        blob *b = static_cast<blob *>(data_);

        odbc_blob_backend *bbe
            = static_cast<odbc_blob_backend *>(b->get_backend());

        bbe->statement_ = &statement_;
        bbe->position_ = position_;
        bbe->chunksize_ = 0;

        return (void*)bbe;
    }
    break;
    case x_statement:
    case x_rowid:
        // Unsupported data types.
        return NULL;
    }

    // Return either the pointer to C++ data itself or the buffer that we
    // allocated, if any.
    return buf_ ? buf_ : data_;
}

void odbc_standard_use_type_backend::bind_by_pos(
    int &position, void *data, exchange_type type, bool /* readOnly */, SQLLEN* ind)
{
    position_ = position++;
    data_ = data;
    type_ = type;
    indHolder_ = ind;
}

void odbc_standard_use_type_backend::pre_use()
{
    // first deal with data
    SQLSMALLINT sqlType;
    SQLSMALLINT cType;
    SQLLEN size;

    void* const sqlData = prepare_for_bind(size, sqlType, cType);

    SQLRETURN rc = SQLBindParameter(statement_.hstmt_,
                                    static_cast<SQLUSMALLINT>(position_),
                                    SQL_PARAM_INPUT,
                                    cType, sqlType, size, 0,
                                    sqlData, 0, indHolder_);

    if( type_ == x_blob )
    {
        if( indHolder_ )
            *indHolder_ = SQL_LEN_DATA_AT_EXEC(0);
    }

    if (is_odbc_error(rc))
    {
        throw odbc_soci_error(SQL_HANDLE_STMT, statement_.hstmt_,
                                "Binding");
    }
}

void odbc_standard_use_type_backend::clean_up()
{
    if (buf_ != NULL)
    {
        if (type_ != x_mnsocistring && type_ != x_mnsocitext)
        {
            delete[] buf_;
        }
        buf_ = NULL;
    }
}
