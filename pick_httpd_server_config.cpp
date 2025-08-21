#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pour getuid
#include <unistd.h>
#include <sys/types.h>
#include <cstdlib>

#include <Poco/UTF8Encoding.h>
#include <Poco/MacRomanEncoding.h>

#include "pick_httpd_server.h"
#include "pick_httpd_server_config.h"

// Declarations

static void print_memory_full ();
static void free_url_config (struct url_config_struct *url_config);
static const char *check_pick_object_name (const char* object_name);
static struct url_config_struct *read_url_config (config_setting_t *config_url_elem);

// Constants

static const char config_path_pick_account [] = "pick.account";
static const char config_path_pick_encoding [] = "pick.encoding";
static const char config_path_httpd_port [] = "httpd.port";
static const unsigned char pattern_object_name [] = "^[[:alpha:]][[:alnum:]._-]*$";

// Globals variables

config_t config_pick_httpd_server;
std::string config_pick_account;
Poco::TextEncoding *config_pick_encoding = NULL;

int config_http_port;
struct url_config_struct *first_url_config = NULL;

// Functions

void print_memory_full ()
{
   fprintf (stderr, "Memory full when read configuration\n");
}

void free_url_config (struct url_config_struct *url_config)
{
   if (url_config->pattern_comp != NULL) {
      pcre2_code_free (url_config->pattern_comp);
   }
   if (url_config->method != NULL) {
      free (url_config->method);
   }
   if (url_config->get_param != NULL) {
      free (url_config->get_param);
   }
   struct url_config_struct *sub_path_config = url_config->sub_path;
   while (sub_path_config != NULL) {
      struct url_config_struct *next_config = sub_path_config->next;
      free_url_config (sub_path_config);
      sub_path_config = next_config;
   }
   free (url_config);
}

const char *check_pick_object_name (const char* object_name)
{
   pcre2_code *reg_exp;
   int prce_status;
   int error;
   long unsigned int erroffset;
   static const size_t error_message_detail_length = 255;

   reg_exp = pcre2_compile (pattern_object_name, PCRE2_ZERO_TERMINATED, 0 /* options */, &error, &erroffset, NULL) ;
   if (reg_exp == NULL) {
      char *error_message_detail = (char*) malloc (error_message_detail_length);

      if (error_message_detail == NULL) {
         return "Memory full";
      }
      snprintf (error_message_detail, error_message_detail_length, "Object name PCRE compilation failed at offset %lu: %d", erroffset, error);
      return error_message_detail;
   }
   pcre2_match_data *match_data = pcre2_match_data_create_from_pattern (reg_exp, NULL);
   prce_status = pcre2_match (reg_exp, (const unsigned char *) object_name, PCRE2_ZERO_TERMINATED, 0 /* startoffset */, 0 /* options */, match_data, NULL);
   pcre2_match_data_free (match_data);
   pcre2_code_free (reg_exp);
   if (prce_status < 0) {
      if (prce_status == PCRE2_ERROR_NOMATCH) {
         return "Invalid object name";
      }

      char *error_message_detail = (char*) malloc (error_message_detail_length);

      if (error_message_detail == NULL) {
         return "Memory full";
      }
      snprintf (error_message_detail, error_message_detail_length, "Object name PCRE match failed with error %d", prce_status);
      return error_message_detail;
   }
   return NULL;
}

struct url_config_struct * read_url_config (config_setting_t *config_url_elem)
{
   struct url_config_struct *new_url_config;

   new_url_config = (struct url_config_struct *) malloc (sizeof (struct url_config_struct));
   if (new_url_config == NULL) {
      print_memory_full ();
      return NULL;
   }
   new_url_config->path = NULL;
   new_url_config->pattern_comp = NULL;
   new_url_config->subr = NULL;
   new_url_config->method_length = -1;
   new_url_config->method = NULL;
   new_url_config->get_param_length = -1;
   new_url_config->get_param = NULL;
   new_url_config->sub_path = NULL;
   new_url_config->next = NULL;

