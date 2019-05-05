#include "header.h"
#include "general.h"

CStringPlus GetLastErrorMessage(DWORD error_code)
{
	char *tmp = (char *)malloc(4096);
	::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code, NULL, tmp, 4096, NULL);
	CStringPlus ret(tmp);
	free(tmp);
	return ret;
}

LPCSTR GetHttpDefaultStatusCodeDesc(USHORT usStatusCode)
{
	switch(usStatusCode)
	{
		case HSC_CONTINUE						: return "Continue";
		case HSC_SWITCHING_PROTOCOLS			: return "Switching Protocols";
		case HSC_PROCESSING						: return "Processing";
		case HSC_OK								: return "OK";
		case HSC_CREATED						: return "Created";
		case HSC_ACCEPTED						: return "Accepted";
		case HSC_NON_AUTHORITATIVE_INFORMATION	: return "Non-Authoritative Information";
		case HSC_NO_CONTENT						: return "No Content";
		case HSC_RESET_CONTENT					: return "Reset Content";
		case HSC_PARTIAL_CONTENT				: return "Partial Content";
		case HSC_MULTI_STATUS					: return "Multi-Status";
		case HSC_ALREADY_REPORTED				: return "Already Reported";
		case HSC_IM_USED						: return "IM Used";
		case HSC_MULTIPLE_CHOICES				: return "Multiple Choices";
		case HSC_MOVED_PERMANENTLY				: return "Moved Permanently";
		case HSC_MOVED_TEMPORARILY				: return "Found"; // SEE HTTP SPEC
		case HSC_SEE_OTHER						: return "See Other";
		case HSC_NOT_MODIFIED					: return "Not Modified";
		case HSC_USE_PROXY						: return "Use Proxy";
		case HSC_SWITCH_PROXY					: return "Switch Proxy";
		case HSC_TEMPORARY_REDIRECT				: return "Temporary Redirect";
		case HSC_PERMANENT_REDIRECT				: return "Permanent Redirect";
		case HSC_BAD_REQUEST					: return "Bad Request";
		case HSC_UNAUTHORIZED					: return "Unauthorized";
		case HSC_PAYMENT_REQUIRED				: return "Payment Required";
		case HSC_FORBIDDEN						: return "Forbidden";
		case HSC_NOT_FOUND						: return "Not Found";
		case HSC_METHOD_NOT_ALLOWED				: return "Method Not Allowed";
		case HSC_NOT_ACCEPTABLE					: return "Not Acceptable";
		case HSC_PROXY_AUTHENTICATION_REQUIRED	: return "Proxy Authentication Required";
		case HSC_REQUEST_TIMEOUT				: return "Request Timeout";
		case HSC_CONFLICT						: return "Conflict";
		case HSC_GONE							: return "Gone";
		case HSC_LENGTH_REQUIRED				: return "Length Required";
		case HSC_PRECONDITION_FAILED			: return "Precondition Failed";
		case HSC_REQUEST_ENTITY_TOO_LARGE		: return "Request Entity Too Large";
		case HSC_REQUEST_URI_TOO_LONG			: return "Request-URI Too Long";
		case HSC_UNSUPPORTED_MEDIA_TYPE			: return "Unsupported Media Type";
		case HSC_REQUESTED_RANGE_NOT_SATISFIABLE: return "Requested Range Not Satisfiable";
		case HSC_EXPECTATION_FAILED				: return "Expectation Failed";
		case HSC_MISDIRECTED_REQUEST			: return "Misdirected Request";
		case HSC_UNPROCESSABLE_ENTITY			: return "Unprocessable Entity";
		case HSC_LOCKED							: return "Locked";
		case HSC_FAILED_DEPENDENCY				: return "Failed Dependency";
		case HSC_UNORDERED_COLLECTION			: return "Unordered Collection";
		case HSC_UPGRADE_REQUIRED				: return "Upgrade Required";
		case HSC_PRECONDITION_REQUIRED			: return "Precondition Required";
		case HSC_TOO_MANY_REQUESTS				: return "Too Many Requests";
		case HSC_REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
		case HSC_UNAVAILABLE_FOR_LEGAL_REASONS	: return "Unavailable For Legal Reasons";
		case HSC_RETRY_WITH						: return "Retry With";
		case HSC_INTERNAL_SERVER_ERROR			: return "Internal Server Error";
		case HSC_NOT_IMPLEMENTED				: return "Not Implemented";
		case HSC_BAD_GATEWAY					: return "Bad Gateway";
		case HSC_SERVICE_UNAVAILABLE			: return "Service Unavailable";
		case HSC_GATEWAY_TIMEOUT				: return "Gateway Timeout";
		case HSC_HTTP_VERSION_NOT_SUPPORTED		: return "HTTP Version Not Supported";
		case HSC_VARIANT_ALSO_NEGOTIATES		: return "Variant Also Negotiates";
		case HSC_INSUFFICIENT_STORAGE			: return "Insufficient Storage";
		case HSC_LOOP_DETECTED					: return "Loop Detected";
		case HSC_BANDWIDTH_LIMIT_EXCEEDED		: return "Bandwidth Limit Exceeded";
		case HSC_NOT_EXTENDED					: return "Not Extended";
		case HSC_NETWORK_AUTHENTICATION_REQUIRED: return "Network Authentication Required";
		case HSC_UNPARSEABLE_RESPONSE_HEADERS	: return "Unparseable Response Headers";
		default									: return "Unknown";
	}
}

