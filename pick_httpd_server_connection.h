#include <cstdint>
#include <vector>
#include <string>

#include <microhttpd.h>

#include "pick_dynarray.h"

class pick_connection {
public:
   static enum MHD_Result pick_to_connection (void *cls,
         struct MHD_Connection *connection,
         const char *url,
         const char *method,
         const char *version,
         const char *upload_data,
         size_t *upload_data_size,
         void **connection_info_cls);
   static void request_completed (void *cls,
         struct MHD_Connection *connection,
         void **pick_connection_cls,
         enum MHD_RequestTerminationCode toe);
private:
   typedef unsigned int http_sc_t;
   enum class connection_t {
      GET,
      POST,
      PUT,
      DELETE,
      PATCH
   };

   pick_connection ();
   ~pick_connection ();

   void set_error_exception (std::exception *new_exception);

   enum MHD_Result initialize (void *cls,
         struct MHD_Connection *connection,
         const char *url,
         const std::string &method,
         const char *version,
         const char *upload_data,
         size_t *upload_data_size);
   enum MHD_Result process (void *cls,
         struct MHD_Connection *connection,
         const char *url,
         const std::string &method,
         const char *version,
         const char *upload_data,
         size_t *upload_data_size);
   void initialize_request (struct MHD_Connection *connection,
         const char *url,
         const std::string &method);
   void initialize_response ();

   static enum MHD_Result iterate_post (void *postinfo_cls,
         enum MHD_ValueKind kind,
         const char *key,
         const char *filename,
         const char *content_type,
         const char *transfer_encoding,
         const char *data,
         uint64_t off,
         size_t size);
   static enum MHD_Result iterate_header (void *headerininfo_cls, enum MHD_ValueKind kind, const char *key, const char *value);
   static enum MHD_Result iterate_querystring (void *quertystringinfo_cls, enum MHD_ValueKind kind, const char *key, const char *value);
   bool post_json_process (const char *upload_data, size_t upload_data_size);

   struct MHD_Response *make_default_error_page (struct MHD_Connection *connection, http_sc_t http_return_code) const;
   MHD_Result send_response (struct MHD_Connection *connection, http_sc_t http_return_code, struct MHD_Response *response) const;

   void add_conn_addr_2dynarray (struct sockaddr *conn_addr);

   struct MHD_PostProcessor *post_processor; 

   std::string subr;
   std::vector<std::string> method_authorized;
   std::vector<std::string> param_authorized;
   connection_t connection_type;
   std::exception *error_exception;
   pick_dynarray post_dynarray;
   bool fetch_post_json;

   char *req_auth_type;
   char *req_hostname;
   pick_dynarray req_header_in;
   pick_dynarray req_query_string;
   pick_dynarray req_remote_info;
   char *req_remote_user;
   std::string req_method;
   char *req_uri;
   pick_dynarray req_server_info;
   char *resp_http_output;
   char  resp_http_status [4];
   char *resp_header_out;
};
