#pragma once

CStringPlus		GetLastErrorMessage(DWORD error_code = ::GetLastError());
LPCSTR			GetHttpDefaultStatusCodeDesc(USHORT usStatusCode);
CStringPlus		GenerateHttpErrorPage(USHORT usStatusCode, const char *custom_err_mg = nullptr);
CStringPlus		GetMIMETypeByExtension(const CStringPlus & ext);
CStringPlus		GetExtensionByPath(const CStringPlus & path);
CStringPlus		FilterPath(const CStringPlus & path);
void			GetHttpGMTTime(char *lpszBuffer, const LPSYSTEMTIME st = nullptr);
bool			ParseRangeByte(const CStringPlus & range, CStringPlus & start, CStringPlus & end);
