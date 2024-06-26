#pragma once

#include "core/definitions.h"    // stdint + size_t
#include "libs/stdio.h"
#include <vector>

/**
 * Computes the length of the string `str`, excluding the terminating null byte.
 * @param str Pointer to the null-terminated byte string to be examined.
 * @return The number of characters in the string pointed to by `str`.
 */
extern "C" size_t strlen(const char* str);

/**
 * Breaks a string into a series of tokens using a delimiter.
 * @param s Pointer to the string to be tokenized; subsequent calls should pass NULL to continue tokenizing the same string.
 * @param delim String containing the delimiter characters.
 * @return A pointer to the next token or NULL if there are no more tokens.
 */
char* strtok(char* s, const char* delim);

/**
 * Locates the first occurrence of character `c` in the string `s`.
 * @param s Pointer to the null-terminated byte string to analyze.
 * @param c Character to search for, cast to an `int`, automatically converted to `char`.
 * @return A pointer to the first occurrence of `c` in `s`, or NULL if `c` is not found.
 */
char* strchr(const char* s, int c);

/**
 * Extracts tokens from the string `*stringp` that are delimited by characters in the string `delim`.
 * @param stringp Address of a pointer that points to a string to be tokenized; it is updated to point past the token.
 * @param delim Null-terminated string containing the delimiter characters.
 * @return A pointer to the extracted token, or NULL if there are no more tokens.
 */
char* strsep(char** stringp, const char* delim);

/**
 * Compares the two strings `s1` and `s2`.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @return An integer less than, equal to, or greater than zero, if `s1` is found, respectively, to be less than, to match, or be greater than `s2`.
 */
int strcmp(const char* s1, const char* s2);

/**
 * Compares up to `n` characters of the string `s1` to those of the string `s2`.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be greater than the first `n` bytes of `s2`.
 */
int strncmp(const char* s1, const char* s2, size_t n);

/**
 * Compares the two strings `s1` and `s2` ignoring case.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @return An integer less than, equal to, or greater than zero, if `s1` is found, respectively, to be less than, to match, or be greater than `s2`.
 */
int strcasecmp(const char* s1, const char* s2);

/**
 * Compares up to `n` characters of the string `s1` to those of the string `s2`, ignoring case.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be greater than the first `n` bytes of `s2`.
 */
int strncasecmp(const char* s1, const char* s2, size_t n);

/**
 * Copies the C string pointed by `src` into the array pointed by `dest`.
 * @param dest Pointer to the buffer where the content is to be copied.
 * @param src Pointer to the null-terminated byte string to be copied.
 * @return A pointer to `dest`.
 */
char* strcpy(char* dest, const char* src);

/**
 * Copies up to `n` characters from the C string pointed by `src` to `dest`.
 * @param dest Pointer to the buffer where the content is to be copied.
 * @param src Pointer to the null-terminated byte string to be copied.
 * @param n Number of characters to copy.
 * @return A pointer to `dest`.
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Appends the string pointed to by `src` to the end of the string pointed to by `dest`.
 * @param dest Pointer to the null-terminated byte string to be appended to.
 * @param src Pointer to the null-terminated byte string to append.
 * @return A pointer to the resulting string `dest`.
 */
char* strcat(char* dest, const char* src);

/**
 * Searches the string `s` for the first occurrence of any of the characters in the string `accept`.
 * @param s The null-terminated string to be scanned.
 * @param accept The null-terminated string containing the characters to search for.
 * @return A pointer to the character in `s` that matches one of the characters in `accept`, or NULL if no such character is found.
 */
char* strpbrk(const char* s, const char* accept);

namespace PalmyraOS::types
{

  // requires an allocator for the std::vector
  template<typename _CharT, typename _Alloc>
  class string
  {
   public:
	  // Type definitions
	  using value_type = _CharT;
	  using allocator_type = _Alloc;
	  using size_type = typename std::vector<_CharT, _Alloc>::size_type;
	  using reference = _CharT&;
	  using const_reference = const _CharT&;

	  // Constructors
	  explicit string(_Alloc alloc) : data_(alloc)
	  {
		  data_.push_back('\0');
	  }

	  string()
	  {
		  data_.push_back('\0');
	  }

	  // Range constructor with allocator
	  template<typename InputIt>
	  string(InputIt first, InputIt last, const _Alloc& alloc)
		  : data_(first, last, alloc)
	  {
		  ensure_null_terminator();
	  }

	  // Copy constructor
	  string(const string& other) : data_(other.data_, other.data_.get_allocator())
	  {}

	  // Move constructor
	  string(string&& other) noexcept: data_(std::move(other.data_))
	  {}


	  // Assignment operators

	  // Move assignment operator
	  string& operator=(string&& other) noexcept
	  {
		  if (this != &other)
		  {
			  data_ = std::move(other.data_);
			  ensure_null_terminator();
		  }
		  return *this;
	  }

	  // Copy assignment operator
	  string& operator=(const string& other)
	  {
		  if (this != &other)
		  {
			  data_ = other.data_;
			  ensure_null_terminator();
		  }
		  return *this;
	  }

