#pragma once
#include <C:\RML\Source\morph_dict\contrib\plog\plog/Record.h>
#include <C:\RML\Source\morph_dict\contrib\plog\plog/Util.h>

namespace plog
{
    class PLOG_LINKAGE IAppender
    {
    public:
        virtual ~IAppender()
        {
        }

        virtual void write(const Record& record) = 0;
    };
}
