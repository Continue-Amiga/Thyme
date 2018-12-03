/**
 * @file
 *
 * @author OmniBlade
 *
 * @brief Class for handling strings that have a series of bytes as a code point.
 *
 * @copyright Thyme is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            2 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "asciistring.h"
#include "endiantype.h"
#include "gamedebug.h"
#include "unicodestring.h"
#include <cctype>
#include <cstdio>

Utf8String const Utf8String::s_emptyString(nullptr);

Utf8String::Utf8String() :
    m_data(nullptr)
{
}

Utf8String::Utf8String(const char *s) :
    m_data(nullptr)
{
    if ( s != nullptr ) {
        int len = (int)strlen(s);
        if ( len > 0 ) {
            Ensure_Unique_Buffer_Of_Size(len + 1, false, s);
        }
    }
}

Utf8String::Utf8String(Utf8String const &string) :
    m_data(string.m_data)
{
    if ( m_data != nullptr ) {
        m_data->Inc_Ref_Count();
    }
}

void Utf8String::Validate()
{
    //TODO, doesnt seem to be implimented anywhere? It is called though...
}

char *Utf8String::Peek() const
{
    DEBUG_ASSERT_PRINT(m_data != nullptr, "null string ptr");
    
    return m_data->Peek();
}


void Utf8String::Release_Buffer()
{
    if ( m_data != nullptr ) {
        m_data->Dec_Ref_Count();

        if ( m_data->ref_count == 0 ) {
            g_dynamicMemoryAllocator->Free_Bytes(m_data);
        }
        
        m_data = nullptr;
    }
}

void Utf8String::Ensure_Unique_Buffer_Of_Size(int chars_needed, bool keep_data, const char *str_to_cpy, const char *str_to_cat)
{
    if ( m_data != nullptr && m_data->ref_count == 1 && m_data->num_chars_allocated >= chars_needed ) {
        if ( str_to_cpy != nullptr ) {
            strcpy(Peek(), str_to_cpy);
        }

        if ( str_to_cat != nullptr ) {
            strcpy(Peek() + strlen(Peek()), str_to_cat);
        }

    } else {

        //this block would have been a macro like DEBUG_CRASH(numCharsNeeded + 8 > MAX_LEN, THROW_02); (see cl_debug.h)
        //if ( numCharsNeeded + 8 > MAX_LEN ) {
            // *&preserveData = 0xDEAD0002;
            //throw(&preserveData, &_TI1_AW4ErrorCode__);
        //}

        int size = g_dynamicMemoryAllocator->Get_Actual_Allocation_Size(chars_needed + sizeof(AsciiStringData));
        AsciiStringData *new_data = reinterpret_cast<AsciiStringData *>(g_dynamicMemoryAllocator->Allocate_Bytes_No_Zero(size));
        
        new_data->ref_count = 1;
        new_data->num_chars_allocated = size - sizeof(AsciiStringData);
        //new_data->num_chars_allocated = numCharsNeeded;
    #ifdef GAME_DEBUG_STRUCTS
        new_data->debug_ptr = new_data->Peek();
    #endif

        if ( m_data != nullptr && keep_data ) {
            strcpy(new_data->Peek(), Peek());
        } else {
            *new_data->Peek() = '\0';
        }

        if ( str_to_cpy != nullptr ) {
            strcpy(new_data->Peek(), str_to_cpy);
        }

        if ( str_to_cat != nullptr ) {
            strcat(new_data->Peek(), str_to_cat);
        }

        Release_Buffer();
        m_data = new_data;
    }
}

int Utf8String::Get_Length() const
{
    if ( m_data != nullptr ) {
        int len = strlen(Str());
        DEBUG_ASSERT_PRINT(len > 0, "length of string is less than or equal to 0.");
        
        return len;
    }
    
    return 0;
}

char Utf8String::Get_Char(int index) const
{
    DEBUG_ASSERT_PRINT(index >= 0, "bad index in getCharAt.");
    DEBUG_ASSERT_PRINT(Get_Length() > 0, "strlen returned less than or equal to 0 in getCharAt.");
    
    return Peek()[index];
}

const char *Utf8String::Str() const
{
    static char const TheNullChr[4] = "";

    if ( m_data != nullptr ) {
        return Peek();
    }
    
    return TheNullChr;
}

char *Utf8String::Get_Buffer_For_Read(int len)
{
    
    // Generate buffer sufficient to read requested size into.
    Ensure_Unique_Buffer_Of_Size(len + 1, 0, 0, 0);

    return Peek();
}

void Utf8String::Set(const char *s)
{
    if ( m_data == nullptr || s != m_data->Peek() ) {
        size_t len;
        
        if ( s && (len = strlen(s) + 1, len != 1) ) {
            Ensure_Unique_Buffer_Of_Size(len, false, s, nullptr);
        } else {
            Release_Buffer();
        }
    }
}

void Utf8String::Set(Utf8String const &string)
{
    if ( &string != this ) {
        Release_Buffer();
        m_data = string.m_data;
        
        if ( m_data != nullptr ) {
            m_data->Inc_Ref_Count();
        }
    }
}

void Utf8String::Translate(Utf16String const &string)
{
    Release_Buffer();
    
#if defined BUILD_WITH_ICU // Use ICU convertors
    int32_t length;
    UErrorCode error = U_ZERO_ERROR;
    u_strToUTF8(nullptr, 0, &length, string, -1, &error);

    if (U_SUCCESS(error) && length > 0) {
        u_strToUTF8(Get_Buffer_For_Read(length), length, nullptr, string, -1, &error);

        if (U_FAILURE(error)) {
            Clear();
        }
    }
#elif defined PLATFORM_WINDOWS // Use WIN32 API convertors.
    int length = WideCharToMultiByte(CP_UTF8, 0, string, -1, nullptr, 0, nullptr, nullptr);

    if (length > 0) {
        WideCharToMultiByte(CP_UTF8, 0, string, -1, Get_Buffer_For_Read(length), length, nullptr, nullptr);
    }
#else // Naive copy, this is what the original does.
    int str_len = string.Get_Length();

    for ( int i = 0; i < str_len; ++i ) {
        //This is a debug assert from the look of it.
        /*if ( v4 < 8 || (!stringSrc.m_data ? (v6 = 0) : (v5 = stringSrc.Peek(), v6 = wcslen(v5)), v3 >= v6) )
        {
            if ( `Utf16String::Get_Char'::`14'::allowCrash )
            {
                TheCurrentAllowCrashPtr = &`Utf16String::Get_Char'::`14'::allowCrash;
                DebugCrash(aBadIndexInGetch);
                TheCurrentAllowCrashPtr = 0;
            }
        }*/
        wchar_t c;

        if ( string.m_data != nullptr ) {
            c = string.Get_Char(i);
        } else {
            c = L'\0';
        }
        
        // null out the second byte so Concat only concatenates the first byte.
        // prevents issues if unicode string contains none ascii chars.
        // This will have endian issues on big endian.
        c &= 0xFF;

