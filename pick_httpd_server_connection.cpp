#include <algorithm>
#include <sstream>
#include <cstring>
#include <iomanip>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <Poco/UTF8Encoding.h>
#include <Poco/TextConverter.h>

#include <qmclilib.h>

#include "pick_httpd_server.h"
#include "pick_httpd_server_config.h"
#include "pick_httpd_server_logs.h"
#include "pick_httpd_server_connection.h"

// Constantes globales

static const size_t headerin_max_size = 16384;
static const size_t post_max_size = 32768;
static const size_t post_buffer_size = post_max_size / 32;
static const size_t querystring_max_size = 16384;
static const char protocol_http [] = "http";
static const char protocol_https [] = "https";
static const Poco::UTF8Encoding utf8_encoding;

// Fonctions non membres

std::string input_conversion (const std::string in_string)
{
   if (config_pick_encoding != NULL) {
      Poco::TextConverter converter (utf8_encoding, *config_pick_encoding, '\x1f');
      std::string out_string;

      int error_count = converter.convert (in_string, out_string);
      if (error_count > 0 || out_string.find('\x1f') != std::string::npos) {
         throw std::range_error (std::string ("String convertion from ") + utf8_encoding.canonicalName () + " to " + config_pick_encoding->canonicalName ());
      }
      return out_string;
   }
   return in_string;
}

std::string output_conversion (const std::string in_string)
{
   if (config_pick_encoding != NULL) {
      Poco::TextConverter converter (*config_pick_encoding, utf8_encoding);
      std::string out_string;
      converter.convert (in_string, out_string);
      return out_string;
   }
   return in_string;
}

// Fonctions membres de la classe

pick_connection::pick_connection ()
   : connection_type (connection_t::GET),
   error_exception (NULL),
   fetch_post_json (false),
   req_auth_type (NULL),
   req_hostname (NULL),
   req_remote_user (NULL),
   req_uri (NULL),
   resp_http_output (NULL),
   resp_header_out (NULL)
{
   resp_http_status [0] = '\0';
}

pick_connection::~pick_connection ()
{
   if (error_exception) {
      delete error_exception;
   }
   if (req_auth_type) {
      free (req_auth_type);
   }
   if (req_hostname) {
      free (req_hostname);
   }
   if (req_remote_user) {
      free (req_remote_user);
   }
   if (req_uri) {
      free (req_uri);
   }
   if (resp_http_output) {
      free (resp_http_output);
   }
   if (resp_header_out) {
      free (resp_header_out);
   }
}

enum MHD_Result pick_connection::pick_to_connection (void *cls,
      struct MHD_Connection *connection,
      const char *url,
      const char *method,
      const char *version,
      const char *upload_data,
      size_t *upload_data_size,
      void **pick_connection_cls)
{
   enum MHD_Result result;
   pick_connection *current_pick_connection = NULL;

   if (*pick_connection_cls == NULL) {
#if PHS_DEBUG
      std::cerr << "Création pick_connection" << std::endl;
#endif
      current_pick_connection = new pick_connection ();
      *pick_connection_cls = current_pick_connection;
      result = current_pick_connection->initialize (cls, connection, url, method, version, upload_data, upload_data_size);
   }
   else {
      current_pick_connection = (pick_connection*) *pick_connection_cls;

      if (current_pick_connection->connection_type != connection_t::GET && *upload_data_size != 0) {
         if (current_pick_connection->fetch_post_json) {
            if (!current_pick_connection->post_json_process (upload_data, *upload_data_size)) {
               return MHD_NO;
            }
         }
         else {
            MHD_post_process (current_pick_connection->post_processor,
                  upload_data,
                  *upload_data_size);
         }
         *upload_data_size = 0;
         result = MHD_YES;
      }
      else {
         if (current_pick_connection->fetch_post_json) {
            try {
               std::string post_dynarray = current_pick_connection->post_dynarray;
#if PHS_DEBUG
               std::cerr << "Contenu du json avant conversion : ";
               for (auto it = post_dynarray.begin(); it != post_dynarray.end(); ++it) {
                  std::cerr << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(*it);
               }
               std::cerr << std::endl;
#endif

               std::string post_pick_encoding = input_conversion (post_dynarray);
               current_pick_connection->post_dynarray = post_pick_encoding;
            }
            catch (const std::range_error& e) {
               // Impossible de convertir un caractère
               PHSLogging::fatal (std::string ("Can't convert json, ") + e.what ());
               http_sc_t http_status_code = MHD_HTTP_BAD_REQUEST;
               struct MHD_Response *response = current_pick_connection->make_default_error_page (connection, http_status_code);
               return current_pick_connection->send_response (connection, http_status_code, response);
            }
         }
         result = current_pick_connection->process (cls, connection, url, method, version, upload_data, upload_data_size);
      }
   }
   if (result != MHD_YES) {
      delete current_pick_connection;
      *pick_connection_cls = NULL;
   }
   return result;
}

