/* 
*/ 

#include "pick_httpd_server.h"
#include "pick_httpd_server_config.h"
#include "pick_httpd_server_connection.h"

// Function

int main ()
{
   config_init (&config_pick_httpd_server);
   if (!phs_config_read ()) {
      phs_config_free ();
      config_destroy (&config_pick_httpd_server);
      return 2;
   }

   struct MHD_Daemon *daemon;

   daemon = MHD_start_daemon (MHD_USE_INTERNAL_POLLING_THREAD,
                              config_http_port,
                              NULL,                        // apc (check client)
                              NULL,                        // apc_cls
                              &pick_connection::pick_to_connection,       // dh (handler for all url)
                              NULL,                        // dh_cls
                              MHD_OPTION_NOTIFY_COMPLETED,
                              &pick_connection::request_completed,          // Cleanup when completed
                              NULL,
                              MHD_OPTION_END);
   if (daemon == NULL) {
      phs_config_free ();
      return 1;
   }

   getchar ();

   MHD_stop_daemon (daemon);
   config_destroy (&config_pick_httpd_server);
   phs_config_free ();
   return 0;
}
