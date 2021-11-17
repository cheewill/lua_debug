
#ifndef __NEW_FOPEN_H__
#define __NEW_FOPEN_H__

#include <stdio.h>
#include "curl_setup.h"

FILE*
__stdcall 
_new_fopen(
	_In_ char* pszUtf8FileName, 
	_In_ const char *mode
	);


int
__stdcall 
_new_stat(
	_In_ char* pszUtf8FileName,
	_In_ struct_stat *file
	);

#endif	//__NEW_FOPEN_H__
