
#pragma once

#include "core/definitions.h"    // stdint + size_t
#include "libs/stdio.h"


namespace PalmyraOS::types
{

  template<typename T, size_t BUFFER_SIZE = 50>
  class CircularBuffer
  {
   public:
	  CircularBuffer()
	  {
		  for (size_t i = 0; i < BUFFER_SIZE; ++i)
		  {
			  backBuffer[i]  = 0;
			  frontBuffer[i] = 0;
		  }
	  }

	  void clear()
	  {
		  start = 0;
		  end   = 0;

		  for (size_t i = 0; i < BUFFER_SIZE; ++i)
		  {
			  backBuffer[i] = 0;
		  }
	  }

	  void append(const T* str, size_t length)
	  {
		  if (length > BUFFER_SIZE)
		  {
			  // Only keep the last BUFFER_SIZE characters
			  str += length - BUFFER_SIZE;
			  length = BUFFER_SIZE;
		  }

		  for (size_t i = 0; i < length; ++i)
		  {
			  // Stop if zero character is reached
			  if (str[i] == 0) break;

			  backBuffer[end] = str[i];
			  end = (end + 1) % BUFFER_SIZE;

			  if (end == start)
			  {
				  // Move start forward if we overlap
				  start = (start + 1) % BUFFER_SIZE;
			  }
		  }
	  }

	  void append(T ch)
	  {
		  backBuffer[end] = ch;
		  end = (end + 1) % BUFFER_SIZE;

		  if (end == start)
		  {
			  // Move start forward if we overlap
			  start = (start + 1) % BUFFER_SIZE;
		  }
	  }

	  void backspace()
	  {
		  if (start == end) return;

		  if (end == 0)
		  {
			  end = BUFFER_SIZE - 1;
		  }
		  else
		  {
			  --end;
		  }

		  backBuffer[end] = 0; // clear the character
	  }

	  const T* get() const
	  {
		  size_t current = start;
		  size_t index   = 0;

		  while (current != end)
		  {
			  frontBuffer[index++] = backBuffer[current];
			  current = (current + 1) % BUFFER_SIZE;
		  }
		  frontBuffer[index] = 0;  // Null-terminate the string

		  return frontBuffer;
	  }

	  size_t capacity() const
	  {
		  return BUFFER_SIZE;
	  }

	  size_t size() const
	  {
		  if (end >= start)
		  {
			  return end - start;
		  }
		  else
		  {
			  return BUFFER_SIZE - start + end;
		  }
	  }

   private:
	  T         backBuffer[BUFFER_SIZE]{};
	  mutable T frontBuffer[BUFFER_SIZE]{};
	  size_t    start = 0;
	  size_t    end   = 0;
  };


}