void pick_connection::request_completed (void *cls,
      struct MHD_Connection *connection,
      void **pick_connection_cls,
      enum MHD_RequestTerminationCode toe)
{
   pick_connection *current_pick_connection = (pick_connection*) *pick_connection_cls;

#ifdef PHS_DEBUG
   printf ("Start request_completed\n");
#endif
   if (current_pick_connection != NULL) {
      delete current_pick_connection;
      *pick_connection_cls = NULL;
   }
#ifdef PHS_DEBUG
   printf ("End request_completed\n");
#endif
}

void pick_connection::set_error_exception (std::exception *new_exception)
{
   if (error_exception) {
      delete error_exception;
   }
   error_exception = new_exception;
}

enum MHD_Result pick_connection::initialize (void *cls,
      struct MHD_Connection *connection,
      const char *url,
      const std::string &method,
      const char *version,
      const char *upload_data,
      size_t *upload_data_size)
{
   struct MHD_Response *response = NULL;

   // Recherche des paramètres
   http_sc_t http_status_code = extract_subroutine_name_from_url (url, subr, method_authorized, param_authorized);
   if (http_status_code) {
      response = make_default_error_page (connection, http_status_code);
      return send_response (connection, http_status_code, response);
   }

   // Contrôle de la méthode
   if (std::find (method_authorized.begin (), method_authorized.end (), method) == method_authorized.end ()) {
      http_status_code = MHD_HTTP_METHOD_NOT_ALLOWED;
      response = make_default_error_page (connection, http_status_code);
      return send_response (connection, http_status_code, response);
   }

   if (method == "GET") {
      connection_type = connection_t::GET;
   }
   else if (method == "OPTIONS") {
      connection_type = connection_t::OPTIONS;
   }
   else {
      // Gestion des paramètres post
      if (method == "PUT") {
         connection_type = connection_t::PUT;
      }
      else if (method == "PATCH") {
         connection_type = connection_t::PATCH;
      }
      else if (method == "DELETE") {
         connection_type = connection_t::DELETE;
      }
      else {
         connection_type = connection_t::POST;
      }

      const char *content_type_header = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Content-Type");

#ifdef PHS_DEBUG
      std::cerr << "content_type=\"" << (content_type_header ? content_type_header : "NULL") << "\"" << std::endl;
#endif

      std::string content_type_main;
      if (content_type_header != NULL) {
         // Extraire avant le ';'
         const char *semicolon = strchr(content_type_header, ';');
         if (semicolon) {
            content_type_main.assign(content_type_header, semicolon - content_type_header);
         } else {
            content_type_main = content_type_header;
         }
         // Supprimer les espaces à la fin
         size_t end = content_type_main.find_last_not_of(" \t");
         if (end != std::string::npos) {
            content_type_main = content_type_main.substr(0, end + 1);
         } else {
            content_type_main.clear();
         }
      }

      if (!content_type_main.empty() && content_type_main == "application/json") {
         fetch_post_json = true;
      } else if (!content_type_main.empty() &&
                 (content_type_main == "application/x-www-form-urlencoded" ||
                  content_type_main == "multipart/form-data")) {
#ifdef PHS_DEBUG
         printf ("Create post processor\n");
#endif
         post_processor = MHD_create_post_processor (connection,
               post_buffer_size,
               iterate_post,
               (void *) this);
         if (post_processor == NULL) {
            PHSLogging::fatal ("Can't create post processor (out of memory, unsupported encoding)");
            return MHD_NO;
         }
      } else {
         // Content-Type non supporté
         http_sc_t http_status_code = MHD_HTTP_UNSUPPORTED_MEDIA_TYPE;
         struct MHD_Response* response = make_default_error_page(connection, http_status_code);
         return send_response(connection, http_status_code, response);
      }
   }

   return MHD_YES;
}