   bool error_config = false;

   // strings
   const char *pattern_string = NULL;

   config_setting_lookup_string (config_url_elem, "path", &new_url_config->path);
   config_setting_lookup_string (config_url_elem, "pattern", &pattern_string);
   config_setting_lookup_string (config_url_elem, "subr", &new_url_config->subr);


   if ((new_url_config->path == NULL) == (pattern_string == NULL)) {
      fprintf (stderr, "Either path (%s) or pattern (%s) must be present\n", new_url_config->path, pattern_string);
      error_config = true;
   }
#ifdef PHS_DEBUG
   printf ("Find path=%s, pattern=%s\n", new_url_config->path, pattern_string);
#endif

   if (pattern_string != NULL) {
      int error;
      long unsigned int erroffset;

      new_url_config->pattern_comp = pcre2_compile ((const unsigned char *) pattern_string, PCRE2_ZERO_TERMINATED, 0 /* options */, &error, &erroffset, NULL);
      if (new_url_config->pattern_comp == NULL) {
         fprintf (stderr, "PCRE compilation failed for pattern \"%s\" at offset %lu: %d\n", pattern_string, erroffset, error);
         error_config = true;
      }
   }

   // Check subr name
   if (new_url_config->subr != NULL) {
      const char *subr_name_error_message = check_pick_object_name (new_url_config->subr);
      if (subr_name_error_message != NULL) {
         fprintf (stderr, "Subroutine name (%s) error: %s\n", new_url_config->subr, subr_name_error_message);
         // TODO free (subr_name_error_message); dans certains cas, utiliser C++ et les exceptions
         error_config = true;
      }
   }

   // method
   config_setting_t *config_url_method = config_setting_get_member (config_url_elem, "method");
   if (config_url_method != NULL) {
      if (config_setting_is_array (config_url_method) == CONFIG_FALSE) {
         fprintf (stderr, "method isn't a array\n");
         error_config = true;
      }
      else {
         new_url_config->method_length = (int) config_setting_length (config_url_method);
         if (new_url_config->method_length) {
            new_url_config->method = (const char**) malloc (sizeof (const char **) * new_url_config->method_length);
            if (new_url_config->method == NULL) {
               print_memory_full ();
               error_config = true;
            }
            else {
               for (int method_index = 0 ; method_index < new_url_config->method_length ; ++method_index) {
                  const char *method_elem = config_setting_get_string_elem (config_url_method, method_index);
                  if (method_elem == NULL) {
                     fprintf (stderr, "error reading method %d\n", method_index);
                     error_config = true;
                  }
                  else if (strcasecmp (method_elem, "GET") != 0 && strcasecmp (method_elem, "POST") != 0 && strcasecmp (method_elem, "PUT") != 0 && strcasecmp (method_elem, "PATCH") != 0 && strcasecmp (method_elem, "DELETE") && strcasecmp (method_elem, "OPTIONS")) {
                     fprintf (stderr, "unknown method %d\n", method_index);
                     error_config = true;
                     method_elem = NULL;
                  }
                  new_url_config->method [method_index] = method_elem;
               }
            }
         }
      }
   }

   // get_param
   config_setting_t *config_url_get_param = config_setting_get_member (config_url_elem, "get_param");
   if (config_url_get_param != NULL) {
      if (config_setting_is_array (config_url_get_param) == CONFIG_FALSE) {
         fprintf (stderr, "get_param isn't a array\n");
         error_config = true;
      }
      else {
         new_url_config->get_param_length = (int) config_setting_length (config_url_get_param);
         if (new_url_config->get_param_length) {
            new_url_config->get_param = (const char**) malloc (sizeof (const char **) * new_url_config->get_param_length);
            if (new_url_config->get_param == NULL) {
               print_memory_full ();
               error_config = true;
            }
            else {
               for (int get_param_index = 0 ; get_param_index < new_url_config->get_param_length ; ++get_param_index) {
                  const char *get_param_elem = config_setting_get_string_elem (config_url_get_param, get_param_index);
                  if (get_param_elem == NULL) {
                     fprintf (stderr, "error reading get_param %d\n", get_param_index);
                     error_config = true;
                  }
                  new_url_config->get_param [get_param_index] = get_param_elem;
               }
            }
         }
      }
   }

