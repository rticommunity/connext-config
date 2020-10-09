
/*
WARNING: THIS FILE IS AUTO-GENERATED. DO NOT MODIFY.

This file was generated from Primes.idl using "rtiddsgen".
The rtiddsgen tool is part of the RTI Connext distribution.
For more information, type 'rtiddsgen -help' at a command shell
or consult the RTI Connext manual.
*/

#include "PrimesSupport.h"
#include "PrimesPlugin.h"

/* ========================================================================= */
/**
<<IMPLEMENTATION>>

Defines:   TData,
TDataWriter,
TDataReader,
TTypeSupport

Configure and implement 'PrimeNumberRequest' support classes.

Note: Only the #defined classes get defined
*/

/* ----------------------------------------------------------------- */
/* DDSDataWriter
*/

/**
<<IMPLEMENTATION >>

Defines:   TDataWriter, TData
*/

/* Requires */
#define TTYPENAME   PrimeNumberRequestTYPENAME

/* Defines */
#define TDataWriter PrimeNumberRequestDataWriter
#define TData       PrimeNumberRequest

#define ENABLE_TDATAWRITER_DATA_CONSTRUCTOR_METHODS
#include "dds_c/generic/dds_c_data_TDataWriter.gen"
#undef ENABLE_TDATAWRITER_DATA_CONSTRUCTOR_METHODS

#undef TDataWriter
#undef TData

#undef TTYPENAME

/* ----------------------------------------------------------------- */
/* DDSDataReader
*/

/**
<<IMPLEMENTATION >>

Defines:   TDataReader, TDataSeq, TData
*/

/* Requires */
#define TTYPENAME   PrimeNumberRequestTYPENAME

/* Defines */
#define TDataReader PrimeNumberRequestDataReader
#define TDataSeq    PrimeNumberRequestSeq
#define TData       PrimeNumberRequest

#define ENABLE_TDATAREADER_DATA_CONSISTENCY_CHECK_METHOD
#include "dds_c/generic/dds_c_data_TDataReader.gen"
#undef ENABLE_TDATAREADER_DATA_CONSISTENCY_CHECK_METHOD

#undef TDataReader
#undef TDataSeq
#undef TData

#undef TTYPENAME

/* ----------------------------------------------------------------- */
/* TypeSupport

<<IMPLEMENTATION >>

Requires:  TTYPENAME,
TPlugin_new
TPlugin_delete
Defines:   TTypeSupport, TData, TDataReader, TDataWriter
*/

/* Requires */
#define TTYPENAME    PrimeNumberRequestTYPENAME
#define TPlugin_new  PrimeNumberRequestPlugin_new
#define TPlugin_delete  PrimeNumberRequestPlugin_delete

/* Defines */
#define TTypeSupport PrimeNumberRequestTypeSupport
#define TData        PrimeNumberRequest
#define TDataReader  PrimeNumberRequestDataReader
#define TDataWriter  PrimeNumberRequestDataWriter
#define TGENERATE_SER_CODE
#ifndef NDDS_STANDALONE_TYPE
#define TGENERATE_TYPECODE
#endif

#include "dds_c/generic/dds_c_data_TTypeSupport.gen"

#undef TTypeSupport
#undef TData
#undef TDataReader
#undef TDataWriter
#ifndef NDDS_STANDALONE_TYPE
#undef TGENERATE_TYPECODE
#endif
#undef TGENERATE_SER_CODE
#undef TTYPENAME
#undef TPlugin_new
#undef TPlugin_delete

/* ========================================================================= */
/**
<<IMPLEMENTATION>>

Defines:   TData,
TDataWriter,
TDataReader,
TTypeSupport

Configure and implement 'PrimeNumberReply' support classes.

Note: Only the #defined classes get defined
*/

/* ----------------------------------------------------------------- */
/* DDSDataWriter
*/

/**
<<IMPLEMENTATION >>

Defines:   TDataWriter, TData
*/

/* Requires */
#define TTYPENAME   PrimeNumberReplyTYPENAME

/* Defines */
#define TDataWriter PrimeNumberReplyDataWriter
#define TData       PrimeNumberReply

#define ENABLE_TDATAWRITER_DATA_CONSTRUCTOR_METHODS
#include "dds_c/generic/dds_c_data_TDataWriter.gen"
#undef ENABLE_TDATAWRITER_DATA_CONSTRUCTOR_METHODS

#undef TDataWriter
#undef TData

#undef TTYPENAME

/* ----------------------------------------------------------------- */
/* DDSDataReader
*/

/**
<<IMPLEMENTATION >>

Defines:   TDataReader, TDataSeq, TData
*/

/* Requires */
#define TTYPENAME   PrimeNumberReplyTYPENAME

/* Defines */
#define TDataReader PrimeNumberReplyDataReader
#define TDataSeq    PrimeNumberReplySeq
#define TData       PrimeNumberReply

#define ENABLE_TDATAREADER_DATA_CONSISTENCY_CHECK_METHOD
#include "dds_c/generic/dds_c_data_TDataReader.gen"
#undef ENABLE_TDATAREADER_DATA_CONSISTENCY_CHECK_METHOD

#undef TDataReader
#undef TDataSeq
#undef TData

#undef TTYPENAME

/* ----------------------------------------------------------------- */
/* TypeSupport

<<IMPLEMENTATION >>

Requires:  TTYPENAME,
TPlugin_new
TPlugin_delete
Defines:   TTypeSupport, TData, TDataReader, TDataWriter
*/

/* Requires */
#define TTYPENAME    PrimeNumberReplyTYPENAME
#define TPlugin_new  PrimeNumberReplyPlugin_new
#define TPlugin_delete  PrimeNumberReplyPlugin_delete

/* Defines */
#define TTypeSupport PrimeNumberReplyTypeSupport
#define TData        PrimeNumberReply
#define TDataReader  PrimeNumberReplyDataReader
#define TDataWriter  PrimeNumberReplyDataWriter
#define TGENERATE_SER_CODE
#ifndef NDDS_STANDALONE_TYPE
#define TGENERATE_TYPECODE
#endif

#include "dds_c/generic/dds_c_data_TTypeSupport.gen"

#undef TTypeSupport
#undef TData
#undef TDataReader
#undef TDataWriter
#ifndef NDDS_STANDALONE_TYPE
#undef TGENERATE_TYPECODE
#endif
#undef TGENERATE_SER_CODE
#undef TTYPENAME
#undef TPlugin_new
#undef TPlugin_delete

