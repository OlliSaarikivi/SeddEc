#include "Pch.hpp"
#include "SeddEcException.hpp"

#define X(a) #a,
string reasonName[] = {
  EXCEPTION_REASON_TABLE
};
#undef X

SeddEcException::SeddEcException(Reason reason) : reason_(reason), what_(reasonName[reason]) {}
SeddEcException::SeddEcException(Reason reason, string message) : reason_(reason),
        what_(reasonName[reason] + ": " + message) {}