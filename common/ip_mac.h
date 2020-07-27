#pragma once

//=================================================================================================
// These define an IP address and a MAC address
//=================================================================================================
struct sMAC
{
    uint8_t octet[6];
    void    from(const char* p);
    PString to_string();
    bool operator==(const sMAC& other) const;
    bool operator!=(const sMAC& other) const;
};

struct sIP
{
    uint8_t  octet[4];
    void     from(const char* p);
    PString  to_string();
    uint32_t to_int()
    {
        return octet[0] << 24 | octet[1] << 16 | octet[2] << 8 | octet[3];
    }
    bool operator==(const sIP& other) const;
    bool operator!=(const sIP& other) const;
};
//=================================================================================================

