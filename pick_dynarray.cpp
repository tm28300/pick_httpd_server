#include <cstdint>
#include <string>
#include <cstring>

#include <qmclilib.h>

#include "pick_dynarray.h"

static const std::string empty_string ("");

pick_dynarray::pick_dynarray (const char *original_string)
   : content (NULL)
{
   content = strdup (original_string);
   if (content == NULL) {
      throw std::bad_alloc ();
   }
}

pick_dynarray::~pick_dynarray ()
{
   if (content != NULL) {
      QMFree (content);
   }
}

const char* pick_dynarray::c_str () const
{
   return (content == NULL) ? empty_string.c_str () : content;
}

void pick_dynarray::add_key_value (rang_num_t field_num, const std::string key, const std::string value)
{
   rang_num_t mv_key = locate (key, "AL", field_num);
   if (mv_key > 0) {
      replace (extract (field_num + 1, mv_key) + value, field_num + 1, mv_key);
   }
   else
   {
      insert (key, field_num, -mv_key);
      insert (value, field_num, -mv_key);
   }
}

pick_dynarray::rang_num_t pick_dynarray::locate (std::string key, const std::string order, rang_num_t field_num, rang_num_t value_num, rang_num_t subvalue_num) const
{
   int pos_key;
   if (content == NULL) {
      pos_key = -1;
   }
   else if (!QMLocate (key.c_str (), content, field_num, value_num, subvalue_num, &pos_key, order.c_str ())) {
      pos_key *= -1;
   }
   return pos_key;
}

std::string pick_dynarray::extract (rang_num_t field_num, rang_num_t value_num, rang_num_t subvalue_num) const
{
   std::string result_string;
   if (content != NULL) {
      char *result_ptr = QMExtract (content, field_num, value_num, subvalue_num);
      result_string = result_ptr;
      QMFree (result_ptr);
   }
   else {
      result_string = empty_string;
   }
   return result_string;
}

void pick_dynarray::replace (std::string string_value, rang_num_t field_num, rang_num_t value_num, rang_num_t subvalue_num)
{
   char *result_ptr = QMReplace (content != NULL ? content : empty_string.c_str (), field_num, value_num, subvalue_num, string_value.c_str ());
   if (content != NULL) {
      QMFree (content);
   }
   content = result_ptr;
}

void pick_dynarray::insert (std::string string_value, rang_num_t field_num, rang_num_t value_num, rang_num_t subvalue_num)
{
   char *result_ptr = QMIns (content != NULL ? content : empty_string.c_str (), field_num, value_num, subvalue_num, string_value.c_str ());
   if (content != NULL) {
      QMFree (content);
   }
   content = result_ptr;
}

pick_dynarray::rang_num_t pick_dynarray::dcount (const std::string sep_char) const
{
   return content != NULL ? QMDcount (content, sep_char.c_str ()) : 0;
}
