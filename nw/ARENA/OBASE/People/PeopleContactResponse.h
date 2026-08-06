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

// May 1999 ON_OBJ contact codes are persistent direction classes. Horizontal
// classes 1/3 retain the authored ten-degree detour but anchor it to the route
// bearing so a persistent contact cannot compound into an orbit. Support
// classes 9/11 remain current-heading relative, code 2 retains full speed, and
// code 4 uses the contact-derived heading at 80%.
bool PeopleContactResponse_Advance(
    const SPeopleContactResponseRequest &request,
    SPeopleContactResponseResult *result);

bool PeopleContactResponse_IsSupportedCode(int contactCode);
bool PeopleContactResponse_Probe();

#endif