enum MHD_Result pick_connection::iterate_post (void *postinfo_cls,
      enum MHD_ValueKind kind,
      const char *key,
      const char *filename,
      const char *content_type,
      const char *transfer_encoding,
      const char *data,
      uint64_t off,
      size_t size)
{
   pick_connection *current_pick_connection = (pick_connection*) postinfo_cls;
   try {
      size_t new_len = current_pick_connection->post_dynarray.length () + strlen (key) + strlen(data) + 2; // 2 séparateurs

      if (new_len > post_max_size) {
         current_pick_connection->set_error_exception (new std::length_error ("Post data too big"));
         return MHD_NO;
      }
      std::string key_pick_encoding = input_conversion (key);
      std::string data_pick_encoding = input_conversion (data);
      current_pick_connection->post_dynarray.add_key_value (1, key_pick_encoding, data_pick_encoding);
   }
   catch (const std::range_error& e) {
      // Impossible de convertir un caractère
      current_pick_connection->set_error_exception (new std::range_error (e));
      return MHD_NO;
   }
   catch (const std::bad_alloc& e) {
      // Mémoire insuffisante
      current_pick_connection->set_error_exception (new std::bad_alloc (e));
      return MHD_NO;
   }
   catch (const std::exception& e) {
      // Autre execption
      current_pick_connection->set_error_exception (new std::exception (e));
      return MHD_NO;
   }
   return MHD_YES;
}

bool pick_connection::post_json_process (const char *upload_data, size_t upload_data_size)
{
   try {
      size_t new_len = post_dynarray.length () + upload_data_size + 1; // 1 = \0 de fin

      if (new_len > post_max_size) {
         set_error_exception (new std::length_error ("Post data too big"));
         return false;
      }
      post_dynarray += std::string (upload_data, upload_data_size);
   }
   catch (const std::bad_alloc& e) {
      // Mémoire insuffisante
      set_error_exception (new std::bad_alloc (e));
      return false;
   }
   catch (const std::exception& e) {
      // Autre execption
      set_error_exception (new std::exception (e));
      return false;
   }
   return true;
}

