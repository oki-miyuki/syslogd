#pragma once

#include <string>
#include <string.h>
#include <iostream>

typedef std::basic_string<TCHAR> tstring;

template <typename T>
size_t stlen( const T* val ) { return 0; }
template <>
size_t stlen<char>( const char* val ) { return strlen( val ); }
template <>
size_t stlen<wchar_t>( const wchar_t* val ) { return wcslen( val ); }

template <typename T>
int sticmp( const T* lhs, const T* rhs ) { return 1; }
template <>
int sticmp<char>( const char* lhs, const char* rhs ) { return _stricmp( lhs, rhs ); }
template <>
int sticmp<wchar_t>( const wchar_t* lhs, const wchar_t* rhs ) {	return _wcsicmp( lhs, rhs ); }

#ifdef _UNICODE
#define tout std::wcout
#define terr std::wcerr
#else
#define tout std::cout
#define terr std::cerr
#endif