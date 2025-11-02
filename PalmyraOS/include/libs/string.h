#pragma once

#include "core/definitions.h"  // stdint + size_t
#include "libs/ctype.h"        // tolower
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

int strcmp(const wchar_t* s1, const wchar_t* s2);

/**
 * Compares up to `n` characters of the string `s1` to those of the string `s2`.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be
 * greater than the first `n` bytes of `s2`.
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
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be
 * greater than the first `n` bytes of `s2`.
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

namespace PalmyraOS::types {

    // requires an allocator for the std::vector
    template<typename _CharT, template<typename> class _Alloc>
    class string {
    public:
        // Type definitions
        using value_type      = _CharT;
        using allocator_type  = _Alloc<_CharT>;
        using size_type       = typename std::vector<_CharT, _Alloc<_CharT>>::size_type;
        using reference       = _CharT&;
        using const_reference = const _CharT&;
        using iterator        = typename std::vector<_CharT, _Alloc<_CharT>>::iterator;
        using const_iterator  = typename std::vector<_CharT, _Alloc<_CharT>>::const_iterator;


        // Constructors
        explicit string(_Alloc<_CharT> alloc) : data_(alloc) {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Constructors
        explicit string(const _CharT* _cstr) : data_() {
            this->operator=(_cstr);
            ensure_null_terminator();
            cstr = &data_[0];
        }

        string() {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Range constructor with allocator
        template<typename InputIt>
        string(InputIt first, InputIt last, const _Alloc<_CharT>& alloc) : data_(first, last, alloc) {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Range Constructor
        template<typename InputIt>
        string(InputIt first, InputIt last) : data_(first, last) {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Copy constructor
        string(const string& other) : data_(other.data_, other.data_.get_allocator()) {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Move constructor
        string(string&& other) noexcept : data_(std::move(other.data_)) {
            ensure_null_terminator();
            cstr = &data_[0];
        }

        explicit string(const _CharT* _cstr, size_t length) : data_() {
            data_.assign(_cstr, _cstr + length);
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Assignment operators

        // Move assignment operator
        string& operator=(string&& other) noexcept {
            if (this != &other) {
                data_ = std::move(other.data_);
                ensure_null_terminator();
                cstr = &data_[0];
            }
            return *this;
        }

        // Copy assignment operator
        string& operator=(const string& other) {
            if (this != &other) {
                data_ = other.data_;
                ensure_null_terminator();
                cstr = &data_[0];
            }
            return *this;
        }

        string& operator=(const _CharT* s) {
            data_.assign(s, s + strlen(s));
            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        string& operator=(std::initializer_list<_CharT> ilist) {
            data_ = ilist;
            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        // Size and capacity
        size_type size() const { return data_.size() > 0 ? data_.size() - 1 : 0; }
        size_type capacity() const { return data_.capacity(); }
        [[nodiscard]] bool empty() const {
            if (data_.empty()) return true;
            return data_[0] == '\0';
        }
        void reserve(size_type new_cap) {
            data_.reserve(new_cap);
            ensure_null_terminator();
            cstr = &data_[0];
        }
        void resize(size_type count) {
            data_.resize(count);
            ensure_null_terminator();
            cstr = &data_[0];
        }
        void resize(size_type count, _CharT ch) {
            data_.resize(count, ch);
            ensure_null_terminator();
            cstr = &data_[0];
        }

        // Iterators
        auto begin() { return data_.begin(); }
        auto begin() const { return data_.begin(); }
        auto end() { return data_.end() - 1; }  // Exclude the null terminator
        auto end() const { return data_.end() - 1; }

        // Element access
        reference operator[](size_type pos) { return data_[pos]; }
        const_reference operator[](size_type pos) const { return data_[pos]; }

        reference front() { return data_.front(); }
        const_reference front() const { return data_.front(); }

        reference back() { return data_[data_.size() - 2]; }
        const_reference back() const { return data_[data_.size() - 2]; }

        reference at(size_type pos) {
            if (pos >= size()) {
                // throw std::out_of_range("string::at: pos out of range");
                pos = pos % size();  // TODO throw?
            }
            return data_[pos];
        }

        const_reference at(size_type pos) const {
            if (pos >= size()) {
                // throw std::out_of_range("string::at: pos out of range");
                pos = pos % size();
            }
            return data_[pos];
        }

        // Modifiers
        void push_back(const _CharT& c) {
            data_.insert(data_.end() - 1, c);
            ensure_null_terminator();
            cstr = &data_[0];
        }

        void clear() {
            data_.clear();
            ensure_null_terminator();
            cstr = &data_[0];
        }

        string& operator+=(const _CharT& c) {
            data_.insert(data_.end() - 1, c);
            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        string& operator+=(const string& other) {
            data_.insert(data_.end() - 1, other.data_.begin(), other.data_.end() - 1);
            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        string& operator+=(const _CharT* s) {
            data_.insert(data_.end() - 1, s, s + strlen(s));
            return *this;
        }

        string& operator+=(std::initializer_list<_CharT> ilist) {
            data_.insert(data_.end() - 1, ilist);
            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        // Comparison operators
        bool operator==(const string& other) const { return strcmp(c_str(), other.c_str()) == 0; }

        bool operator==(const _CharT* other) const { return strcmp(static_cast<const _CharT*>(c_str()), other) == 0; }

        bool operator!=(const _CharT* other) const { return !(this->operator==(other)); }

        bool operator!=(const string& other) const { return *this != other; }
        bool operator<(const string& other) const { return data_ < other.data_; }
        bool operator<=(const string& other) const { return data_ <= other.data_; }
        bool operator>(const string& other) const { return data_ > other.data_; }
        bool operator>=(const string& other) const { return data_ >= other.data_; }

        size_type find(_CharT ch, size_type start = 0) const {
            for (size_type i = start; i < size(); ++i) {
                if (data_[i] == ch) return i;
            }
            return npos;
        }

        size_type find(const _CharT* str) const {
            size_type len = strlen(str);
            if (len == 0) return 0;
            if (len > size()) return npos;  // Search string longer than target

            for (size_type i = 0; i <= size() - len; ++i) {
                if (strcmp(&data_[i], str, len) == 0) return i;
            }
            return npos;
        }

        // Method to erase a single character at position `pos`
        string& erase(size_type pos = 0, size_type len = npos) {
            if (pos >= size()) {
                // If pos is out of range, do nothing
                return *this;
            }

            if (len == npos || pos + len > size()) {
                // If len is npos or goes beyond the string size, adjust len
                len = size() - pos;
            }

            data_.erase(data_.begin() + pos, data_.begin() + pos + len);
            ensure_null_terminator();
            cstr = &data_[0];

            return *this;
        }

        // Method to erase a character at iterator position `pos`
        auto erase(typename std::vector<_CharT, _Alloc<_CharT>>::iterator pos) {
            auto it = data_.erase(pos);
            ensure_null_terminator();
            cstr = &data_[0];
            return it;
        }

        // Method to erase a range of characters between iterator positions `first` and `last`
        auto erase(typename std::vector<_CharT, _Alloc<_CharT>>::iterator first, typename std::vector<_CharT, _Alloc<_CharT>>::iterator last) {
            auto it = data_.erase(first, last);
            ensure_null_terminator();
            cstr = &data_[0];
            return it;
        }

        // Split function
        template<typename AllocV>
        std::vector<string<_CharT, _Alloc>, AllocV> split(AllocV allocV, _CharT delimiter, bool ignoreEmpty = false) const {
            std::vector<string<_CharT, _Alloc>, AllocV> result(allocV);
            split_(result, delimiter, ignoreEmpty);
            return result;
        }

        std::vector<string<_CharT, _Alloc>, _Alloc<string<_CharT, _Alloc>>> split(_CharT delimiter, bool ignoreEmpty = false) const {
            std::vector<string<_CharT, _Alloc>, _Alloc<string<_CharT, _Alloc>>> result;
            split_(result, delimiter, ignoreEmpty);
            return result;
        }

        // C-string access
        const _CharT* c_str() const { return &data_[0]; }

        /**
         * Strips leading and trailing whitespace characters from the string.
         * @return A reference to the modified string.
         */
        string& strip() {
            // Lambda function to check if a character is a whitespace
            auto is_whitespace = [](const _CharT& ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r'; };

            // Find the position of the first non-whitespace character
            size_type start    = 0;
            while (start < size() && is_whitespace(data_[start])) { ++start; }

            // Find the position of the last non-whitespace character
            size_type end = size();
            while (end > start && is_whitespace(data_[end - 1])) { --end; }

            // Erase leading and trailing whitespace
            if (start > 0 || end < size()) {
                data_.erase(data_.begin() + end, data_.end() - 1);
                data_.erase(data_.begin(), data_.begin() + start);
            }

            ensure_null_terminator();
            cstr = &data_[0];
            return *this;
        }

        // Substring method
        string substr(size_type pos, size_type count = npos) const {
            if (pos > size()) pos = size();

            size_type len = (count == npos || pos + count > size()) ? size() - pos : count;
            return string(data_.begin() + pos, data_.begin() + pos + len);
        }

        // Method to find the last occurrence of any character from the set `chars`
        size_type find_last_of(const _CharT* chars, size_type pos = npos) const {
            if (pos >= size()) { pos = size() - 1; }

            for (size_type i = pos; i != npos; --i) {
                for (const _CharT* p = chars; *p != '\0'; ++p) {
                    if (data_[i] == *p) { return i; }
                }
            }
            return npos;
        }

        // Insert a single character at position `pos`
        string& insert(size_type pos, const _CharT& ch) {
            if (pos > size()) pos = size();

            data_.insert(data_.begin() + pos, ch);
            ensure_null_terminator();
            return *this;
        }

        // Insert a C-string at position `pos`
        string& insert(size_type pos, const _CharT* s) {
            size_type len = strlen(s);
            if (pos > size()) pos = size();

            data_.insert(data_.begin() + pos, s, s + len);
            ensure_null_terminator();
            return *this;
        }

        // Insert another string at position `pos`
        string& insert(size_type pos, const string& str) {
            if (pos > size()) pos = size();

            data_.insert(data_.begin() + pos, str.data_.begin(), str.data_.end() - 1);  // Exclude null terminator
            ensure_null_terminator();
            return *this;
        }

        // Insert a substring at position `pos`
        string& insert(size_type pos, const _CharT* s, size_type count) {
            if (pos > size()) pos = size();

            data_.insert(data_.begin() + pos, s, s + count);
            ensure_null_terminator();
            return *this;
        }

        // Insert single character at iterator position
        iterator insert(iterator pos, const _CharT& ch) {
            auto it = data_.insert(pos, ch);
            ensure_null_terminator();
            return it;
        }

        // Insert count copies of character at iterator position
        iterator insert(iterator pos, size_type count, const _CharT& ch) {
            auto it = data_.insert(pos, count, ch);
            ensure_null_terminator();
            return it;
        }

        string& toLower() {
            // Iterate only over actual characters, not the null terminator
            for (size_type i = 0; i < size(); ++i) { data_[i] = tolower(data_[i]); }
            ensure_null_terminator();
            return *this;
        }

        string& toUpper() {
            // Iterate only over actual characters, not the null terminator
            for (size_type i = 0; i < size(); ++i) { data_[i] = toupper(data_[i]); }
            ensure_null_terminator();
            return *this;
        }

    private:
        void ensure_null_terminator() {
            if (data_.empty() || data_.back() != '\0') { data_.push_back('\0'); }
        }

        template<typename AllocV>
        std::vector<string<_CharT, _Alloc>, AllocV> split_(std::vector<string<_CharT, _Alloc>, AllocV>& result, _CharT delimiter, bool ignoreEmpty) const {
            _Alloc alloc    = data_.get_allocator();
            size_type start = 0;
            size_type end   = find(delimiter);

            while (end != npos) {
                string<_CharT, _Alloc> token(data_.begin() + start, data_.begin() + end, alloc);
                if (!token.empty() || !ignoreEmpty) result.push_back(token);

                start = end + 1;
                end   = find(delimiter, start);
            }

            if (start < size()) {
                string<_CharT, _Alloc> token(data_.begin() + start, data_.end() - 1, alloc);
                if (!token.empty() || !ignoreEmpty) result.push_back(token);
            }

            return result;
        }

    public:
        // Definition of npos (not found position)
        static const size_type npos = static_cast<size_type>(-1);


    private:
        std::vector<_CharT, _Alloc<_CharT>> data_;
        const _CharT* cstr;
    };

    // Non-member functions
    // Concatenate two strings
    template<typename _CharT, template<typename> class _Alloc>
    string<_CharT, _Alloc> operator+(const string<_CharT, _Alloc>& lhs, const string<_CharT, _Alloc>& rhs) {
        string<_CharT, _Alloc> result(lhs);
        result += rhs;
        return result;
    }

    // Concatenate string with C-string
    template<typename _CharT, template<typename> class _Alloc>
    string<_CharT, _Alloc> operator+(const string<_CharT, _Alloc>& lhs, const _CharT* rhs) {
        string<_CharT, _Alloc> result(lhs);
        result += rhs;
        return result;
    }

    // Concatenate C-string with string
    template<typename _CharT, template<typename> class _Alloc>
    string<_CharT, _Alloc> operator+(const _CharT* lhs, const string<_CharT, _Alloc>& rhs) {
        string<_CharT, _Alloc> result(lhs);
        result += rhs;
        return result;
    }
}  // namespace PalmyraOS::types