enum MHD_Result pick_connection::process (void *cls,
      struct MHD_Connection *connection,
      const char *url,
      const std::string &method,
      const char *version,
      const char *upload_data,
      size_t *upload_data_size)
{
   http_sc_t http_status_code = 0;
   struct MHD_Response *response = NULL;

   try {
      if (error_exception) {
         throw *error_exception;
      }

      /*
       * 1 Auth type : MHD_basic_auth_get_username_password, MHD_digest_auth_get_username, certificat?
       * 2 Hostname
       * 3 HTTP headers (in) : MHD_get_connection_values MHD_HEADER_KIND, entre autre pour les variables suivantes
       * 4 Query string
       * 5 Post data : post_dynarray
       * 6 Remote info : IP address and port
       * 7 Remote user : MHD_basic_auth_get_username_password, MHD_digest_auth_get_username, certificat?
       * 8 Request method : method
       * 9 Request uri : url
       * 10 Server info : protocol, IP address, port
       * 11 Response content (out)
       * 12 HTTP status code (out)
       * 13 HTTP header (out)
       */

#ifdef PHS_DEBUG
      printf ("Starting\n");
#endif

      strcpy (resp_http_status, "*3");

      initialize_request (connection, url, method);

      initialize_response ();

      if (!QMConnectLocal (config_pick_account.c_str ())) {
         PHSLogging::fatal ("Can't connect to OpenQM account " + config_pick_account + ": " + QMError ());
         http_status_code = MHD_HTTP_SERVICE_UNAVAILABLE;
      }
      else {
#ifdef PHS_DEBUG
         printf ("Connected to OpenQM\n");
#endif

#ifdef PHS_DEBUG
         printf ("Calling to OpenQM, routine %s\n", subr.c_str ());
#endif
         char *char_req_header_in = strdup (req_header_in.c_str ());
         char *char_req_query_string = strdup (req_query_string.c_str ());
         char *char_post_dynarray = strdup (post_dynarray.c_str ());
         char *char_req_remote_info = strdup (req_remote_info.c_str ());
         char *char_req_method = strdup (req_method.c_str ());
         char *char_req_server_info = strdup (req_server_info.c_str ());
         if (char_req_header_in == NULL || char_req_query_string == NULL || char_post_dynarray == NULL || char_req_remote_info == NULL || char_req_method == NULL || char_req_server_info == NULL) {
            if (char_req_header_in == NULL) {
               free (char_req_header_in);
            }
            if (char_req_query_string == NULL) {
               free (char_req_query_string);
            }
            if (char_post_dynarray == NULL) {
               free (char_post_dynarray);
            }
            if (char_req_remote_info == NULL) {
               free (char_req_remote_info);
            }
            if (char_req_method == NULL) {
               free (char_req_method);
            }
            if (char_req_server_info == NULL) {
               free (char_req_server_info);
            }
            throw std::bad_alloc ();
         }
         QMCall (subr.c_str (),
               13,
               req_auth_type,                 // 1
               req_hostname,                  // 2
               char_req_header_in,            // 3
               char_req_query_string,         // 4
               char_post_dynarray,            // 5
               char_req_remote_info,          // 6
               req_remote_user,               // 7
               char_req_method,               // 8
               req_uri,                       // 9
               char_req_server_info,          // 10
               resp_http_output,              // 11
               resp_http_status,              // 12
               resp_header_out                // 13
               );
         free (char_req_header_in);
         free (char_req_query_string);
         free (char_post_dynarray);
         free (char_req_remote_info);
         free (char_req_method);
         free (char_req_server_info);

#ifdef PHS_DEBUG
         printf ("OpenQM call return\n");
#endif
         QMDisconnect ();
#ifdef PHS_DEBUG
         printf ("Disconnected from OpenQM\n");
#endif

         // Check if the routine update the status
         if (strcmp (resp_http_status, "*3") == 0) {
            PHSLogging::fatal ("The routine " + subr + " didn't update http status");
            http_status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
         }
         else {
            // Return status
            std::istringstream stream_http_status (resp_http_status);

            if (!(stream_http_status >> http_status_code && stream_http_status.eof ())) {
               PHSLogging::fatal ("The routine " + subr + " returned a bas status (" + resp_http_status + ")");
               http_status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            }
            else if (!http_status_code) {
               http_status_code = MHD_HTTP_OK;
            }

            // Complete web page (only text page from Pick - text, html, json, ...)
            std::string http_output_utf8 = output_conversion (resp_http_output);
            free (resp_http_output);
            resp_http_output = NULL;
            response = MHD_create_response_from_buffer (http_output_utf8.length (), (void *) http_output_utf8.c_str (), MHD_RESPMEM_MUST_COPY);
            if (response == NULL) {
               PHSLogging::fatal ("Can't create response from subroutine response");
               http_status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
            }

            // Process headers out
            pick_dynarray header_out_pda (resp_header_out);
            pick_dynarray header_out_fields (header_out_pda.extract (1));
            pick_dynarray header_out_values (header_out_pda.extract (2));
            pick_dynarray::rang_num_t field_numbers_hout = header_out_fields.dcount (pick_dynarray::value_mark_string);

            for (pick_dynarray::rang_num_t field_number = 0 ; field_number < field_numbers_hout ; field_number++)
            {
               std::string header_field = header_out_fields.extract (1, field_number + 1, 1);
               std::string header_value = header_out_values.extract (1, field_number + 1, 1);
               MHD_add_response_header (response, header_field.c_str (), header_value.c_str ());
#ifdef PHS_DEBUG
               std::cerr << "Header out " << header_field << "=" << header_value << std::endl;
#endif
            }
         }
      }
   }
   catch (const std::bad_alloc& e) {
      // Mémoire insuffisante
      PHSLogging::fatal ("Memory full");
      http_status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
   }
   catch (const std::length_error& e) {
      // En-tête, paramètre get ou payload post trop long
      PHSLogging::fatal ("Parameters to long");
      http_status_code = MHD_HTTP_CONTENT_TOO_LARGE;
      // Pourait être aussi MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE
   }
   catch (const std::invalid_argument& e) {
      // Nom de champ interdit dans les paramètres de la requête
      PHSLogging::fatal (e.what ());
      http_status_code = MHD_HTTP_BAD_REQUEST;
   }
   catch (const std::exception& e) {
      // Autre execption
      PHSLogging::fatal (std::string ("Other problem ") + e.what ());
      http_status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
   }
   if (response == NULL) {
      response = make_default_error_page (connection, http_status_code);
   }
   return send_response (connection, http_status_code, response);
}

