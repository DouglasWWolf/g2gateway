//=================================================================================================
// CUpdSpec.cpp - Implements a spec-file that can be updated
//=================================================================================================
#include "updspec.h"
#include <string.h>
#include <stdint.h>


//=================================================================================================
// Constructor() - Intializes important fields
//=================================================================================================
CUpdSpec::CUpdSpec()
{
    m_pre_save  = nullptr;
    m_post_save = nullptr;
}
//=================================================================================================



//=================================================================================================
// set_pre_post_save() - Allows the caller to declare functions that should be called immediately
//                       before and immediately after saving to a file
//=================================================================================================
void CUpdSpec::set_pre_post_save(vpvf pre, vpvf post)
{
    m_pre_save  = pre;
    m_post_save = post;
}
//=================================================================================================



//=================================================================================================
// configure_as_file() - Declare that this object represents a file in the file-system
//=================================================================================================
void CUpdSpec::configure_as_file(const char* filename)
{
    m_is_file = true;

    // Clear existing data from memory
    m_lines.clear();
    m_record_map.clear();

    // Save the filename for posterity
    m_filename = filename;

    // We only need these when we're an EEPROM
    m_header = "";
    m_eeprom_size = 0;
}
//=================================================================================================



//=================================================================================================
// configure_as_eeprom() - Declare that this spec-file is being stored in EEPROM
//=================================================================================================
void CUpdSpec::configure_as_eeprom(const char* filename, int eeprom_size, const char* header)
{
    m_is_file = false;

    // Clear existing data from memory
    m_lines.clear();
    m_record_map.clear();

    // Save the filename for posterity
    m_filename = filename;

    // Save the file header for posterity
    m_header = header;

    // Save the EEPROM size
    m_eeprom_size = eeprom_size;

    // If the header isn't empty, make sure it ends with a linefeed
    if (!m_header.is_empty() && strchr(m_header.c(), '\n') == nullptr) m_header += '\n';
}
//=================================================================================================



//=================================================================================================
// load() - Reads a spec-file from a file
//=================================================================================================
bool CUpdSpec::load()
{
    // Throw away an existing spec-file data we have in memory
    m_lines.clear();
    m_record_map.clear();

    // Open the input file
    FILE* ifile = fopen(m_filename, "rb");

    // If we can't, tell the caller
    if (ifile == nullptr) return false;

    // Figure out how much data we're going to read
    int file_length = m_eeprom_size;

    // If this isn't an EEPROM, the file itself will tell us how much data to read in
    if (file_length == 0)
    {
        fseek(ifile, 0, SEEK_END);
        file_length = ftell(ifile);
        rewind(ifile);
    }

    // Allocate memory space for the file (plus a nul-byte)
    char* data = new char[file_length + 1];

    // Read in the file data
    size_t bytes_read = fread(data, 1, file_length, ifile);

    // Make sure our data is null terminated
    data[bytes_read] = 0;

    // If we're supposed to look for a header, see if it's present
    if (m_header.is_empty())
    {
        read_from_buffer(data);
    }
    else
    {
        // Find out how many bytes long the header we're expecting is
        size_t header_length = m_header.length();

        // If the header is present in the data we just read, parse it
        if (memcmp(data, m_header.c(), header_length) == 0)
        {
            read_from_buffer(data + header_length);
        }
    }

    // We don't need the file data anymore
    delete[] data;

    // Tell the caller that all is well
    return true;
}
//=================================================================================================



