#ifndef __AUTO_FUNC_LOG__
#define __AUTO_FUNC_LOG__

#define ENABLE_AUTO_LOG

#if defined(ENABLE_AUTO_LOG)
#define AUTO_LOGGING

#ifdef AUTO_LOGGING

#include "nsDebug.h"
#include "nsThreadUtils.h"

/*
 * Usage:
 * Put AFLOG() in the function. For example:
 *   void Foo() {
 *      AFLOG();
 *   }
 * Use FLLOG to print file and line no.
 * FLLOG("This is test %d", i));
 * Use LLOG to print line no.
 * LLOG("This is test %d", i));
 *--------------------------------------
 * Print log wiht thread id and name
 * Put TAFLOG() in the function. For example:
 *   void Foo() {
 *      TAFLOG();
 *   }
 * Use TFLLOG to print file and line no.
 * TFLLOG("This is test %d", i));
 * Use TLLOG to print line no.
 * TLLOG("This is test %d", i));
 *
 */

static const char* sIndentation= "  ";

static const char* GetThreadName() {
  nsIThread* currThread = NS_GetCurrentThread();
  if (currThread) {
    PRThread* result;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(currThread->GetPRThread(&result)));
    MOZ_ASSERT(result);
    return PR_GetThreadName(result);
  }
  return "NULL";
}

static PRUint32 GetThreadID() {
  nsIThread* currThread = NS_GetCurrentThread();
  if (currThread) {
    PRThread* result;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(currThread->GetPRThread(&result)));
    MOZ_ASSERT(result);
    return PR_GetThreadId(result);
  }
  return 0;
}

static void PrintIndentation(int num)
{
  for (int ix = 0; ix < num; ++ix) {
      printf_stderr("%s", sIndentation);
  }
}

#define PINDENT(x) PrintIndentation(x)

#define PRLLOG(...) do {\
                       printf_stderr("%s", sIndentation); \
                       printf_stderr(__VA_ARGS__); \
                       printf_stderr(" in line:%d, File: %s\n", __LINE__, __FILE__); \
                     } while (0)


#define AFLOG() AutoFuncLog autoFuncLog(__PRETTY_FUNCTION__, __FUNCTION__)
#define FLLOG(...) do {\
                       printf_stderr(__VA_ARGS__); \
                       printf_stderr(" in line:%d, File: %s\n", __LINE__, __FILE__); \
                     } while (0)
#define LLOG(...) do {\
                      printf_stderr(__VA_ARGS__); \
                      printf_stderr(" in line:%d\n", __LINE__); \
                    } while (0)

#define TAFLOG() ThreadAutoFuncLog threadAutoFuncLog(__PRETTY_FUNCTION__, __FUNCTION__)
#define TFLLOG(...) do {\
                       printf_stderr("%sIn thread %s(%02x): ",sIndentation, GetThreadName(), GetThreadID()); \
                       printf_stderr(__VA_ARGS__); \
                       printf_stderr(" in line:%d, File: %s\n", __LINE__, __FILE__); \
                     } while (0)
#define TLLOG(...) do {\
                      printf_stderr("%sIn thread %s(%02x): ",sIndentation, GetThreadName(), GetThreadID()); \
                      printf_stderr(__VA_ARGS__); \
                      printf_stderr(" in line:%d\n", __LINE__); \
                    } while (0)

class AutoFuncLog {
public:
    AutoFuncLog(char const* prettyFunction, char const* function)
    : mFunc(function),
      mPrettyFunc(prettyFunction)
    {
      printf_stderr("|%s|\n", mPrettyFunc);
    }
    ~AutoFuncLog()
    {
        printf_stderr("End |%s|\n", mFunc);
    }
private:
    char const* mFunc;
    char const* mPrettyFunc;
};

class ThreadAutoFuncLog {
public:
    ThreadAutoFuncLog(char const* prettyFunction, char const* function)
    : mFunc(function),
      mPrettyFunc(prettyFunction)
    {
      printf_stderr("In thread %s(%02x): |%s|\n", GetThreadName(), GetThreadID(), mPrettyFunc);
    }
    ~ThreadAutoFuncLog()
    {
        printf_stderr("End |%s|\n", mFunc);
    }
private:
    char const* mFunc;
    char const* mPrettyFunc;
};


#else
#define AFLOG()
#define FLLOG(Format_, ...)
#define LLOG(Format_, ...)

#define TAFLOG()
#define TFLLOG(Format_, ...)
#define TLLOG(Format_, ...)

#endif // AUTO_LOGGING

#endif // ENABLE_AUTO_LOG
#endif // __AUTO_FUNC_LOG__
