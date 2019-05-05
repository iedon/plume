#define _CRT_SECURE_NO_WARNINGS
#include "CStringPlus.h"
#include <algorithm>   
#include <functional>

CStringPlus::CStringPlus(const std::string & str)
{
	this->append(str);
}

CStringPlus::CStringPlus(const char * str)
{
	this->append(str);
}

CStringPlus & CStringPlus::format(const char *format, ...)
{
	va_list arg_list;
	va_start(arg_list, format);
		int needed = vsnprintf(NULL, 0, format, arg_list) + 1;
	va_end(arg_list);
	char *p = new char[needed + 1];
	va_start(arg_list, format);
		vsnprintf(p, needed, format, arg_list);
	va_end(arg_list);
	*this = p;
	delete[]p;
	return *this;
}

void CStringPlus::replace_all(const CStringPlus & toSearch, const CStringPlus & replaceStr)
{
	size_t pos = this->find(toSearch);
	while (pos != CStringPlus::npos)
	{
		this->replace(pos, toSearch.size(), replaceStr);
		pos = this->find(toSearch, pos + toSearch.size());
	}
}

void CStringPlus::split(std::vector<CStringPlus> & result, const CStringPlus & seperator)
{
	CStringPlus::size_type pos1, pos2;
	pos2 = this->find(seperator);
	pos1 = 0;
	while (CStringPlus::npos != pos2)
	{
		result.push_back(this->substr(pos1, pos2 - pos1));

		pos1 = pos2 + seperator.size();
		pos2 = this->find(seperator, pos1);
	}
	if (pos1 != this->length())
		result.push_back(this->substr(pos1));
}

CStringPlus::CStringPlus(unsigned long long num)
{
	this->format("%llu", num);
}

CStringPlus & CStringPlus::ltrim()
{
	CStringPlus::iterator p = std::find_if(this->begin(), this->end(), std::not1(std::ptr_fun<int, int>(isspace)));
	this->erase(this->begin(), p);
	return *this;
}

CStringPlus & CStringPlus::rtrim()
{
	CStringPlus::reverse_iterator p = std::find_if(this->rbegin(), this->rend(), std::not1(std::ptr_fun<int, int>(isspace)));
	this->erase(p.base(), this->end());
	return *this;
}

CStringPlus & CStringPlus::trim()
{
	rtrim();
	ltrim();
	return *this;
}

CStringPlus & CStringPlus::to_upcase()
{
	std::transform(this->begin(), this->end(), this->begin(), toupper);
	return *this;
}

CStringPlus & CStringPlus::to_lowcase()
{
	std::transform(this->begin(), this->end(), this->begin(), tolower);
	return *this;
}