#ifdef SYSTEM_BIG_ENDIAN
        // Byte swap if on big endian, only care about least significant byte
        if (sizeof(wchar_t) == 2) {
            c = htole16(c);
        } else if (sizeof(wchar_t) == 4) {
            c = htole32(c);
        } else {
            DEBUG_ASSERT_PRINT(false, "wchar_t is not an expected size.\n");
        }
#endif

        Concat(reinterpret_cast<char *>(&c));
    }
#endif
}

void Utf8String::Concat(char c)
{
    char str[2];
 
    str[0] = c;
    str[1] = '\0';
    Concat(str);
}

void Utf8String::Concat(const char *s)
{
    int len = strlen(s);

    if ( len > 0 ) {
        if ( m_data != nullptr ) {
            Ensure_Unique_Buffer_Of_Size(strlen(Peek()) + len + 1, true, 0, s);
        } else {
            Set(s);
        }
    }
}

void Utf8String::Trim()
{
    // No string, no Trim.
    if ( m_data == nullptr ) {
        return;
    }

    char *str = Peek();

    // Find first none space in string if not the first.
    for ( char i = *str; i != '\0'; i = *(++str) ) {
        if ( !isspace(i) ) {
            break;
        }
    }

    // If we had some spaces and moved Str, Set string to that position.
    if ( str != Peek() ) {
        Set(str);
    }

    // Oops, Set call broke the string.
    if ( m_data == nullptr ) {
        return;
    }

    for ( int i = strlen(Peek()) - 1; i >= 0; --i ) {
        if ( !isspace(Get_Char(i)) ) {
            break;
        }

        Remove_Last_Char();
    }
}

void Utf8String::To_Lower()
{
    char buf[MAX_FORMAT_BUF_LEN];

    if ( m_data == nullptr ) {
        return;
    }

    strcpy(buf, Peek());

    for ( char *c = buf; *c != '\0'; ++c ) {
        *c = tolower(*c);
    }

    Set(buf);
}

/**
 * @brief Convert any windows path separators to posix ('\' to '/').
 */
Utf8String Utf8String::Posix_Path() const
{
    char buf[MAX_FORMAT_BUF_LEN];

    if ( m_data == nullptr ) {
        return s_emptyString;
    }

    strcpy(buf, Peek());

    for ( char *c = buf; *c != '\0'; ++c ) {
        if ( *c == '\\' ) {
            *c = '/';
        }
    }

    return buf;
}

