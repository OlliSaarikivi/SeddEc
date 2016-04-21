#pragma once

#include <exception>

#define EXCEPTION_REASON_TABLE \
X(LOGIC_ERROR) \
X(INVALID_INPUT_FORMAT) 

#define X(a) a,
enum Reason {
  EXCEPTION_REASON_TABLE
};
#undef X

class SeddEcException : public std::exception {
public:
    SeddEcException(Reason reason);
    SeddEcException(Reason reason, string message);

    const char* what() const throw() override { return what_.c_str(); }
private:
    Reason reason_;
    string what_;
};