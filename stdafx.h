// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include "../SDK/foobar2000.h"
#include "../helpers/helpers.h"

#include <windowsx.h>
#include <string>
#include <vector>
#include <ctime>
#include <Magick++.h>
#include <sstream>

std::string wstrtostr(const std::wstring &wstr);

// Convert an UTF8 string to a wide Unicode String
std::wstring strtowstr(const std::string &str);
