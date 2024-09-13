
#include "core/encodings.h"


namespace PalmyraOS::kernel
{
  // Function to convert UTF-16LE encoded data to UTF-8
  KString utf16le_to_utf8(const KWString& utf16le_string)
  {
	  // TODO handle utf16-le better
	  KString      utf8_string;
	  for (wchar_t utf16_char : utf16le_string)
	  {
		  if (utf16_char < 0x80)
		  {
			  utf8_string += static_cast<char>(utf16_char);
		  }
		  else if (utf16_char < 0x800)
		  {
			  utf8_string += static_cast<char>((utf16_char >> 6) | 0xC0);
			  utf8_string += static_cast<char>((utf16_char & 0x3F) | 0x80);
		  }
		  else
		  {
			  utf8_string += static_cast<char>((utf16_char >> 12) | 0xE0);
			  utf8_string += static_cast<char>(((utf16_char >> 6) & 0x3F) | 0x80);
			  utf8_string += static_cast<char>((utf16_char & 0x3F) | 0x80);
		  }
	  }
	  return utf8_string;
  }

  KWString utf8_to_utf16le(const KString& utf8_string)
  {
	  KWString utf16le_string;
	  size_t   i = 0;
	  while (i < utf8_string.size())
	  {
		  unsigned char utf8_char = utf8_string[i];
		  wchar_t       utf16_char;
		  if (utf8_char < 0x80)
		  {
			  // 1-byte sequence
			  utf16_char = utf8_char;
			  ++i;
		  }
		  else if ((utf8_char & 0xE0) == 0xC0)
		  {
			  // 2-byte sequence
			  utf16_char = (utf8_char & 0x1F) << 6;
			  utf16_char |= (utf8_string[i + 1] & 0x3F);
			  i += 2;
		  }
		  else if ((utf8_char & 0xF0) == 0xE0)
		  {
			  // 3-byte sequence
			  utf16_char = (utf8_char & 0x0F) << 12;
			  utf16_char |= (utf8_string[i + 1] & 0x3F) << 6;
			  utf16_char |= (utf8_string[i + 2] & 0x3F);
			  i += 3;
		  }
		  else
		  {
			  // Invalid UTF-8 sequence
			  // Handle error or throw exception
			  // TODO throw std::runtime_error("Invalid UTF-8 sequence");
		  }
		  utf16le_string += utf16_char;
	  }
	  return utf16le_string;
  }


}