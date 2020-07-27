//=================================================================================================
// cpp_string.cpp - Implements a portable, powerful string class
//=================================================================================================
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdarg>
#include <string>
#include "cppstring.h"
using std::string;



//=================================================================================================
// replace() - Replaces all occurrences of "oldch" with "newch"
//=================================================================================================
void CPPString::replace(char oldch, char newch)
{
    if (this->is_empty()) return;
    char* p = &(this->at(0));
    while (*p)
    {
        if (*p == oldch) *p = newch;
        ++p;
    }
}
//=================================================================================================


//=================================================================================================
// make_upper() - Converts this string to upper-case
//=================================================================================================
void CPPString::make_upper()
{
    if (this->is_empty()) return;
    char* p = &(this->at(0));
    while (*p)
    {
        *p = toupper(*p);
        ++p;
    }
}
//=================================================================================================


//=================================================================================================
// make_lower() - Converts this string to lower-case
//=================================================================================================
void CPPString::make_lower()
{
    if (this->is_empty()) return;
    char* p = &(this->at(0));
    while (*p)
    {
        *p = tolower(*p);
        ++p;
    }
}
//=================================================================================================


//=================================================================================================
// left() - Return the left-most 'n' characters
//=================================================================================================
CPPString CPPString::left(int n)
{
    return substr(0, n);
}
//=================================================================================================


//=================================================================================================
// right() - Return the right-most 'n' characters
//=================================================================================================
CPPString CPPString::right(int n)
{
    // Find out how long our underlying string is
    int len = length();

    // If the caller wants more characters than exist, hand him
    // the entire source string
    if (n > len) return *this;

    // Return the right-most 'n' characters
    return substr(len - n);
}
//=================================================================================================


//=================================================================================================
// mid() - Returns a substring
//=================================================================================================
CPPString CPPString::mid(int start, size_t len)
{
    return substr(start, len);
}
//=================================================================================================


//=================================================================================================
// format() - Performs printf()-style formatting on a string
//=================================================================================================
void CPPString::format(CPPString fmt, ...)
{
    char buffer[4096];

    // This is a pointer to a variable argument list
    va_list first_arg;

    // Point to the first argument after the "fmt" parameter
    va_start(first_arg, fmt);

    // Perform a "printf" of our arguments into the "buffer" area.
    vsprintf(buffer, fmt.c_str(), first_arg);

    // Tell the system we're done with first_arg
    va_end(first_arg);

    // And store this buffer as our string contents
    assign(buffer);
}
//=================================================================================================


//=================================================================================================
// formatv() - Performs printf()-style formatting on a string
//=================================================================================================
void CPPString::formatv(CPPString fmt, va_list argptr)
{
    char buffer[4096];

    // Perform a "printf" of our arguments into the "buffer" area.
    vsprintf(buffer, fmt.c_str(), argptr);

    // And store this buffer as our string contents
    assign(buffer);
}
//=================================================================================================


//=================================================================================================
// trim_left() - Trims the leftmost whitespace from a string
//=================================================================================================
void CPPString::trim_left(const char* ws)
{
    // Find the first character that's not a whitespace
    const size_t pos = find_first_not_of(ws);
    if (pos == string::npos)
        clear();
    else
        assign(substr(pos));
}
//=================================================================================================



//=================================================================================================
// trim_right() - Trims the right-most whitespace from a string
//=================================================================================================
void CPPString::trim_right(const char* ws)
{
    // Find the last character that's not a whitespace
    const size_t pos = find_last_not_of(ws);
    if (pos == string::npos)
        clear();
    else
        assign(substr(0, pos+1));
}
//=================================================================================================


//=================================================================================================
// trim_all() - Trims leading and trailing white-space from a string
//=================================================================================================
void CPPString::trim_all(const char* ws)
{
    // Find the first character that's not a whitespace
    const size_t first = find_first_not_of(ws);

    // If there wasn't any non-white space, the result is empty
    if (first == string::npos)
    {
        clear();
        return;
    }

    // Find the last character that's not a whitespace
    const size_t last = find_last_not_of(" \t");

    // If there were no trailing spaces, return just the first part
    if (last == string::npos)
    {
        assign(substr(first));
                return;
    }

    // If we get here, there were both leading and trailing spaces
    assign(substr(first, last-first+1));
}
//=================================================================================================


//=================================================================================================
// to_signed() - Convert the string to a signed number
//=================================================================================================
s32 CPPString::to_signed()
{
    // Get a pointer to the characters of the string
    const char* in = c_str();

    // Skip over leading spaces and tabs
    while (*in == ' ' || *in == '\t') ++in;

    // Figure out if this number is decimal or hexadecimal
    int base = (in[0] == '0' && (in[1] == 'x' || in[1] == 'X')) ? 16 : 10;

    // And convert it to binary
    return strtol(in, NULL, base);
}
//=================================================================================================


//=================================================================================================
// to_unsigned() - Convert the string to an unsigned number
//=================================================================================================
u32 CPPString::to_unsigned()
{
    // Get a pointer to the characters of the string
    const char* in = c_str();

    // Skip over leading spaces and tabs
    while (*in == ' ' || *in == '\t') ++in;

    // Figure out if this number is decimal or hexadecimal
    int base = (in[0] == '0' && (in[1] == 'x' || in[1] == 'X')) ? 16 : 10;

    // And convert it to binary
    return strtoul(in, NULL, base);
}
//=================================================================================================


//=================================================================================================
// to_string() - Helper functions that creates a CPPString from printf()-style
//               formatting parameters
//=================================================================================================
CPPString to_string(CPPString fmt, ...)
{
    char buffer[8000];

    // This is a pointer to a variable argument list
    va_list first_arg;

    // Point to the first argument after the "fmt" parameter
    va_start(first_arg, fmt);

    // Perform a "printf" of our arguments into the "buffer" area.
    vsprintf(buffer, fmt.c_str(), first_arg);

    // Tell the system we're done with first_arg
    va_end(first_arg);

    // Hand our formatted string to the caller
    return buffer;
}
//=================================================================================================



//=================================================================================================
// str_to_upper() - Constructs a CPPString and converts to upper case
//=================================================================================================
CPPString str_to_upper(const CPPString& s)
{
    CPPString ret = s;
    ret.make_upper();
    return ret;
}
//=================================================================================================



//=================================================================================================
// str_to_lower() - Constructs a CPPString and converts to lower case
//=================================================================================================
CPPString str_to_lower(const CPPString& s)
{
    CPPString ret = s;
    ret.make_lower();
    return ret;
}
//=================================================================================================


//=================================================================================================
// str_trim_left() - Constructs a CPPString and trims left spaces
//=================================================================================================
CPPString str_trim_left(const CPPString& s)
{
    CPPString ret = s;
    ret.trim_left();
    return ret;
}
//=================================================================================================


//=================================================================================================
// str_trim_right() - Constructs a CPPString and trims right spaces
//=================================================================================================
CPPString str_trim_right(const CPPString& s)
{
    CPPString ret = s;
    ret.trim_right();
    return ret;
}
//=================================================================================================


//=================================================================================================
// str_trim_all() - Constructs a CPPString and trims all spaces
//=================================================================================================
CPPString str_trim_all(const CPPString& s)
{
    CPPString ret = s;
    ret.trim_all();
    return ret;
}
//=================================================================================================



//=================================================================================================
// chomp() - Removes cr/lf from the end of a line
//=================================================================================================
char* chomp(char* in)
{
    char*   p;
    p = strchr(in, 10); if (p) *p = 0;
    p = strchr(in, 13); if (p) *p = 0;
    return in;
}
//=================================================================================================


