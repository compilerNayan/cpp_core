#ifndef STANDARDEFINES_H
#define STANDARDEFINES_H

#include <string>
#include <optional>
#include <ArduinoJson.h>

// Container type aliases (Std prefix; separate files so they can be included on need basis and avoid clashes with <vector> etc.)
#include "StdVector"
#include "StdList"
#include "StdDeque"
#include "StdSet"
#include "StdUnorderedSet"
#include "StdArray"
#include "StdMap"
#include "StdUnorderedMap"
#include "StdQueue"
#include "StdStack"

// Arduino/ESP32 compatible integer types
typedef int Int;
typedef const int CInt;
typedef unsigned int UInt;
typedef const unsigned int CUInt;
typedef long Long;
typedef const long CLong;
typedef unsigned long ULong;
typedef const unsigned long CULong;
typedef unsigned char UInt8;

// Character types
typedef char Char;
typedef const char CChar;
typedef unsigned char UChar;
typedef const unsigned char CUChar;

// Boolean type
typedef bool Bool;
typedef const bool CBool;

// Size type
typedef size_t Size;
typedef const size_t CSize;

// Pointer types
typedef void* VoidPtr;
typedef const void* CVoidPtr;
typedef void Void;

// String types
typedef std::string StdString;
typedef const std::string CStdString;
using std::optional;

#define Var auto
#define Val const auto
#define Const constexpr auto

#define Private public: private:
#define Protected public: protected:
#define Public private: public:
#define Static static
#define Virtual virtual
#define Explicit explicit
#define NoDiscard [[nodiscard]]

// Standard pointers
#include <memory>
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

using std::make_shared;

#define DefineStandardPointers(class_name) \
    class class_name; \
    typedef const class_name C##class_name; \
    typedef shared_ptr<class_name> class_name##SPtr; \
    typedef const shared_ptr<class_name> C##class_name##SPtr; \
    typedef weak_ptr<class_name> class_name##WPtr; \
    typedef const weak_ptr<class_name> C##class_name##WPtr; \
    typedef shared_ptr<class_name> class_name##Ptr; \
    typedef const shared_ptr<class_name> C##class_name##Ptr; \
    typedef unique_ptr<class_name> class_name##UPtr; \
    typedef const unique_ptr<class_name> C##class_name##UPtr;

#define make_ptr std::make_shared

#define DefineStandardTypes(enum_name) \
    enum class enum_name; \
    typedef const enum_name C##enum_name;

// These annotations are used by the preprocessing scripts
// They are written as //@AnnotationName in source files
// After processing, they become /*@AnnotationName*/ to be ignored

// Template declaration for Implementation
template <class T>
struct Implementation;

#endif // STANDARDEFINES_H