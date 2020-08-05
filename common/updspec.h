#pragma once
#include <vector>
#include <map>
#include <stdint.h>
#include "cppstring.h"

typedef void (*vpvf)();

//=================================================================================================
// CUpdSpec - An updateable spec-file class
//=================================================================================================
class CUpdSpec
{
public:

     // Constructor
     CUpdSpec();

     // Call this once if this object represents a file in the file-system
     void   configure_as_file(const char* filename);

     // Call this once if we're storing this file in an EEPROM
     void   configure_as_eeprom(const char* filename, int eeprom_size, const char* header = "");

     // Define what routines should be called just before and just after we save
     void   set_pre_post_save(vpvf pre, vpvf post);

     // Read in the spec-file from a file
     bool   load();

     // Save the spec-file to a file
     bool   save();

     // Fetch the name of the file
     const char* filename() {return m_filename;}

     // Fetch a spec as a string
     bool   get(PString spec, PString* value = nullptr);

     // Fetch a spec as a boolean
     bool   get(PString spec, bool* value = nullptr);

     // Fetch a spec as an integer
     bool   get(PString spec, int32_t*  value);
     bool   get(PString spec, uint32_t* value);


     // Set a spec to a specified value
     bool   set(PString spec, PString value);
     bool   set(PString spec, int value);

     // Removes a spec entirely
     bool   remove(PString spec);


protected:

     // Read in the spec-file from a buffer
     void       read_from_buffer(const char* buffer);

     // Write the text of the specfile to a buffer
     void       write_to_buffer(char* buffer);

     // The size of the buffer required to store the current spec-file
     int        buffer_size();

     // This will be true if w
     bool       m_is_file;

     // If this is an eeprom, this will tell us the size
     int        m_eeprom_size;

     // This is the filename we will read/write
     PString    m_filename;

     // This is the header that tells us this is one of our files
     PString    m_header;

     struct record_t {int index; PString value;};

     // This holds the lines of text that comprise our spec file
     std::vector<PString>        m_lines;

     // Maps a spec-name to a record that contains key's index and value
     std::map<PString, record_t> m_record_map;

     // These are pointers to the functions that will be called pre/post saving the file
     void   (*m_pre_save)();
     void   (*m_post_save)();
};
//=================================================================================================
