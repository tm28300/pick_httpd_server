#include <string>

#include <Poco/Util/HelpFormatter.h>

#include "pick_httpd_server_logs.h"

using namespace Poco;

PHSLogging PHSLogging::myLogging;

PHSLogging::PHSLogging ()
{
   // Créer un canal de fichier avec rotation journalière
   fileChannel = new FileChannel;
   fileChannel->setProperty ("path", "/var/log/pick_httpd_server.log");
   fileChannel->setProperty ("rotation", "daily");
   fileChannel->setProperty ("archive", "timestamp");
   fileChannel->setProperty ("purgeCount", "14"); // Garder les logs des 14 derniers jours

   // Créer un formateur et un canal de formatage
   AutoPtr<PatternFormatter> formatter = new PatternFormatter ("%Y-%m-%d %H:%M:%S [%p] %t");
   AutoPtr<FormattingChannel> formattingChannel = new FormattingChannel (formatter, fileChannel);

   // Obtenir le logger racine et ajouter le canal
   rootLogger = &Logger::root ();
   rootLogger->setChannel (formattingChannel);
   rootLogger->setLevel ("warning");
}

void PHSLogging::fatal (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->fatal (message);
   }
}

void PHSLogging::critical (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->critical(message);
   }
}

void PHSLogging::error (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->error(message);
   }
}

void PHSLogging::warning (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->warning(message);
   }
}

void PHSLogging::notice (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->notice(message);
   }
}

void PHSLogging::information (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->information(message);
   }
}

void PHSLogging::debug (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->debug(message);
   }
}

void PHSLogging::trace (const std::string& message)
{
   if (myLogging.rootLogger) {
      myLogging.rootLogger->trace(message);
   }
}
