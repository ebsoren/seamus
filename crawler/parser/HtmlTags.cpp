#include <ctype.h>
#include <cstring>
#include <cassert>
#include "HtmlTags.h"

#define M(l, r) (l + (r - l) / 2)

// name points to beginning of the possible HTML tag name.
// nameEnd points to one past last character.
// Comparison is case-insensitive.
// Use a binary search.
// If the name is found in the TagsRecognized table, return
// the corresponding action.
// If the name is not found, return OrdinaryText.

DesiredAction LookupPossibleTag( const char *name, const char *nameEnd ) {
   // Your code here.
   const size_t N = 141;
   size_t l = 0, r = N + 1;
   size_t len = nameEnd - name;

   while (l < r) {
      int cmp = strncasecmp(TagsRecognized[M(l, r)].Tag, name, len);
      if (cmp < 0) l = M(l, r) + 1;
      else if (cmp > 0) r = M(l, r);
      else {
         if (strlen(TagsRecognized[M(l, r)].Tag) == len) return TagsRecognized[M(l, r)].Action;
         r = M(l, r);
      }
   }

   return DesiredAction::OrdinaryText;
}