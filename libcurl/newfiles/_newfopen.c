#include "_newfopen.h"
#ifdef _MSC_VER
#include <windows.h>
#include <assert.h>
#endif

#ifdef _MSC_VER

#define ISSLASH(a)  ((a) == L'\\' || (a) == L'/')

#define A_RO            0x1     /* read only */
#define A_H             0x2     /* hidden */
#define A_S             0x4     /* system */
#define A_V             0x8     /* volume id */
#define A_D             0x10    /* directory */
#define A_A             0x20    /* archive */

#define A_MOD   (A_RO+A_H+A_S+A_A)      /* changeable attributes */

static
LPWSTR
__stdcall
AnsiToUnicode(
	_In_ LPCSTR pstr
)
{
	assert(pstr != NULL);
	int nLen = MultiByteToWideChar(CP_ACP, 0, pstr, -1, NULL, 0);

	WCHAR *pwstr = (WCHAR*)HeapAlloc(GetProcessHeap(),
									 HEAP_ZERO_MEMORY,
									 nLen * sizeof(WCHAR));
	if (pwstr) {
		MultiByteToWideChar(CP_ACP, 0, pstr, -1, pwstr, nLen);
	}
	return pwstr;
}

static 
LPWSTR
__stdcall
Utf8ToUnicode(
	_In_ LPCSTR pstr
)
{
	assert(pstr != NULL);
	int nLen = MultiByteToWideChar(CP_UTF8, 0, pstr, -1, NULL, 0);

	WCHAR *pwstr = (WCHAR*)HeapAlloc(GetProcessHeap(),
									HEAP_ZERO_MEMORY,
									nLen * sizeof(WCHAR));
	if (pwstr) {
		MultiByteToWideChar(CP_UTF8, 0, pstr, -1, pwstr, nLen);
	}
	return pwstr;
}

static
void
__stdcall
FreeStr(
	_In_ LPVOID lpStrMem
)
{
	if (lpStrMem != NULL) {
		HeapFree(GetProcessHeap(), 0, lpStrMem);
	}
}

unsigned short
__cdecl
_new_tdtoxmode(int attr, const WCHAR *name)
{
	unsigned short uxmode;
	unsigned dosmode;
	const WCHAR *p;

	dosmode = attr & 0xff;
	if ((p = name)[1] == L':')
		p += 2;

	/* check to see if this is a directory - note we must make a special
	* check for the root, which DOS thinks is not a directory
	*/
	uxmode = (unsigned short)
		(((ISSLASH(*p) && !p[1]) || (dosmode & A_D) || !*p)
			? _S_IFDIR | _S_IEXEC : _S_IFREG);

	/* If attribute byte does not have read-only bit, it is read-write */

	uxmode |= (dosmode & A_RO) ? _S_IREAD : (_S_IREAD | _S_IWRITE);

	/* see if file appears to be executable - check extension of name */

	if ((p = wcsrchr(name, L'.')) != NULL) {
		if (!_wcsicmp(p, L".exe") ||
			!_wcsicmp(p, L".cmd") ||
			!_wcsicmp(p, L".bat") ||
			!_wcsicmp(p, L".com"))
			uxmode |= _S_IEXEC;
	}

	/* propagate user read/write/execute bits to group/other fields */
	uxmode |= (uxmode & 0700) >> 3;
	uxmode |= (uxmode & 0700) >> 6;

	return(uxmode);
}


void 
__stdcall 
_systemTimeToTimet32(SYSTEMTIME st, __time32_t *pt)
{
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	LONGLONG ll;
	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;
	ll = ft.dwHighDateTime;
	ll = (ll <<32);
	ll += ft.dwLowDateTime;
	*pt = (DWORD)((LONGLONG)(ui.QuadPart - 116444736000000000) / 10000000);
}

void 
__stdcall 
_systemTimeToTimet64(SYSTEMTIME st, __time64_t *pt)
{
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	LONGLONG ll;
	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;
	ll = ft.dwHighDateTime;
	ll = (ll << 32);
	ll += ft.dwLowDateTime;
	*pt = (DWORD)((LONGLONG)(ui.QuadPart - 116444736000000000) / 10000000);
}

