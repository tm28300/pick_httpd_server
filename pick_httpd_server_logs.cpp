#include <string>

// Pour getuid
#include <unistd.h>
#include <sys/types.h>
#include <cstdlib>

#ifdef PHS_DEBUG
#include <iostream>
#endif


#include <Poco/Util/HelpFormatter.h>
#include <sys/stat.h>

#include "pick_httpd_server_logs.h"

using namespace Poco;

PHSLogging PHSLogging::myLogging;

PHSLogging::PHSLogging ()
{
   // Déterminer le chemin du fichier de log selon l'utilisateur
   std::string logPath;
   if (getuid() == 0) {
      // root
      logPath = "/var/log/pick_httpd_server.log";
   } else {
      const char* home = getenv("HOME");
      if (home && home[0] != '\0') {
         std::string logDir = std::string(home) + "/log";
         struct stat st = {0};
         if (stat(logDir.c_str(), &st) == -1) {
            mkdir(logDir.c_str(), 0700);
         }
         logPath = logDir + "/pickhttpd_server.log";
      } else {
         // fallback : log dans le répertoire courant
         logPath = "pickhttpd_server.log";
      }
   }

   // Créer un canal de fichier avec rotation journalière
   fileChannel = new FileChannel;
   fileChannel->setProperty ("path", logPath);
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
#ifdef PHS_DEBUG
   std::cerr << "Log fatal: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->fatal (message);
   }
}

void PHSLogging::critical (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log critical: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->critical(message);
   }
}

void PHSLogging::error (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log error: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->error(message);
   }
}

void PHSLogging::warning (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log warning: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->warning(message);
   }
}

void PHSLogging::notice (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log notice: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->notice(message);
   }
}

void PHSLogging::information (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log information: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->information(message);
   }
}

void PHSLogging::debug (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log debug: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->debug(message);
   }
}

void PHSLogging::trace (const std::string& message)
{
#ifdef PHS_DEBUG
   std::cerr << "Log trace: \"" << message << "\"" << std::endl;
#endif
   if (myLogging.rootLogger) {
      myLogging.rootLogger->trace(message);
   }
}
