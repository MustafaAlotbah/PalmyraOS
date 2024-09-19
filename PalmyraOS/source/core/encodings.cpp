
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
		  auto     utf8_char   = static_cast<unsigned char>(utf8_string[i]);
		  uint32_t codepoint   = 0;
		  size_t   extra_bytes = 0;

		  if (utf8_char <= 0x7F)
		  {
			  // 1-byte sequence
			  codepoint   = utf8_char;
			  extra_bytes = 0;
		  }
		  else if ((utf8_char & 0xE0) == 0xC0)
		  {
			  // 2-byte sequence
			  codepoint   = utf8_char & 0x1F;
			  extra_bytes = 1;
		  }
		  else if ((utf8_char & 0xF0) == 0xE0)
		  {
			  // 3-byte sequence
			  codepoint   = utf8_char & 0x0F;
			  extra_bytes = 2;
		  }
		  else if ((utf8_char & 0xF8) == 0xF0)
		  {
			  // 4-byte sequence
			  codepoint   = utf8_char & 0x07;
			  extra_bytes = 3;
		  }
		  else
		  {
			  // Invalid first byte
			  // Handle error or skip
			  ++i;
			  continue;
		  }

		  // Ensure there are enough bytes left
		  if (i + extra_bytes >= utf8_string.size())
		  {
			  // Incomplete sequence
			  // Handle error or skip
			  break;
		  }

		  // Process continuation bytes
		  for (size_t j = 1; j <= extra_bytes; ++j)
		  {
			  auto cc   = static_cast<unsigned char>(utf8_string[i + j]);
			  if ((cc & 0xC0) != 0x80)
			  {
				  // Invalid continuation byte
				  // Handle error or skip
				  i += j;
				  continue;
			  }
			  codepoint = (codepoint << 6) | (cc & 0x3F);
		  }

		  i += extra_bytes + 1;

		  if (codepoint <= 0xFFFF)
		  {
			  if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
			  {
				  // Reserved for surrogate pairs in UTF-16
				  // Handle error or skip
				  continue;
			  }
			  utf16le_string.push_back(static_cast<uint16_t>(codepoint));
		  }
		  else if (codepoint <= 0x10FFFF)
		  {
			  // Encode as surrogate pair
			  codepoint -= 0x10000;
			  uint16_t high_surrogate = 0xD800 | ((codepoint >> 10) & 0x3FF);
			  uint16_t low_surrogate  = 0xDC00 | (codepoint & 0x3FF);
			  utf16le_string.push_back(high_surrogate);
			  utf16le_string.push_back(low_surrogate);
		  }
		  else
		  {
			  // Codepoint out of range
			  // Handle error or skip
			  continue;
		  }
	  }
	  return utf16le_string;
  }


}