/**
 * @brief Convert any posix path separators to windows ('/' to '\').
 */
Utf8String Utf8String::Windows_Path() const
{
    char buf[MAX_FORMAT_BUF_LEN];

    if (m_data == nullptr) {
        return s_emptyString;
    }

    strcpy(buf, Peek());

    for (char *c = buf; *c != '\0'; ++c) {
        if (*c == '/') {
            *c = '\\';
        }
    }

    return buf;
}

void Utf8String::Remove_Last_Char()
{
    if ( m_data == nullptr ) {
        return;
    }

    int len = strlen(Peek());

    if ( len > 0 ) {
        Ensure_Unique_Buffer_Of_Size(len + 1, true);
        Peek()[len - 1] = '\0';
    }
}

void Utf8String::Format(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    Format_VA(format, va);
}

void Utf8String::Format(Utf8String format, ...)
{
    va_list va;

    va_start(va, format);
    Format_VA(format, va);
}

void Utf8String::Format_VA(const char *format, va_list args)
{
    char buf[MAX_FORMAT_BUF_LEN];

    DEBUG_ASSERT_THROW(vsnprintf(buf, sizeof(buf), format, args) > 0, 0xDEAD0002, "Unable to format buffer");

    Set(buf);
}

void Utf8String::Format_VA(Utf8String &format, va_list args)
{
    char buf[MAX_FORMAT_BUF_LEN];

    DEBUG_ASSERT_THROW(vsnprintf(buf, sizeof(buf), format.Str(), args) > 0, 0xDEAD0002, "Unable to format buffer");

    Set(buf);
}

bool Utf8String::Starts_With(const char *p) const
{
    if ( *p == '\0' ) {
        return true;
    }
    
    int thislen = m_data != nullptr ? strlen(Peek()) : 0;
    int thatlen = strlen(p);
    
    if ( thislen < thatlen ) {
        return false;
    }
    
    return strncmp(Peek(), p, thatlen) == 0;
}

bool Utf8String::Ends_With(const char *p) const
{
    if ( *p == '\0' ) {
        return true;
    }
    
    int thislen = m_data != nullptr ? strlen(Peek()) : 0;
    int thatlen = strlen(p);
    
    if ( thislen < thatlen ) {
        return false;
    }
    
    return strncmp(Peek() + thislen - thatlen, p, thatlen) == 0;
}

bool Utf8String::Starts_With_No_Case(const char *p) const
{
    if ( *p == '\0' ) {
        return true;
    }
    
    int thislen = Get_Length();
    int thatlen = strlen(p);
    
    if ( thislen < thatlen ) {
        return false;
    }
    
    return strncasecmp(Peek(), p, thatlen) == 0;
}

bool Utf8String::Ends_With_No_Case(const char *p) const
{
    if ( *p == '\0' ) {
        return true;
    }
    
    int thislen = m_data != nullptr ? strlen(Peek()) : 0;
    int thatlen = strlen(p);
    
    if ( thislen < thatlen ) {
        return false;
    }
    
    return strncasecmp(Peek() + thislen - thatlen, p, thatlen) == 0;
}

bool Utf8String::Next_Token(Utf8String *tok, const char *delims)
{
    if ( m_data == nullptr ) {
        return false;
    }
    
    if ( *Peek() == '\0' || this == tok ) {
        return false;
    }
    
    //
    // If no separators provided, default to white space.
    //
    if ( delims == nullptr ) {
        delims = " \n\r\t";
    }
    
    char *start = Peek();
    
    //
    // Find next instance of token or end of string
    //
    for ( char c = *start; c != '\0'; c = *(++start) ) {
        if ( strchr(delims, c) == nullptr ) {
            break;
        }
    }
    
    if ( *start == '\0' ) {
        Release_Buffer();
        tok->Release_Buffer();
        
        return false;
    }
    
    char *end = start;
    
    //
    // Find next instance of token or end of string.
    //
    for ( char c = *end; c != '\0'; c = *(++end) ) {
        if ( strchr(delims, c) != nullptr ) {
            break;
        }
    }
    
    if ( end <= start ) {
        Release_Buffer();
        tok->Release_Buffer();
        
        return false;
    }
    
    //
    // Copy found region into provided Utf8String, then move this string
    // to start of next section.
    //
    char *tokstr = tok->Get_Buffer_For_Read(end - start + 1);
    memcpy(tokstr, start, end - start);
    tokstr[end - start] = '\0';
    Set(end);
    
    return true;
}

#ifdef GAME_DEBUG
void Utf8String::Debug_Ignore_Leaks()
{
    //TODO, doesnt seem to be implimented anywhere? It is called though...
}
#endif // GAME_DEBUG
