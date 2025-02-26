#include <eosio.system/eosio.system.hpp>
#include <cassert>

namespace eosiosystem {

// -------------------------------------------------------------------------------------------
struct canon_name_t
{
   uint64_t _value; // all significant characters are shifted to the least significant position
   uint64_t _size;  // number of significant characters
   bool     _valid_pattern;

   uint64_t mask() const { return (1ull << (_size * 5)) - 1; }
   uint64_t size() const { return _size; }
   bool     valid() const { return _valid_pattern; }

   // starts from the end, so n[0] is the last significant character of the name
   uint64_t operator[](uint64_t i) const { return (_value >> (5 * i)) & 0x1F; }

   canon_name_t(name n)
   {
      uint64_t v         = n.value;
      auto     num_zeros = std::countr_zero(v);
      _valid_pattern = (num_zeros >= 4) && (num_zeros < 64); // 13th char must be zero, and pattern should not be empty

      if (_valid_pattern) {
         uint64_t shift = 4 + ((num_zeros - 4) / 5) * 5;
         _value         = v >> shift;
         auto sig_bits  = 64 - shift;
         _size          = sig_bits / 5;
      }
   }

   // returns true if account_name ends with the same characters as `this`, preceded by a `.` (zero) character
   bool is_suffix_of(canon_name_t account_name) const
   {
      assert(_valid_pattern && _size < account_name._size);
      if (((_value ^ account_name._value) & mask()) != 0)
         return false;

      // pattern matches the end of account_name. To be a suffix it has to be preceded by a dot in account_name
      return account_name[size()] == 0;
   }

   bool found_in(canon_name_t account_name) const
   {
      assert(_valid_pattern && _size < account_name._size);
      auto   sz   = size();
      size_t diff = account_name.size() - sz + 1;
      for (size_t i = 0; i < diff; ++i)
         if (((_value ^ (account_name._value >> (5 * i))) & mask()) == 0)
            return true;
      return false;
   }
};

// -------------------------------------------------------------------------------------------
bool name_allowed(name account, name patt)
{
   canon_name_t pattern(patt);
   canon_name_t account_name(account);

   if (!pattern.valid())
      return true; // ignore invalid patterns

   if (pattern.size() >= account_name.size())
      return true;
   if (pattern.is_suffix_of(account_name))
      return true;
   if (pattern.found_in(account_name))
      return false;
   return true;
}

} // namespace eosiosystem