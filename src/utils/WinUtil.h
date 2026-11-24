#pragma once
#include <string>
#include <Windows.h>

namespace n4x::winutil {

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);
std::string ToLower(std::string s);
std::wstring GetModuleDir();
std::wstring GetExePath();
bool IsUserAdmin();
std::string LastErrorString(DWORD err);
std::string FormatBytes(uint64_t bytes);
std::string FormatPercent(double v);

} // namespace n4x::winutil