int
__stdcall
_new_getdrive(void)
{
	ULONG drivenum = 0;
	wchar_t curdirstr[_MAX_PATH + 1] = { 0 };
	wchar_t *cdirstr = curdirstr;
	int memfree = 0, r = 0;

	r = GetCurrentDirectoryW(MAX_PATH + 1, cdirstr);
	if (r > MAX_PATH) {
		if ((cdirstr = (wchar_t *)malloc((r + 1) * sizeof(wchar_t))) == NULL) {
			errno = ENOMEM;
			r = 0;
		} else {
			memfree = 1;
		}

		if (r) {
			r = GetCurrentDirectoryW(r + 1, cdirstr);
		}
	}

	drivenum = 0;
	if (r) {
		if (cdirstr[1] == L':') {
			drivenum = __ascii_towupper(cdirstr[0]) - L'A' + 1;
		}
	} else {
		errno = ENOMEM;
	}

	if (memfree) {
		free(cdirstr);
	}

	return drivenum;
}

static
WCHAR *
__cdecl
_new_tfullpath_helper(
	WCHAR * buf, 
	const WCHAR *path, 
	size_t sz,
	WCHAR ** pBuf
	)
{
	WCHAR* ret;
	errno_t save_errno = errno;
	errno = 0;
	ret = _wfullpath(buf, path, sz);

	if (ret) {
		errno = save_errno;
		return ret;
	}

	/* if _tfullpath fails because buf is too small, then we just call again _tfullpath and
	* have it allocate the appropriate buffer
	*/
	if (errno != ERANGE) {
		/* _tfullpath is failing for another reason, just propagate the failure and keep the
		* failure code in errno
		*/
		return NULL;
	}

	errno = save_errno;
	*pBuf = _wfullpath(NULL, path, 0);

	return *pBuf;
}

static
int
__cdecl
_new_IsRootUNCName(const WCHAR *path)
{
	/*
	* If a root UNC name, path will start with 2 (but not 3) slashes
	*/
	if ((wcslen(path) >= 5) /* minimum string is "//x/y" */
		&& ISSLASH(path[0]) && ISSLASH(path[1])
		&& !ISSLASH(path[2])) {
		const WCHAR * p = path + 2;

		/*
		* find the slash between the server name and share name
		*/
		while (*++p) {
			if (ISSLASH(*p)) {
				break;
			}
		}

		if (*p && p[1]) {
			/*
			* is there a further slash?
			*/
			while (*++p) {
				if (ISSLASH(*p)) {
					break;
				}
			}

			/*
			* just final slash (or no final slash)
			*/
			if (!*p || !p[1]) {
				return 1;
			}
		}
	}

	return 0;
}

#endif


FILE*
__stdcall 
_new_fopen(
	_In_ char* pszUtf8FileName, 
	_In_ const char *mode)
{
	FILE *hFile = NULL;
	if (pszUtf8FileName == NULL) {
		return NULL;
	}

#ifdef _MSC_VER

	LPWSTR pwszFileName = NULL;
	LPWSTR pwszMode = NULL;
	pwszFileName = Utf8ToUnicode(pszUtf8FileName);
	pwszMode = AnsiToUnicode(mode);
	if (pwszFileName == NULL || pwszMode == NULL) {
		goto L_Cleanup;
	}
	errno = _wfopen_s(&hFile, pwszFileName, pwszMode);

L_Cleanup:
	if (pwszFileName) {
		FreeStr(pwszFileName);
	}
	if (pwszMode) {
		FreeStr(pwszMode);
	}

#else
	hFile = fopen(pszUtf8FileName, mode);
#endif
	return hFile;
}

