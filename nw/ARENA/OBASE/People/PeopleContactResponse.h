#ifndef RR2NW_PEOPLE_CONTACT_RESPONSE_H
#define RR2NW_PEOPLE_CONTACT_RESPONSE_H

struct SPeopleContactResponseRequest
{
    int contactCode;
    double heading;
    double contactHeading;
    double rollSpeed;
    double deltaTime;
};

struct SPeopleContactResponseResult
{
    int active;
    double targetHeading;
    double angularSpeed;
    double heading;
};

// May 1999 ON_OBJ contact codes are persistent direction classes. Codes 1/9
// turn ten degrees clockwise at 80% roll speed, code 2 turns ten degrees
// counter-clockwise at full speed, codes 3/11 turn counter-clockwise at 80%,
// and code 4 uses the contact-derived heading at 80%.
bool PeopleContactResponse_Advance(
    const SPeopleContactResponseRequest &request,
    SPeopleContactResponseResult *result);

bool PeopleContactResponse_IsSupportedCode(int contactCode);
bool PeopleContactResponse_Probe();

#endif