//=================================================================================================
// read_from_buffer() - Reads in a spec-file from a block of memory
//
// Passed:  buffer = Pointer to the block of memory.  Must be terminated with a nul-byte
//=================================================================================================
void CUpdSpec::read_from_buffer(const char* buffer)
{
    char line[500];
    record_t new_record;
    bool eof = false;
    int  c;

    // Throw away an existing spec-file data we have in memory
    m_lines.clear();
    m_record_map.clear();

    // Get a pointer to the start of our incoming data
    const char* in = buffer;

next_line:

    // If we're pointing at the end of the file data, we're done
    if (eof) return;

    // We're going to parse a line of text from our input buffer
    char* out = line;

    // Fetch one line of text...
    while (true)
    {
        // Fetch the next character
        c = *in++;

        // If we've found the end of file, this is the end of the line
        if (c == 0) {eof=true; break;}

        // Drop carriage returns
        if (c == '\r') continue;

        // If we've found a linefeed, this is the end of the line
        if (c == '\n') break;

        // And write this character to the line
        *out++ = c;
    }

    // Terminate the line we just fetched with a nul-byte
    *out = 0;

    // Add it to our list
    m_lines.push_back(line);

    // Point to our line
    char* p = line;

    // Skip over any spaces or tabs
    while (*p == ' ' || *p == '\t') ++p;

    // If it's a blank line, ignore it
    if (*p == 0) goto next_line;

    // If the line starts with a '#', ignore it
    if (*p == '#') goto next_line;

    // If the line starts with '//', ignore it
    if (p[0] == '/' && p[1] == '/') goto next_line;

    // See if there's am equals sign on the line
    char* equal = strchr(p, '=');

    // If there's not, ignore this line
    if (equal == nullptr) goto next_line;

    // Replace the equal with a nul.  We now have two strings in 'line'
    *equal = 0;

    // Fetch the name of the spec
    PString name = p;

    // Fetch the value of the spec
    PString value = equal + 1;

    // Trim all the spaces off of both of them
    name.trim_all();
    value.trim_all();

    // We're going to treat names as case-insensitive
    name.make_upper();

    // Insert this new key/value pair into the record map
    new_record.index = m_lines.size() - 1;
    new_record.value = value;
    m_record_map[name] = new_record;

    // And go fetch and process the next input line
    goto next_line;
}
//=================================================================================================




//=================================================================================================
// get() - Looks up the value associated with a given key.  Returns true
//         if the key was found, and false if it was not
//=================================================================================================
bool CUpdSpec::get(PString key, PString* p_value)
{
    // Our search key is an upper-case version of our spec-name
    key.make_upper();

    // If the caller gave us a place to store the result, clear it
    if (*p_value) p_value->clear();

    // See if this is a key we know about
    auto it = m_record_map.find(key);

    // If this isn't a key we know about, tell the caller
    if (it == m_record_map.end()) return false;

    // Fill in the callers value field
    if (p_value) *p_value = it->second.value;

    // Tell the caller that we found the requested key
    return true;
}
//=================================================================================================


//=================================================================================================
// get() - Looks up the value associated with a given key.  Returns true
//         if the key was found, and false if it was not
//=================================================================================================
bool CUpdSpec::get(PString key, bool* p_value)
{
    PString str;

    // Fetch the value that goes with this key
    if (!get(key, &str)) return false;

    // Find out if the value is one of the strings that means "true"
    bool is_true = (str == "true" || str == "TRUE" || str == "True" || str == "1");

    // Fill in the caller's output value with the value associated with the key
    if (p_value) *p_value = is_true;

    // Tell the caller that this field existed
    return true;
}
//=================================================================================================


//=================================================================================================
// get() - Looks up the value associated with a given key.  Returns true
//         if the key was found, and false if it was not
//=================================================================================================
bool CUpdSpec::get(PString key, int32_t* p_value)
{
    PString str;

    // We guarantee that if this lookup fails, the output value will be zero
    if (p_value) *p_value = 0;

    // Fetch the string value of the specified key
    bool status = get(key, &str);

    // Fill in the callers field with the numeric version of the value
    if (p_value) *p_value = atoi(str);

    // Tell the caller whether or not the key existed
    return status;
}
//=================================================================================================



//=================================================================================================
// get() - Looks up the value associated with a given key.  Returns true
//         if the key was found, and false if it was not
//=================================================================================================
bool CUpdSpec::get(PString key, uint32_t* p_value)
{
    PString str;

    uint64_t value;

    // We guarantee that if this lookup fails, the output value will be zero
    if (p_value) *p_value = 0;

    // Fetch the string value of the specified key
    bool status = get(key, &str);

    // Fill in the callers field with the numeric version of the value
    if (p_value)
    {
        value = atoll(str);
        *p_value = value;
    }

    // Tell the caller whether or not the key existed
    return status;
}
//=================================================================================================




