//=============================================================================
// CPPString.h - Defines a portable, powerful string class
//
// Copyright (c) 2014 Cepheid Corporation
//=============================================================================
#pragma once
#include <string>
#include <cstdarg>
#include <stdlib.h>
#include "typedefs.h"


//=============================================================================
// class CPPString - Defines a portable string class
//=============================================================================
class CPPString : public std::string
{
public:

    // Default constructor just calls the base-class
    CPPString() : std::string() {};

    // Constructor to create a CPPString from a const char*
    CPPString(const char* p) : std::string()
    {
        this->assign(p);
    }

    // Constructor to create a CPP string from a std::string
    CPPString(const std::string s) : std::string()
    {
        this->assign(s);
    }

    // Convert this string to upper-case
    void        make_upper();

    // Convert this string to lower-case
    void        make_lower();

    // Return the left-most 'n' characters
    CPPString   left(int n);

    // Return the right-most 'n' characters
    CPPString   right(int n);

    // Return a substring
    CPPString   mid(int start, size_t len = std::string::npos);

    // Replace all occurrences of 'oldch' with 'newch'
    void        replace(char oldch, char newch);

    // Perform printf()-style formatting on a string
    void        format(CPPString fmt, ...);

    // Perform printf()-style formatting on a string
    void        formatv(CPPString fmt, va_list argptr);

    // Trims whitespace from a string
    void        trim_left (const char* whitespace = " ");
    void        trim_right(const char* whitespace = " ");
    void        trim_all  (const char* whitespace = " ");

    // Convert a string to a binary value
    s32         to_signed();
    u32         to_unsigned();
    double      to_double() {return atof(c_str());}

    // Synonyms for c_str();
    const char* c() {return this->c_str();}
    operator const char*() const   {return this->c_str();}

    // A synonym for empty()
    bool is_empty() {return empty();}

protected:

    // Skips over leading spaces and tabs and returns a pointer to the first
    // character that is not a leading zero
    char* first_digit();
};
//=============================================================================


//=============================================================================
// Helper functions
//=============================================================================
CPPString to_string     (CPPString, ...);
CPPString str_to_upper  (const CPPString&);
CPPString str_to_lower  (const CPPString&);
CPPString str_trim_left (const CPPString&);
CPPString str_trim_right(const CPPString&);
CPPString str_trim_all  (const CPPString&);
char* chomp(char*);
//=============================================================================


typedef CPPString PString;
