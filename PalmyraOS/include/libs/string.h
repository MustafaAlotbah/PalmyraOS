#include "core/definitions.h"    // stdint + size_t
#include "libs/stdio.h"


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
char* strtok(char *s, const char *delim);

/**
 * Locates the first occurrence of character `c` in the string `s`.
 * @param s Pointer to the null-terminated byte string to analyze.
 * @param c Character to search for, cast to an `int`, automatically converted to `char`.
 * @return A pointer to the first occurrence of `c` in `s`, or NULL if `c` is not found.
 */
char* strchr(const char *s, int c);

/**
 * Extracts tokens from the string `*stringp` that are delimited by characters in the string `delim`.
 * @param stringp Address of a pointer that points to a string to be tokenized; it is updated to point past the token.
 * @param delim Null-terminated string containing the delimiter characters.
 * @return A pointer to the extracted token, or NULL if there are no more tokens.
 */
char* strsep(char **stringp, const char *delim);

/**
 * Compares the two strings `s1` and `s2`.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @return An integer less than, equal to, or greater than zero, if `s1` is found, respectively, to be less than, to match, or be greater than `s2`.
 */
int strcmp(const char *s1, const char *s2);

/**
 * Compares up to `n` characters of the string `s1` to those of the string `s2`.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be greater than the first `n` bytes of `s2`.
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * Compares the two strings `s1` and `s2` ignoring case.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @return An integer less than, equal to, or greater than zero, if `s1` is found, respectively, to be less than, to match, or be greater than `s2`.
 */
int strcasecmp(const char *s1, const char *s2);

/**
 * Compares up to `n` characters of the string `s1` to those of the string `s2`, ignoring case.
 * @param s1 Pointer to the first null-terminated byte string.
 * @param s2 Pointer to the second null-terminated byte string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, if the first `n` bytes of `s1` are found, respectively, to be less than, to match, or be greater than the first `n` bytes of `s2`.
 */
int strncasecmp(const char *s1, const char *s2, size_t n);

/**
 * Copies the C string pointed by `src` into the array pointed by `dest`.
 * @param dest Pointer to the buffer where the content is to be copied.
 * @param src Pointer to the null-terminated byte string to be copied.
 * @return A pointer to `dest`.
 */
char* strcpy(char *dest, const char *src);

/**
 * Copies up to `n` characters from the C string pointed by `src` to `dest`.
 * @param dest Pointer to the buffer where the content is to be copied.
 * @param src Pointer to the null-terminated byte string to be copied.
 * @param n Number of characters to copy.
 * @return A pointer to `dest`.
 */
char* strncpy(char *dest, const char *src, size_t n);

/**
 * Appends the string pointed to by `src` to the end of the string pointed to by `dest`.
 * @param dest Pointer to the null-terminated byte string to be appended to.
 * @param src Pointer to the null-terminated byte string to append.
 * @return A pointer to the resulting string `dest`.
 */
char* strcat(char *dest, const char *src);

/**
 * Searches the string `s` for the first occurrence of any of the characters in the string `accept`.
 * @param s The null-terminated string to be scanned.
 * @param accept The null-terminated string containing the characters to search for.
 * @return A pointer to the character in `s` that matches one of the characters in `accept`, or NULL if no such character is found.
 */
char* strpbrk(const char* s, const char* accept);

namespace PalmyraOS::types
{

template<size_t MAX_LEN = 1024>
class string {
public:
	// empty string
	string()
			:data{'\0'}, length(0) { }

	// Constructor from const char*
	explicit string(const char* cstr)
	{
		length = strlen(cstr);
		if (length >= MAX_LEN) length = MAX_LEN - 1;
		strncpy(data, cstr, length);
		data[length] = '\0';
	};

	// Copy constructor
	string(const string& other)
			: length(other.length) {
		strncpy(data, other.data, length);
		data[length] = '\0'; // Ensure null-termination
	}

	// Destructor
	~string() = default;

	// Size accessor
	inline size_t size() const { return length; }

	// C-style string accessor
	[[nodiscard]] const char* c_str() const {
		return data;
	}

	// Assignment operator from another string instance
	string& operator=(const string& other) {
		if (this != &other) {
			length = other.length;
			strncpy(data, other.data, length);
			data[length] = '\0';
		}
		return *this;
	}

	// Assignment operator from C-style string
	string& operator=(const char* cstr) {
		length = strlen(cstr);
		if (length > MAX_LEN - 1) length = MAX_LEN - 1;
		strncpy(data, cstr, length);
		data[length] = '\0';
		return *this;
	}

	// Concatenation operator
	string operator+(const string& other) const
	{
		string result;
		size_t totalLength = length+other.length;
		if (totalLength>=MAX_LEN) totalLength = MAX_LEN-1;
		strncpy(result.data, data, length);
		strncpy(result.data+length, other.data, totalLength-length);
		result.data[totalLength] = '\0';
		result.length = totalLength;
		return result;
	}


	// Append function
	void append(const char* cstr) {
		size_t appendLen = strlen(cstr);
		size_t newLength = length + appendLen;
		if (MAX_LEN - 1 < newLength) newLength = MAX_LEN - 1;
		strncpy(data + length, cstr, newLength - length);
		data[newLength] = '\0';
		length = newLength;
	}

	// Append a single character
	void append(char c) {
		if (length < MAX_LEN - 1) {
			data[length++] = c;
			data[length] = '\0';
		}
	}

	string& operator<<(const char* cstr) {
		append(cstr);
		return *this;
	}

	string& operator<<(uint64_t num) {
		char buffer[1024];  // Adjust size as necessary for your needs
		snprintf(buffer, sizeof(buffer), "%d", num);
		append(buffer);
		return *this;
	}

	// Subscript operator for non-const strings
	char& operator[](size_t index) {
		if (index >= length) {
			static char dummy = '\0';
			return dummy; // Optionally handle out of range access or throw an exception
		}
		return data[index];
	}

	// Subscript operator for const strings
	const char& operator[](size_t index) const {
		if (index >= length) {
			static const char dummy = '\0';
			return dummy; // Optionally handle out of range access or throw an exception
		}
		return data[index];
	}

	// compare with const char*
	bool operator==(const char* rhs) const {
		if (strlen(rhs) != length) {
			return false;  // Different lengths mean the strings are not equal
		}
		for (size_t i = 0; i < length; i++) {
			if (data[i] != rhs[i]) {
				return false;  // Any differing character means the strings are not equal
			}
		}
		return true;  // If we reach here, all characters matched
	}

	// Remove the last character from the string
	void pop_back() {
		if (length > 0) {
			length--;
			data[length] = '\0';
		}
	}


private:
	char data[MAX_LEN]{'\0'};
	size_t length{0};
};

} // namespace mustyOS::types

