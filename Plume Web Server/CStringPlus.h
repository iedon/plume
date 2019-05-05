#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdarg.h>

class CStringPlus : public std::string {

	public:

		CStringPlus() {};
		~CStringPlus() {};
		CStringPlus(const std::string & str);
		CStringPlus(const char * str);
		CStringPlus(unsigned long long num);
		CStringPlus & format(const char *format, ...);
		void replace_all(const CStringPlus & toSearch, const CStringPlus & replaceStr);
		void split(std::vector<CStringPlus> & result, const CStringPlus & seperator);
		CStringPlus & ltrim();
		CStringPlus & rtrim();
		CStringPlus & trim();
		CStringPlus & to_upcase();
		CStringPlus & to_lowcase();

};