enum MHD_Result pick_connection::iterate_header (void *pick_connection_cls,
      enum MHD_ValueKind kind,
      const char *key,
      const char *value)
{
   // Ignore Host header pass in hostname
   if (strcasecmp (key, "host") != 0) {
      pick_connection *current_pick_connection = (pick_connection*) pick_connection_cls;
      try {
         size_t new_len = current_pick_connection->req_header_in.length() + strlen (key) + strlen (value) + 2; // 2 séparateurs

         if (new_len > headerin_max_size) {
            current_pick_connection->set_error_exception (new std::length_error ("Header received too big"));
            return MHD_NO;
         }
         current_pick_connection->req_header_in.add_key_value (1, key, value);
      }
      catch (const std::bad_alloc& e) {
         // Mémoire insuffisante
         current_pick_connection->set_error_exception (new std::bad_alloc (e));
         return MHD_NO;
      }
      catch (const std::exception& e) {
         // Autre execption
         current_pick_connection->set_error_exception (new std::exception (e));
         return MHD_NO;
      }
   }
   return MHD_YES;
}

enum MHD_Result pick_connection::iterate_querystring (void *pick_connection_cls,
      enum MHD_ValueKind kind,
      const char *key,
      const char *value)
{
   pick_connection *current_pick_connection = (pick_connection*) pick_connection_cls;
   try {
      if (std::find (current_pick_connection->param_authorized.begin(), current_pick_connection->param_authorized.end(), key) == current_pick_connection->param_authorized.end ()) {
         current_pick_connection->set_error_exception (new std::invalid_argument (std::string ("Query string invalid parameter \"") + key + "\""));
         return MHD_NO;
      }

      size_t new_len = current_pick_connection->req_query_string.length () + strlen (key) + (value == NULL ? 0 : strlen (value)) + 2; // 2 séparateurs

      if (new_len > querystring_max_size) {
         current_pick_connection->set_error_exception (new std::length_error ("Query string too long"));
         return MHD_NO;
      }
      std::string key_pick_encoding = input_conversion (key);
      std::string value_pick_encoding = input_conversion (value);
      current_pick_connection->req_query_string.add_key_value (1, key_pick_encoding, value_pick_encoding);
   }
   catch (const std::range_error& e) {
      // Impossible de convertir un caractère
      current_pick_connection->set_error_exception (new std::range_error (e));
      return MHD_NO;
   }
   catch (const std::bad_alloc& e) {
      // Mémoire insuffisante
      current_pick_connection->set_error_exception (new std::bad_alloc (e));
      return MHD_NO;
   }
   catch (const std::exception& e) {
      // Autre execption
      current_pick_connection->set_error_exception (new std::exception (e));
      return MHD_NO;
   }
   return MHD_YES;
}

void pick_connection::initialize_request (struct MHD_Connection *connection,
      const char *url,
      const std::string &method)
{
   // Copy string
   req_method = method;
   req_uri = strdup (url);
   if (req_uri == NULL) {
      throw std::bad_alloc();
   }

   // Request hostname
   const char *header_hostname = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Host");
   /*if (req_hostname == NULL) { TODO
      throw std::invalid_argument ("Hostname missing");
   }
   else { */
      req_hostname = strdup (header_hostname == NULL ? "" : header_hostname);
      if (req_hostname == NULL) {
          throw std::bad_alloc();
      }
   //}

   // Process headers in
   MHD_get_connection_values (connection, MHD_HEADER_KIND, &iterate_header, this);
   if (error_exception) {
      throw *error_exception;
   }

   // Retreive data from GET method
   MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, &iterate_querystring, this);
   if (error_exception) {
      throw *error_exception;
   }

   // Remote info
   const union MHD_ConnectionInfo *remote_conn_info = MHD_get_connection_info (connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
   if (remote_conn_info) {
      add_conn_addr_2dynarray (remote_conn_info->client_addr);
   }

   // Authentication type and remove user
   // TODO basic and digest authentication
   req_auth_type = strdup ("NONE");
   if (req_auth_type == NULL) {
      throw std::bad_alloc();
   }

   // Server info
   req_server_info.add_key_value (1, "protocol", (MHD_get_connection_info (connection, MHD_CONNECTION_INFO_PROTOCOL) == NULL) ? protocol_http : protocol_https);
   // Il ne semble pas y avoir de solution permettant de connaitre l'adresse IP et le port du serveur sur laquelle est arrivée la connexion
}

