#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>

class PHSLogging
{
private:
   PHSLogging ();

   Poco::AutoPtr<Poco::Logger> rootLogger;

   Poco::AutoPtr<Poco::FileChannel> fileChannel;
   Poco::AutoPtr<Poco::PatternFormatter> formatter;
   Poco::AutoPtr<Poco::FormattingChannel> formattingChannel;

   static PHSLogging myLogging;
public:
   static void fatal (const std::string& message);
   static void critical (const std::string& message);
   static void error (const std::string& message);
   static void warning (const std::string& message);
   static void notice (const std::string& message);
   static void information (const std::string& message);
   static void debug (const std::string& message);
   static void trace (const std::string& message);
};
