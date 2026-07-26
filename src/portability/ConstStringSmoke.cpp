#include "cnststr.h"

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
    str_ConstStr values("  -42 3.5 token");
    int integerValue = 0;
    double realValue = 0.0;
    int position = values.trim(0);
    position = values.to(integerValue,position);
    position = values.trim(position);
    position = values.to(realValue,position);
    position = values.trim(position);

    long expressionValue = 0;
    str_ConstStr expression("2 + 3 * 4");
    const int expressionEnd = expression.eval(expressionValue,0);

    if( !Expect(integerValue == -42,
                "signed console integer parsing changed") ||
        !Expect(std::fabs(realValue-3.5) < 1e-9,
                "console floating-point parsing changed") ||
        !Expect(values.get("token",position) == values.length(),
                "console token matching changed") ||
        !Expect(expressionValue == 14 &&
                    expression.trim(expressionEnd) == expression.length(),
                "console expression precedence changed") ) return 1;

    return 0;
}