CStringPlus GenerateHttpErrorPage(USHORT usStatusCode, const char *custom_err_mg)
{
	const char *desc = nullptr;
	switch(usStatusCode)
	{
		case HSC_BAD_REQUEST					: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf\xe6\x8f\x90\xe4\xba\xa4\xe4\xba\x86\xe4\xb8\x80\xe4\xb8\xaa\xe9\x94\x99\xe8\xaf\xaf\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82</p><p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe6\x9c\x89\xe8\xaf\xaf\xe6\x88\x96\xe8\xaf\xa5\xe5\x9f\x9f\xe5\x90\x8d\xe4\xb8\x8b\xe7\x9a\x84\xe7\xab\x99\xe7\x82\xb9\xe5\xb0\x9a\xe6\x9c\xaa\xe8\xbf\x9b\xe8\xa1\x8c\xe9\x85\x8d\xe7\xbd\xae\xef\xbc\x8c\xe8\xaf\xb7\xe7\xa8\x8d\xe5\x90\x8e\xe5\x86\x8d\xe8\xaf\x95\xe3\x80\x82</p>"; break;
		case HSC_UNAUTHORIZED					: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xba\xab\xe4\xbb\xbd\xe6\x9c\xaa\xe9\xaa\x8c\xe8\xaf\x81\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe6\x8f\x90\xe4\xbe\x9b\xe6\x82\xa8\xe7\x9a\x84\xe5\x87\xad\xe6\x8d\xae\xe6\x9d\xa5\xe7\x99\xbb\xe5\xbd\x95\xe6\xad\xa4\xe7\xab\x99\xe7\x82\xb9\xe3\x80\x82</p>"; break;
		case HSC_FORBIDDEN						: desc = "<p>\xe6\x82\xa8\xe5\xbd\x93\xe5\x89\x8d\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe8\xb5\x84\xe6\xba\x90\xe5\xb7\xb2\xe8\xa2\xab\xe7\xa6\x81\xe6\xad\xa2\xe8\xae\xbf\xe9\x97\xae\xe3\x80\x82</p><p>\xe6\x82\xa8\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe8\xb5\x84\xe6\xba\x90\xe5\xb7\xb2\xe7\xbb\x8f\xe8\xa2\xab\xe8\xae\xbe\xe7\xbd\xae\xe7\xa6\x81\xe6\xad\xa2\xe8\xae\xbf\xe9\x97\xae\xe6\x88\x96\xe8\x80\x85\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe6\xb2\xa1\xe6\x9c\x89\xe8\xb6\xb3\xe5\xa4\x9f\xe7\x9a\x84\xe8\xaf\xbb\xe5\x86\x99\xe6\x9d\x83\xe9\x99\x90\xe3\x80\x82</p>"; break;
		case HSC_NOT_FOUND						: desc = "<p>\xe6\x82\xa8\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe8\xb5\x84\xe6\xba\x90\xe5\x9c\xa8\xe6\x9c\xac\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x8a\xe6\x97\xa0\xe6\xb3\x95\xe6\x89\xbe\xe5\x88\xb0\xe3\x80\x82</p><p>\xe6\x82\xa8\xe6\x89\x80\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe8\xb5\x84\xe6\xba\x90\xe5\x8f\xaf\xe8\x83\xbd\xe5\xb7\xb2\xe7\xbb\x8f\xe8\xa2\xab\xe7\xa7\xbb\xe9\x99\xa4\xe3\x80\x81\xe6\x9b\xb4\xe5\x90\x8d\xe6\x88\x96\xe6\x98\xaf\xe6\x9a\x82\xe6\x97\xb6\xe8\xa2\xab\xe7\xa6\x81\xe7\x94\xa8\xe3\x80\x82</p>"; break;
		case HSC_METHOD_NOT_ALLOWED				: desc = "<p>\xe6\xad\xa4\xe6\x93\x8d\xe4\xbd\x9c\xe5\x9c\xa8\xe6\x9c\xac\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x8a\xe4\xb8\x8d\xe8\xa2\xab\xe5\x85\x81\xe8\xae\xb8\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe6\x96\xb9\xe6\xb3\x95\xe3\x80\x82</p>"; break;
		case HSC_REQUEST_ENTITY_TOO_LARGE		: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe6\xad\xa3\xe6\x96\x87\xe5\x86\x85\xe5\xae\xb9\xe8\xb6\x85\xe8\xbf\x87\xe4\xba\x86\xe9\x99\x90\xe5\x88\xb6\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\xbb\xa7\xe7\xbb\xad\xe5\xa4\x84\xe7\x90\x86\xe3\x80\x82</p><p>\xe5\xbb\xba\xe8\xae\xae\xe6\x82\xa8\xe7\xbc\xa9\xe7\x9f\xad\xe8\xaf\xb7\xe6\xb1\x82\xe5\x86\x85\xe5\xae\xb9\xe6\x88\x96\xe8\x80\x85\xe8\xaf\xb7\xe8\x81\x94\xe7\xb3\xbb\xe7\xab\x99\xe7\x82\xb9\xe7\xae\xa1\xe7\x90\x86\xe5\x91\x98\xe5\xa2\x9e\xe5\xa4\xa7\xe9\x99\x90\xe5\x88\xb6\xe9\x98\x88\xe5\x80\xbc\xe3\x80\x82</p>"; break;
		case HSC_REQUEST_URI_TOO_LONG			: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe8\xa1\x8c\xe8\xbf\x87\xe9\x95\xbf\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe7\xbc\xa9\xe7\x9f\xad\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe5\x90\x8e\xe5\x86\x8d\xe6\xac\xa1\xe6\x8f\x90\xe4\xba\xa4\xe3\x80\x82</p>"; break;
		case HSC_INTERNAL_SERVER_ERROR			: desc = "<p>\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x8f\x91\xe7\x94\x9f\xe4\xba\x86\xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\xbb\xa7\xe7\xbb\xad\xe6\x8f\x90\xe4\xbe\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82</p><p>\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe8\xbf\x90\xe8\xa1\x8c\xe9\x81\x87\xe5\x88\xb0\xe4\xba\x86\xe9\x97\xae\xe9\xa2\x98\xe6\x88\x96\xe6\x98\xaf\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe6\xad\xa3\xe7\xa1\xae\xe3\x80\x82</p>"; break;
		case HSC_NOT_IMPLEMENTED				: desc = "<p>\xe6\x9c\xac\xe8\xaf\xb7\xe6\xb1\x82\xe6\x89\x80\xe9\x9c\x80\xe8\xa6\x81\xe7\x9a\x84\xe5\xa4\x84\xe7\x90\x86\xe5\x9c\xa8\xe6\xad\xa4\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x8a\xe8\xbf\x98\xe6\xb2\xa1\xe6\x9c\x89\xe5\xae\x9e\xe7\x8e\xb0\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe6\x96\xb9\xe6\xb3\x95\xe5\xb9\xb6\xe7\xa8\x8d\xe5\x90\x8e\xe5\x86\x8d\xe8\xaf\x95\xe3\x80\x82</p>"; break;
		case HSC_BAD_GATEWAY					: desc = "<p>\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe7\xbd\x91\xe5\x85\xb3\xe5\x8f\x91\xe7\x94\x9f\xe4\xba\x86\xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\xbb\xa7\xe7\xbb\xad\xe6\x8f\x90\xe4\xbe\x9b\xe6\x9c\x8d\xe5\x8a\xa1\xe3\x80\x82</p><p>\xe7\xbd\x91\xe5\x85\xb3\xe6\x88\x96\xe4\xb8\x8a\xe6\xb8\xb8\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe9\x81\x87\xe5\x88\xb0\xe4\xba\x86\xe9\x97\xae\xe9\xa2\x98\xe6\x88\x96\xe6\x98\xaf\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe6\xad\xa3\xe7\xa1\xae\xe3\x80\x82</p>"; break;
		case HSC_SERVICE_UNAVAILABLE			: desc = "<p>\xe6\x82\xa8\xe6\x9a\x82\xe6\x97\xb6\xe6\x97\xa0\xe6\xb3\x95\xe8\xae\xbf\xe9\x97\xae\xe6\xad\xa4\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe3\x80\x82</p><p>\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x8f\xaf\xe8\x83\xbd\xe9\x81\x87\xe5\x88\xb0\xe4\xba\x86\xe4\xb8\xb4\xe6\x97\xb6\xe9\x97\xae\xe9\xa2\x98\xe6\x88\x96\xe8\x80\x85\xe9\x9c\x80\xe8\xa6\x81\xe7\xbb\xb4\xe6\x8a\xa4\xe3\x80\x82</p>"; break;
		case HSC_GATEWAY_TIMEOUT				: desc = "<p>\xe6\x82\xa8\xe6\x9a\x82\xe6\x97\xb6\xe6\x97\xa0\xe6\xb3\x95\xe8\xae\xbf\xe9\x97\xae\xe8\xbf\x99\xe4\xb8\xaa\xe9\xa1\xb5\xe9\x9d\xa2\xe3\x80\x82</p><p>\xe5\xb7\xb2\xe7\xbb\x8f\xe8\xb6\x85\xe8\xbf\x87\xe4\xba\x86\xe9\xa2\x84\xe8\xae\xbe\xe7\x9a\x84\xe7\xbd\x91\xe5\x85\xb3\xe7\xad\x89\xe5\xbe\x85\xe6\x97\xb6\xe9\x97\xb4\xef\xbc\x8c\xe4\xbd\x86\xe6\x98\xaf\xe7\xbd\x91\xe5\x85\xb3\xe4\xbe\x9d\xe7\x84\xb6\xe6\xb2\xa1\xe6\x9c\x89\xe4\xbc\xa0\xe5\x9b\x9e\xe6\x95\xb0\xe6\x8d\xae\xe3\x80\x82</p>"; break;
		case HSC_REQUESTED_RANGE_NOT_SATISFIABLE: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe6\x95\xb0\xe6\x8d\xae\xe6\xae\xb5\xe8\x8c\x83\xe5\x9b\xb4\xe4\xb8\x8d\xe6\xad\xa3\xe7\xa1\xae\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\xbb\xa7\xe7\xbb\xad\xe5\xa4\x84\xe7\x90\x86\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe7\xa1\xae\xe8\xae\xa4\xe8\xb5\x84\xe6\xba\x90\xe5\xa4\xa7\xe5\xb0\x8f\xe5\xb9\xb6\xe6\xa3\x80\xe6\x9f\xa5\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82</p>"; break;
		case HSC_HTTP_VERSION_NOT_SUPPORTED		: desc = "<p>\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81\xe6\x82\xa8\xe8\xaf\xb7\xe6\xb1\x82\xe7\x9a\x84\xe5\x8d\x8f\xe8\xae\xae\xe3\x80\x82</p><p>\xe8\xaf\xb7\xe6\x9b\xb4\xe6\x8d\xa2\xe4\xb8\x8e\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x80\xe8\x87\xb4\xe7\x9a\x84\xe5\x8d\x8f\xe8\xae\xae\xe7\x89\x88\xe6\x9c\xac\xe5\xb9\xb6\xe7\xa8\x8d\xe5\x90\x8e\xe5\x86\x8d\xe8\xaf\x95\xe3\x80\x82</p>"; break;
		default									: desc = "<p>\xe6\x82\xa8\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe5\xbd\x93\xe5\x89\x8d\xe6\x97\xa0\xe6\xb3\x95\xe8\xa2\xab\xe5\xa4\x84\xe7\x90\x86\xef\xbc\x8c\xe8\xaf\xb7\xe7\xa8\x8d\xe5\x90\x8e\xe5\x86\x8d\xe8\xaf\x95\xe3\x80\x82</p>"; break;
	}

	CStringPlus page;

	CStringPlus status;
	status.format("%d - %s", usStatusCode, GetHttpDefaultStatusCodeDesc(usStatusCode));

	const char * HTML_ERROR_PAGE = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><title>%s</title><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><style><!-- body{padding-top:30px;font-family:\"Microsoft Yahei\",\"Helvetica Neue\",Helvetica,Arial,sans-serif;font-size:14px;line-height:1.428571429;color:#333;background-color:#fff}.navbar-default{background-color:#f8f8f8;border-color:#e7e7e7}.navbar-fixed-top,.navbar-fixed-bottom{border-radius:0}.navbar-fixed-top,.navbar-fixed-bottom{position:fixed;right:0;left:0;z-index:1030;top:0}.navbar{min-height:50px;margin-bottom:20px;border:1px solid transparent}.container{padding-right:15px;padding-left:15px;margin-right:auto;margin-left:auto}.navbar-default .navbar-brand{color:#777}.navbar>.container .navbar-brand{margin-left:-20px}.navbar-brand{float:left;padding:15px 15px;font-size:18px;line-height:20px}a{color:#428bca;text-decoration:none;}.post p,.post pre{padding-left:10px}h2,.h2{font-size:28px}h1,h2,h3{margin-top:20px;margin-bottom:10px}footer{border-top:1px solid #eee;padding-left:15px}code{padding:2px 4px;font-size:90%%;color:#c7254e;background-color:#f9f2f4;border-radius:4px}.codeblue{padding:2px 4px;font-size:90%%;color:#2096F3;background-color:#f2f5f9;border-radius:4px} --></style></head><body><header class=\"navbar navbar-default navbar-fixed-top\"><div class=\"container\"><div class=\"navbar-header\"><a class=\"navbar-brand\">\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe9\x94\x99\xe8\xaf\xaf</a></div></div></header><div class=\"container\"><div class=\"post\"><div class=\"page-header\"><h2><code>%s</code></h2></div>%s</div></div><footer><p>%s <code class=\"codeblue\">%s</code></p></footer></body></html>";
	page.format(HTML_ERROR_PAGE, status.c_str(), status.c_str(), custom_err_mg != nullptr ? custom_err_mg : desc, APP_FULL_NAME, APP_VER);
	return page;
}

CStringPlus GetMIMETypeByExtension(const CStringPlus & ext)
{
	if (ext == "/") return "text/html";

	for (auto & mt : VAR_MIME_TYPES) {
		
		if (mt.ext == ext)
			return mt.res_type;
	}

	return "application/octet-stream";
}

CStringPlus GetExtensionByPath(const CStringPlus & path)
{
	size_t pos = path.rfind('.');
	if (pos == CStringPlus::npos || pos == path.length()) return "html";
	return path.substr(pos + 1);
}

CStringPlus FilterPath(const CStringPlus & path)
{
	CStringPlus ret(path);
	while (ret.find("//") != CStringPlus::npos) {
		ret.replace_all("//", "/");
	}
	while (ret.find("../") != CStringPlus::npos) {
		ret.replace_all("../", "");
	}
	while (ret.find("./") != CStringPlus::npos) {
		ret.replace_all("./", "");
	}

	DWORD length = ::SYS_GuessUrlDecodeBound((const BYTE *)ret.c_str(), (DWORD)ret.length()) + 1;
	BYTE *decoded = (BYTE *)malloc(length);
	memset(decoded, 0, length);
	
	if (::SYS_UrlDecode((BYTE *)ret.c_str(), (DWORD)ret.length(), decoded, &length) != 0) {
		
		// 如果不幸URL解码失败，则绕过解码，直接返回
		free(decoded);
		return ret;
	}

	int uni_dest_length = length * 2 + 2; // 因为 WCHAR 一个长度相当于2个内存字节
	WCHAR * unicode = (WCHAR *)malloc(uni_dest_length);
	memset(unicode, 0, uni_dest_length);
		
	if (!::SYS_Utf8ToUnicode((const char *)decoded, unicode, &uni_dest_length)) {
		
		free(decoded);
		free(unicode);
		return ret;
	}

	free(decoded);

	int iDestLength = uni_dest_length * 2 + 1; // 因为上述操作后，uni_dest_length 已经被改写为 unicode 字符串长度，而不是内存长度了
	char *ansi_text = (char *)malloc(iDestLength);
	memset(ansi_text, 0, iDestLength);
	if (!::SYS_UnicodeToCodePage(CP_ACP, (const WCHAR *)unicode, ansi_text, &iDestLength)) {

		free(ansi_text);
		free(unicode);
		return ret;
	}

	free(unicode);

	ret = ansi_text;

	free(ansi_text);

	return ret;
}

// buffer 大小应该大于等于 30(INTERNET_RFC1123_BUFSIZE).
void GetHttpGMTTime(char *lpszBuffer, const LPSYSTEMTIME st)
{
	static const char gmt_wkday[7][4] =
	{ { 'S','u','n', 0 },{ 'M','o','n', 0 },{ 'T','u','e', 0 },{ 'W','e','d', 0 },
	{ 'T','h','u', 0 },{ 'F','r','i', 0 },{ 'S','a','t', 0 } };

	static const char gmt_month[12][4] =
	{ { 'J','a','n', 0 },{ 'F','e','b', 0 },{ 'M','a','r', 0 },{ 'A','p','r', 0 },
	{ 'M','a','y', 0 },{ 'J','u','n', 0 },{ 'J','u','l', 0 },{ 'A','u','g', 0 },
	{ 'S','e','p', 0 },{ 'O','c','t', 0 },{ 'N','o','v', 0 },{ 'D','e','c', 0 } };

	SYSTEMTIME *systime = st;
	
	if (st == nullptr) {

		systime = (LPSYSTEMTIME)malloc(sizeof(SYSTEMTIME));
		::GetSystemTime(systime);
	}

	sprintf(lpszBuffer, "%s, %02d %s %4d %02d:%02d:%02d GMT", gmt_wkday[systime->wDayOfWeek], systime->wDay, gmt_month[systime->wMonth - 1], systime->wYear, systime->wHour, systime->wMinute, systime->wSecond);
	
	if (st == nullptr) free(systime);
}

// 初步断点续传位置解析, start 和 end 可能有为 “-” 的情况，函数返回后调用方还需仔细判断
bool ParseRangeByte(const CStringPlus & range, CStringPlus & start, CStringPlus & end)
{
	size_t pos = range.find("bytes=");
	if (pos == CStringPlus::npos) return false;

	CStringPlus bytes = range.substr(pos + 6 /* bytes= */);
	size_t split_pos = bytes.find("-");
	if(split_pos == CStringPlus::npos) return false;

	start = bytes.substr(0, split_pos + 1);

	// 如果符号在末尾（即符号位置为长度减一）
	if (split_pos == bytes.length() - 1) {

		end = "-";

	} else {
	
		end = bytes.substr(split_pos + 1);
	}

	return true;
}
