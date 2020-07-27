//=================================================================================================
// chcp.h - Defines a CHCP listener
//=================================================================================================
#pragma once
#include "cthread.h"


class CCHCP : public CThread
{
public:

    // When this thread spawns, this is the entry ppoint
    void  main(void* p1, void* p2, void* p3);

};
