#include <cstring>

class pick_dynarray {
public:
   typedef int16_t rang_num_t; // int et pas unsigned car le numéro de champ, valeur ou sous-valeur peut être négatif pour contenir -1 en cas d'insertion à la fin ou un négatif pour le résultat d'un locate

   pick_dynarray ()
      : content (NULL)
   {
   }
   pick_dynarray (const char *original_string);
   pick_dynarray (const std::string &original_string)
      : pick_dynarray (original_string.c_str ())
   {
   }
   ~pick_dynarray ();

   void add_key_value (rang_num_t field_num, const std::string key, const std::string value);
   // locate : retour > 0 avec numéro de rang si trouvé ou < 0 avec numéro de rang si absent
   rang_num_t locate (std::string key, const std::string order, rang_num_t field_num = 0, rang_num_t value_num = 0, rang_num_t subvalue_num = 0) const;
   rang_num_t locate (std::string key, rang_num_t field_num = 0, rang_num_t value_num = 0, rang_num_t subvalue_num = 0) const
   {
      return locate (key, "", field_num, value_num, subvalue_num);
   }
   std::string extract (rang_num_t field_num = 0, rang_num_t value_num = 0, rang_num_t subvalue_num = 0) const;
   void replace (std::string string_value, rang_num_t field_num = 0, rang_num_t value_num = 0, rang_num_t subvalue_num = 0);
   void insert (std::string string_value, rang_num_t field_num, rang_num_t value_num = 0, rang_num_t subvalue_num = 0);

   size_t length () const
   {
      return content ? strlen (content) : 0;
   }
   rang_num_t dcount (const std::string sep_char) const;
   const char* c_str () const;
   operator std::string () {
      return content;
   }

   static constexpr const char *subvalue_mark_string = "\xfc";
   static constexpr const char *value_mark_string    = "\xfd";
   static constexpr const char *field_mark_string    = "\xfe";
private:
   char *content;
};
