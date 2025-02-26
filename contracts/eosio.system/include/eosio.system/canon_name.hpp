#include <eosio.system/eosio.system.hpp>
#include <cassert>

namespace eosiosystem {

// -------------------------------------------------------------------------------------------
struct canon_name_t
{
   uint64_t _value; // all significant characters are shifted to the least significant position
   uint64_t _size;  // number of significant characters

   uint64_t mask()  const { return (1ull << (_size * 5)) - 1; }
   uint64_t size()  const { return _size; }
   bool     valid() const { return _size != 0; }

   // starts from the end, so n[0] is the last significant character of the name
   uint64_t operator[](uint64_t i) const { return (_value >> (5 * i)) & 0x1F; }

   canon_name_t(name n) {
      uint64_t v         = n.value;
      auto     num_zeros = std::countr_zero(v);
      if ((num_zeros < 4) || (num_zeros == 64)) { // 13th char must be zero, and pattern should not be empty
         _size = 0;                               // zero size denotes an invalid pattern
      } else {
         uint64_t shift = 4 + ((num_zeros - 4) / 5) * 5;
         _value         = v >> shift;
         auto sig_bits  = 64 - shift;
         _size          = sig_bits / 5;
      }
   }

   // returns true if account_name ends with the same characters as `this`, preceded by a `.` (zero) character
   bool is_suffix_of(canon_name_t account_name) const {
      assert(valid());
      if (size() > account_name.size())
         return false;

      if (((_value ^ account_name._value) & mask()) != 0)
         return false;

      // pattern matches the end of account_name. To be a suffix it has to either be the same length (i.e. exact match),
      // or be preceded by a dot in account_name
      return (size() == account_name.size()) || (account_name[size()] == 0);
   }

   bool found_in(canon_name_t account_name) const {
      assert(valid());
      if (size() > account_name.size())
         return false;

      auto   sz   = size();
      size_t diff = account_name.size() - sz + 1;
      for (size_t i = 0; i < diff; ++i)
         if (((_value ^ (account_name._value >> (5 * i))) & mask()) == 0)
            return true;
      return false;
   }
};

// -------------------------------------------------------------------------------------------
static inline bool name_allowed(name account, name patt)
{
   canon_name_t pattern(patt);
   canon_name_t account_name(account);

   if (!pattern.valid())
      return true; // ignore invalid patterns

   if (pattern.is_suffix_of(account_name))
      return true;
   if (pattern.found_in(account_name))
      return false;
   return true;
}

} // namespace eosiosystem