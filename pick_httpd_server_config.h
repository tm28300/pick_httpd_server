#include <libconfig.h>
#include <vector>
#include <string>
#include <pcre.h>

// Types

struct url_config_struct {
   const char  *path;
   pcre        *pattern_comp;
   const char  *subr;
   int          method_length;
   const char **method;
   int          get_param_length;
   const char **get_param;
   struct url_config_struct *sub_path;
   struct url_config_struct *next;
};

// Globals variables

extern config_t config_pick_httpd_server;
extern std::string config_pick_account;
extern int config_http_port;
extern struct url_config_struct *first_url_config;

// Globals functions

extern bool phs_config_read ();
extern void phs_config_free ();
extern int extract_subroutine_name_from_url (const char *url, std::string &subr, std::vector<std::string> &method_authorized, std::vector<std::string> param_authorized);
