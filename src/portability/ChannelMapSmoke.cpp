#include "stdtypes.h"
#include <cstddef>
#include "cnlmaps.h"

#include <cmath>
#include <iostream>

namespace {

bool Expect(bool condition,const char *message)
{
    if( condition ) return true;
    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main()
{
    CChannelMap first(4);
    first.CreateLinOpen(2);
    first.Node(0).t = 0.0;
    first.Node(0).f = 10.0;
    first.Node(1).t = 2.0;
    first.Node(1).f = 14.0;

    CChannelMap copy(first);
    if( !Expect(first == copy && !(first != copy),
                "equal briefing channel maps no longer compare equal") ||
        !Expect(std::fabs(first.Phase(1.0)-12.0) < 1e-9,
                "linear briefing channel interpolation changed") ||
        !Expect(std::fabs(copy.Phase(1.0)-12.0) < 1e-9,
                "briefing channel copy lost interpolation flags") ) return 1;

    copy = copy;
    if( !Expect(first == copy,
                "briefing channel self-assignment corrupted its data") )
        return 1;

    copy.Node(1).f = 15.0;
    if( !Expect(first != copy && !(first == copy),
                "different briefing channel maps no longer compare different") )
        return 1;

    return 0;
}