void pick_connection::add_conn_addr_2dynarray (struct sockaddr *conn_addr)
{
   if (conn_addr) {
      uint16_t conn_port;
      char *conn_addr_string = NULL;

      switch (conn_addr->sa_family) {
         case AF_INET:
            // IPv4
            {
               conn_addr_string = (char*) malloc (INET_ADDRSTRLEN);
               if (!conn_addr_string) {
                  throw std::bad_alloc();
               }
               const struct sockaddr_in *ipv4 = (struct sockaddr_in *)conn_addr;
               inet_ntop (AF_INET, &ipv4->sin_addr, conn_addr_string, INET_ADDRSTRLEN);
               conn_port = ntohs (ipv4->sin_port);
            }
            break;
         case AF_INET6:
            // IPv6
            {
               conn_addr_string = (char*) malloc (INET6_ADDRSTRLEN + 6);
               if (!conn_addr_string) {
                  throw std::bad_alloc();
               }
               const struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)conn_addr;
               inet_ntop (AF_INET6, &ipv6->sin6_addr, conn_addr_string, INET6_ADDRSTRLEN);
               conn_port = ntohs (ipv6->sin6_port);
            }
            break;
      }
      if (conn_addr_string != NULL) {
         req_remote_info.add_key_value (1, "address", conn_addr_string);

         std::ostringstream conn_port_string;
         conn_port_string << conn_port;

         req_remote_info.add_key_value (1, "port", conn_port_string.str ());
      }
   }
}


void pick_connection::initialize_response ()
{
   resp_http_output = (char*) malloc (65536);
   if (resp_http_output == NULL) {
      throw std::bad_alloc();
   }
   strcpy (resp_http_output, "*65535");
   resp_header_out = (char*) malloc (16384);
   if (resp_header_out == NULL) {
      throw std::bad_alloc();
   }
   strcpy (resp_header_out, "*16383");
}


struct MHD_Response *pick_connection::make_default_error_page (struct MHD_Connection *connection, http_sc_t status_code) const
{
   struct MHD_Response *response = NULL;
   std::string error_message;

#ifdef PHS_DEBUG
   printf ("Default error page %u\n", status_code);
#endif

   switch (status_code) {
      case MHD_HTTP_BAD_REQUEST:           // 400
         error_message = "Bad request";
         break;
      case MHD_HTTP_NOT_FOUND:             // 404
         error_message = "Page not found";
         break;
      case MHD_HTTP_METHOD_NOT_ALLOWED:    // 405
         error_message = "Method not allowed";
         break;
      case MHD_HTTP_CONTENT_TOO_LARGE:     // 413
         error_message = "Content too large";
         break;
      case MHD_HTTP_UNSUPPORTED_MEDIA_TYPE: // 415
         error_message = "Unsupported Media Type";
         break;
      case MHD_HTTP_INTERNAL_SERVER_ERROR: // 500
         error_message = "Internal server error";
         break;
      case MHD_HTTP_SERVICE_UNAVAILABLE:   // 503
         error_message = "Service unavailable";
         break;
      default:
         {
            std::ostringstream default_message;
            default_message << "Unknown error " << status_code;
            error_message = default_message.str ();
            status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
         }
         break;
   }

   static const std::string common_error_page = "<html><head><title>Error</title></head><body><p>%message%</p></body></html>";

   std::string page_to_send = common_error_page;
   size_t message_pos = page_to_send.find ("%message%");
   if (message_pos != std::string::npos) {
      page_to_send.replace (message_pos, 9, error_message);
   }
   // Si après const est respecté car on utilise COPY
   response = MHD_create_response_from_buffer (page_to_send.length (), const_cast<void*> ((const void *) page_to_send.c_str ()), MHD_RESPMEM_MUST_COPY);
   if (response == NULL) {
      PHSLogging::fatal ("Can't create buffer for default error page");
   }
   else {
      MHD_add_response_header (response, "Content-type", "text/html; charset=utf-8");
   }
   return response;
}


enum MHD_Result pick_connection::send_response (struct MHD_Connection *connection, http_sc_t http_status_code, struct MHD_Response *response) const
{
   enum MHD_Result return_result = MHD_NO;

   if (response != NULL) {
#ifdef PHS_DEBUG
      printf ("Before queue response\n");
#endif
      return_result = MHD_queue_response (connection,
                                          http_status_code,
                                          response);
#ifdef PHS_DEBUG
      printf ("After queue response\n");
#endif
      // La mémoire utilisée par response sera libérée par microhttpd après envoi de la réponse au client
   }
   return return_result;
}