int
__stdcall 
_new_stat(
	_In_ char* pszUtf8FileName, 
	_In_ struct_stat *file)
{
	if (pszUtf8FileName == NULL) {
		return -1;
	}

#ifdef _MSC_VER

	int drive; // A: = 1, B: = 2, etc. 
	errno_t err = 0;
	LPWSTR pwszFileName = NULL;
	LARGE_INTEGER ilFileSize;
	WIN32_FIND_DATAW fileDataW;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	SYSTEMTIME SystemTime;
	FILETIME LocalFTime;
	
	pwszFileName = Utf8ToUnicode(pszUtf8FileName);
	if (pwszFileName == NULL) {
		errno = ENOENT;
		err = -1;
		goto L_Cleanup;
	}

	if (pwszFileName[1] == L':') {
		if (*pwszFileName && !pwszFileName[2]) {
			errno = ENOENT;
			return (-1);
		}
		drive = towlower(*pwszFileName) - _T('a') + 1;
	} else {
		drive = _new_getdrive();
	}

	hFile = FindFirstFileW(pwszFileName, &fileDataW);
	if (hFile == INVALID_HANDLE_VALUE) {
		errno = ENOENT;
		err = -1;
		goto L_Cleanup;
	}  

	if (fileDataW.ftLastWriteTime.dwLowDateTime ||
		fileDataW.ftLastWriteTime.dwHighDateTime) {

		if (!FileTimeToLocalFileTime(&fileDataW.ftLastWriteTime, &LocalFTime)
			|| !FileTimeToSystemTime(&LocalFTime, &SystemTime)) {
			errno = ENOENT;
			err = -1;
			goto L_Cleanup;
		}

#ifdef USE_WIN32_LARGE_FILES
		_systemTimeToTimet64(SystemTime, &file->st_mtime);
#else
		_systemTimeToTimet32(SystemTime, &file->st_mtime); 
#endif 

	} else {
		file->st_mtime = 0;
	}

	if (fileDataW.ftLastAccessTime.dwLowDateTime ||
		fileDataW.ftLastAccessTime.dwHighDateTime) {

		if (!FileTimeToLocalFileTime(&fileDataW.ftLastAccessTime, &LocalFTime)
			|| !FileTimeToSystemTime(&LocalFTime, &SystemTime)) {
			errno = ENOENT;
			err = -1;
			goto L_Cleanup;
		}

#ifdef USE_WIN32_LARGE_FILES
		_systemTimeToTimet64(SystemTime, &file->st_atime);
#else
		_systemTimeToTimet32(SystemTime, &file->st_atime);
#endif 

	} else {
		file->st_atime = file->st_mtime;
	}

	if (fileDataW.ftCreationTime.dwLowDateTime ||
		fileDataW.ftCreationTime.dwHighDateTime) {

		if (!FileTimeToLocalFileTime(&fileDataW.ftCreationTime, &LocalFTime)
			|| !FileTimeToSystemTime(&LocalFTime, &SystemTime)) {
			errno = ENOENT;
			err = -1;
			goto L_Cleanup;
		}
#ifdef USE_WIN32_LARGE_FILES
		_systemTimeToTimet64(SystemTime, &file->st_ctime);
#else
		_systemTimeToTimet32(SystemTime, &file->st_ctime);
#endif 
	} else {
		file->st_ctime = file->st_mtime;
	}

	file->st_nlink = 1;
	file->st_mode = _new_tdtoxmode(fileDataW.dwFileAttributes, pwszFileName);
	file->st_uid = file->st_gid = file->st_ino = 0;
	file->st_rdev = file->st_dev = (_dev_t)(drive - 1);

	ilFileSize.HighPart = fileDataW.nFileSizeHigh;
	ilFileSize.LowPart = fileDataW.nFileSizeLow;
	file->st_size = ilFileSize.QuadPart;

L_Cleanup:

	if (pwszFileName) {
		FreeStr(pwszFileName);
	}
	if (hFile != INVALID_HANDLE_VALUE) {
		FindClose(hFile);
	}
	return err;
#else
	return stat(pszUtf8FileName, file);
#endif
}
  	
