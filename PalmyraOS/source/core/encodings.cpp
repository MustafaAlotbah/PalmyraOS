
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
}