//=================================================================================================
// set() - Sets the value for a key/value pair
//=================================================================================================
bool CUpdSpec::set(PString name, PString value)
{
    int index;
    record_t new_record;

    // Create the line of text that represents this key value pair
    PString line = to_string("%s = %s", name.c(), value.c());

    // Our search key is an upper-case version of our spec-name
    PString key = name;
    key.make_upper();

    // See if this is a key we know about
    auto it = m_record_map.find(key);

    // Is this a key that we already know about?
    bool found = (it != m_record_map.end());

    // If we're updating an already existing key...
    if (found)
    {
        // Record the text that will be part of the file we write out
        m_lines[it->second.index] = line;

        // And update the value for this key
        it->second.value = value;

        // Tell the caller that he updated a previously existing key
        return true;
    }

    // Add this new line to our list of lines
    m_lines.push_back(line);

    // Find the index of the line we just added
    index = m_lines.size() - 1;

    // And this key/value entry to our map
    new_record.index = index;
    new_record.value = value;
    m_record_map[key]  = new_record;

    // Tell the caller that he inserted a new key
    return false;
}
//=================================================================================================


//=================================================================================================
// remove() - Deletes a key/value pair
//=================================================================================================
bool CUpdSpec::remove(PString name)
{
    int index;

    // Our search key is an upper-case version of our spec-name
    PString key = str_to_upper(name);

    // See if this is a key we know about
    auto it = m_record_map.find(key);

    // Is this a key that we already know about?
    bool found  = (it != m_record_map.end());

    // If this key doesn't exist, we're done
    if (!found) return false;

    // Find the index of the string we should update
    index = it->second.index;

    // Record the text that will be part of the file we write out
    m_lines[index] = "";

    // Remove this key/value pair from the record map
    m_record_map.erase(key);

    // Tell the caller that we found his key
    return true;
}
//=================================================================================================


//=================================================================================================
// set() - Sets the value for a key/value pair
//=================================================================================================
bool CUpdSpec::set(PString name, int value)
{
    return set(name, to_string("%i", value));
}
//=================================================================================================



//=================================================================================================
// buffer_size() - Returns the number of bytes required to store a string
//                 that contains every line of our spec-file
//=================================================================================================
int CUpdSpec::buffer_size()
{
    // It will take one byte to store the terminating nul
    int count = 1;

    // Account for the space occupied by the header
    count += m_header.length();

    // Loop through every line
    for (int i=0; i<m_lines.size(); ++i)
    {
        // Add in the number of bytes required for this line and the linefeed
        count += (m_lines[i].length() + 1);
    }

    // Tell the caller how many bytes will are required to store this file
    return count;
}
//=================================================================================================


//=================================================================================================
// write_to_buffer() - Write the text of our spec-file to a buffer
//=================================================================================================
void CUpdSpec::write_to_buffer(char* buffer)
{
    // Point to the buffer that we are going to fill in
    char* out = buffer;

    // Output the header
    int header_length = m_header.length();
    strcpy(out, m_header);
    out += header_length;

    // Loop through each line
    for (int i=0; i<m_lines.size(); ++i)
    {
        // Get a pointer to text of the line
        const char* in = m_lines[i];

        // Fetch this line of text
        while (*in) *out++ = *in++;

        // Terminate the line with a linefeed
        *out++ = '\n';
    }

    // And terminate the entire output string with a nul-byte
    *out = 0;
}
//=================================================================================================


//=================================================================================================
// save() - Saves the file to disk/EEPROM
//=================================================================================================
bool CUpdSpec::save()
{
    // If we have a routine to call before saving, do it
    if (m_pre_save) m_pre_save();

    // Figure out how big of a buffer we'll need
    int file_size = buffer_size();

    // Allocate the memory for our entire file
    char* buffer = new char[file_size];

    // Fetch our entire file as one long block of text
    write_to_buffer(buffer);

    // Try to create the output file
    FILE* ofile = fopen(m_filename, "wb");

    // If we were able to create it, write and close the file
    if (ofile)
    {
        fwrite(buffer, 1, file_size, ofile);
        fclose(ofile);
    }

    // Throw away the buffer, we're done with it
    delete[] buffer;

    // If we have a routine to call after saving, do it
    if (m_post_save) m_post_save();


    // And tell the caller whether we were able to write the file
    return (ofile != nullptr);
}
//=================================================================================================




