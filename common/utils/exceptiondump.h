#pragma once

#include <exception>
#include <string>

// Describe an in-flight exception for logging. On iOS this also recognizes
// Objective-C NSExceptions (name, reason and the throw-site backtrace —
// implemented in exceptiondump.mm); elsewhere it covers std::exception
// (exceptiondump.cpp). Returns an empty string only for a null pointer.
struct ExceptionDump {
  static std::string describe(std::exception_ptr p);
  };