	  string& operator=(const _CharT* s)
	  {
		  data_.assign(s, s + strlen(s));
		  ensure_null_terminator();
		  return *this;
	  }

	  string& operator=(std::initializer_list<_CharT> ilist)
	  {
		  data_ = ilist;
		  ensure_null_terminator();
		  return *this;
	  }

	  // Size and capacity
	  size_type size() const
	  { return data_.size(); }
	  size_type capacity() const
	  { return data_.capacity(); }
	  [[nodiscard]] bool empty() const
	  { return data_.empty(); }
	  void reserve(size_type new_cap)
	  { data_.reserve(new_cap); }
	  void resize(size_type count)
	  {
		  data_.resize(count);
		  ensure_null_terminator();
	  }
	  void resize(size_type count, _CharT ch)
	  {
		  data_.resize(count, ch);
		  ensure_null_terminator();
	  }

	  // Element access
	  reference operator[](size_type pos)
	  { return data_[pos]; }
	  const_reference operator[](size_type pos) const
	  { return data_[pos]; }

	  reference front()
	  { return data_.front(); }
	  const_reference front() const
	  { return data_.front(); }

	  reference back()
	  { return data_.back(); }
	  const_reference back() const
	  { return data_.back(); }

	  reference at(size_type pos)
	  {
		  if (pos >= size())
		  {
			  pos = pos % size();
		  }
		  return data_[pos];
	  }

	  const_reference at(size_type pos) const
	  {
		  if (pos >= size())
		  {
			  pos = pos % size();
		  }
		  return data_[pos];
	  }

	  // Modifiers
	  void push_back(const _CharT& c)
	  { data_.push_back(c); }

	  void clear()
	  { data_.clear(); }

	  string& operator+=(const _CharT& c)
	  {
		  data_.push_back(c);
		  return *this;
	  }

	  string& operator+=(const string& other)
	  {
		  data_.insert(data_.end(), other.data_.begin(), other.data_.end());
		  return *this;
	  }

	  string& operator+=(const _CharT* s)
	  {
		  data_.insert(data_.end(), s, s + std::char_traits<_CharT>::length(s));
		  return *this;
	  }

	  string& operator+=(std::initializer_list<_CharT> ilist)
	  {
		  data_.insert(data_.end(), ilist);
		  return *this;
	  }

	  // Comparison operators
	  bool operator==(const string& other) const
	  { return data_ == other.data_; }

	  bool operator==(const char* other) const
	  {
		  return strcmp(c_str(), other) == 0;
	  }

	  bool operator!=(const string& other) const
	  { return *this != other; }
	  bool operator<(const string& other) const
	  { return data_ < other.data_; }
	  bool operator<=(const string& other) const
	  { return data_ <= other.data_; }
	  bool operator>(const string& other) const
	  { return data_ > other.data_; }
	  bool operator>=(const string& other) const
	  { return data_ >= other.data_; }

	  // Split function
	  template<typename AllocV>
	  std::vector<string<_CharT, _Alloc>, AllocV> split(AllocV allocV, _CharT delimiter) const
	  {
		  std::vector<string<_CharT, _Alloc>, AllocV> result(allocV);
		  _Alloc                                      alloc = data_.get_allocator();
		  size_type                                   start = 0;
		  size_type                                   end   = 0;

		  while (end < size())
		  {
			  // Search for the delimiter
			  while (end < size() && data_[end] != delimiter)
			  {
				  ++end;
			  }

			  // Create a string from the start index to the end index using the range constructor and explicitly passing allocator
			  string<_CharT, _Alloc> token(data_.begin() + start, data_.begin() + end, alloc);
			  result.push_back(token);

			  // Skip the delimiter
			  ++end;
			  start = end;
		  }

		  // Handle the case where there is no delimiter at the end of the string
		  if (start < size())
		  {
			  string<_CharT, _Alloc> token(data_.begin() + start, data_.end(), alloc);
			  result.push_back(token);
		  }

		  return result;
	  }

	  // C-string access
	  const _CharT* c_str() const
	  {
		  return &data_[0];
	  }

	  void ensure_null_terminator()
	  {
		  if (data_.empty() || data_.back() != '\0')
		  {
			  data_.push_back('\0');
		  }
	  }

   private:
	  std::vector<_CharT, _Alloc> data_;
  };

// Non-member functions
  template<typename _CharT, typename _Alloc>
  string<_CharT, _Alloc> operator+(const string<_CharT, _Alloc>& lhs, const string<_CharT, _Alloc>& rhs)
  {
	  string<_CharT, _Alloc> temp(lhs);
	  temp += rhs;
	  return temp;
  }

  template<typename _CharT, typename _Alloc>
  string<_CharT, _Alloc> operator+(const string<_CharT, _Alloc>& lhs, const _CharT* rhs)
  {
	  string<_CharT, _Alloc> temp(lhs);
	  temp += rhs;
	  return temp;
  }

  template<typename _CharT, typename _Alloc>
  string<_CharT, _Alloc> operator+(const _CharT* lhs, const string<_CharT, _Alloc>& rhs)
  {
	  string<_CharT, _Alloc> temp(lhs);
	  temp += rhs;
	  return temp;
  }

} // namespace mustyOS::types