   // sub_path
   config_setting_t *config_url_sub_path = config_setting_get_member (config_url_elem, "sub_path");
   if (config_url_sub_path != NULL) {
      if (config_setting_is_list (config_url_sub_path) == CONFIG_FALSE) {
         fprintf (stderr, "sub_path isn't a list\n");
         error_config = true;
      }
      else {
         unsigned int sub_path_length = config_setting_length (config_url_sub_path);
#ifdef PHS_DEBUG
         printf ("Find %u sub_path\n", sub_path_length);
#endif
         if (sub_path_length) {
            struct url_config_struct *prev_sub_path_config = NULL;
            for (unsigned int sub_path_index = 0 ; sub_path_index < sub_path_length ; ++sub_path_index) {
               config_setting_t *sub_path_elem = config_setting_get_elem (config_url_sub_path, sub_path_index);
               struct url_config_struct *sub_path_config = NULL;

               if (sub_path_elem == NULL) {
                  fprintf (stderr, "error reading sub_path %d\n", sub_path_index);
                  error_config = true;
               }
               else if (config_setting_is_group (sub_path_elem) == CONFIG_FALSE) {
                  fprintf (stderr, "sub_path %d isn't a group\n", sub_path_index);
                  error_config = true;
               }
               else {
                  sub_path_config = read_url_config (sub_path_elem);
                  if (sub_path_config == NULL ) {
                     fprintf (stderr, "Previous error in sub_path %d\n", sub_path_index);
                     error_config = true;
                  }
               }
               if (sub_path_config != NULL) {
                  if (prev_sub_path_config == NULL) {
                     new_url_config->sub_path = sub_path_config;
                  }
                  else {
                     prev_sub_path_config->next = sub_path_config;
                  }
                  prev_sub_path_config = sub_path_config;
               }
            }
#ifdef PHS_DEBUG
            printf ("Config->sub_path=%p\n", new_url_config->sub_path);
#endif
         }
      }
   }

   if (error_config) {
      free_url_config (new_url_config);
      new_url_config = NULL;
   }

   return new_url_config;
}

bool phs_config_read ()
{
   char config_file_name[512] = "";

   // Déterminer le chemin du fichier de configuration selon l'utilisateur
   if (config_file_name[0] == '\0') {
      if (getuid() == 0) {
         strncpy(config_file_name, "/etc/pick_httpd_server.cfg", sizeof(config_file_name)-1);
         config_file_name[sizeof(config_file_name)-1] = '\0';
      } else {
         const char* home = getenv("HOME");
         if (home && home[0] != '\0') {
            snprintf(config_file_name, sizeof(config_file_name), "%s/conf/pick_httpd_server.cfg", home);
         } else {
            strncpy(config_file_name, "pick_httpd_server.cfg", sizeof(config_file_name)-1);
            config_file_name[sizeof(config_file_name)-1] = '\0';
         }
      }
   }

   //Ouverture du fichier
   if (config_read_file (&config_pick_httpd_server, config_file_name) != CONFIG_TRUE) {
      fprintf (stderr, "Can't read configuration file %s:%d %s\n", config_file_name, config_error_line (&config_pick_httpd_server), config_error_text (&config_pick_httpd_server));
      return false;
   }

   // httpd.port
   if (config_lookup_int (&config_pick_httpd_server, config_path_httpd_port, &config_http_port) != CONFIG_TRUE) {
      fprintf (stderr, "Can't find configuration %s in file %s:%d %s\n", config_path_httpd_port, config_error_file (&config_pick_httpd_server), config_error_line (&config_pick_httpd_server), config_error_text (&config_pick_httpd_server));
      return false;
   }

   // httpd.env
   config_setting_t *config_httpd_env = config_lookup (&config_pick_httpd_server, "httpd.env");
   if (config_httpd_env != NULL) {
      unsigned int env_count = config_setting_length (config_httpd_env);

      if (config_setting_is_group (config_httpd_env) == CONFIG_FALSE) {
         fprintf (stderr, "Incorrect url configuration type\n");
         return false;
      }
#ifdef PHS_DEBUG
      printf ("Config httpd.env=%u\n", env_count);
#endif
      for (unsigned int env_index = 0 ; env_index < env_count ; ++env_index) {
         config_setting_t *config_httpd_env_index = config_setting_get_elem (config_httpd_env, env_index);
         if (config_httpd_env_index != NULL) {
            const char *name = config_setting_name (config_httpd_env_index);
            const char *value = config_setting_get_string (config_httpd_env_index);
            if (name != NULL && value != NULL) {
               setenv (name, value, true);
#ifdef PHS_DEBUG
               printf ("Setenv %s=%s\n", name, value);
#endif
            }
         }
      }
   }
   // pick.account
   const char* string_pick_account;
   if (config_lookup_string (&config_pick_httpd_server, config_path_pick_account, &string_pick_account) != CONFIG_TRUE) {
      fprintf (stderr, "Can't find configuration %s in file %s:%d %s\n", config_path_pick_account, config_error_file (&config_pick_httpd_server), config_error_line (&config_pick_httpd_server), config_error_text (&config_pick_httpd_server));
      return false;
   }
   config_pick_account = string_pick_account;
   if (config_pick_account.empty ()) {
      fprintf (stderr, "OpenQM account not configured\n");
      return false;
   }

   // pick.encoding, Par défaut il n'y a pas de conversion
   const char* string_pick_encoding;
   if (config_lookup_string (&config_pick_httpd_server, config_path_pick_encoding, &string_pick_encoding) == CONFIG_TRUE) {
      try {
         if (string_pick_encoding == std::string("MacRoman")) {
            config_pick_encoding = new Poco::MacRomanEncoding ();
         } else {
            config_pick_encoding = &Poco::TextEncoding::byName (string_pick_encoding);
         }
      } catch (const Poco::NotFoundException& e) {
         fprintf (stderr, "Unknown encoding %s\n", string_pick_encoding);
         return false;
      }
   }

   // url
   config_setting_t *config_url = config_lookup (&config_pick_httpd_server, "url");
   if (config_url == NULL) {
      fprintf (stderr, "Missing url configuration\n");
      return false;
   }
   if (config_setting_is_list (config_url) == CONFIG_FALSE) {
      fprintf (stderr, "url isn't a list\n");
      return false;
   }

   unsigned int url_length = config_setting_length (config_url);
   for (unsigned int url_index = 0 ; url_index < url_length ; ++url_index) {
      config_setting_t *config_url_elem = config_setting_get_elem (config_url, url_index);
      if (config_url_elem != NULL) {
         struct url_config_struct *new_url_config = read_url_config (config_url_elem);
         if (new_url_config == NULL) {
            fprintf (stderr, "Previous error in url %d\n", url_index);
            return false;
         }
         new_url_config->next = first_url_config;
         first_url_config = new_url_config;
      }
   }
#ifdef PHS_DEBUG
   printf ("First config path=%s sub_path=%p\n", first_url_config->path, first_url_config->sub_path);
#endif

   return true;
}

void phs_config_free ()
{
   while (first_url_config != NULL) {
      struct url_config_struct *current_url_config = first_url_config;

      first_url_config = current_url_config->next;
      free_url_config (current_url_config);
   }
}
