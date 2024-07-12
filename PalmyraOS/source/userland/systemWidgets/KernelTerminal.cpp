
#include "userland/systemWidgets/KernelTermina.h"

#include "palmyraOS/unistd.h"       // Include PalmyraOS system calls
#include "palmyraOS/time.h"            // For sleeping
#include "palmyraOS/stdlib.h"       // For dynamic memory management
#include "palmyraOS/stdio.h"        // For standard input/output functions: printf, perror
#include "palmyraOS/HeapAllocator.h"// C++ heap allocator for efficient memory management
#include "libs/circularBuffer.h"    // For efficient FIFO buffer implementation

#include "libs/string.h"            // strlen

// TODO: move these to libs (or put them in palmyraOS)
#include "core/FrameBuffer.h"        // FrameBuffer
#include "core/VBE.h"                // for Brush, TextRenderer
#include "core/Font.h"               // For Fonts


// Use the types namespace from PalmyraOS: CircularBuffer, UserHeapManager
using namespace PalmyraOS::types;


namespace PalmyraOS::Userland::builtin::KernelTerminal
{

  // Typedefs for buffer types simplify declarations
  using StdoutType = CircularBuffer<char, 512>;
  using StdinType = CircularBuffer<char>;

  // Function declarations for command parsing and execution
  void parseCommand(UserHeapManager& heap, CircularBuffer<char>& input, types::UVector<types::UString<char>>& tokens);
  void executeCommand(UserHeapManager& heap, StdinType& input, StdoutType& output);

  int main(uint32_t argc, char** argv)
  {
	  // Initialize dynamic memory allocator for the application
	  UserHeapManager heap;

	  // Set initial window position and dimensions
	  size_t x = 240, y = 140, width = 640, height = 480;

	  // Allocate a large buffer for double buffering in graphics
	  size_t total_size = 301 * 4096;
	  void* backBuffer = malloc(total_size);
	  if (backBuffer == MAP_FAILED) perror("Failed to map memory\n");
	  else printf("Success to map memory\n");

	  // Create and set up the main application window
	  uint32_t* frontBuffer = nullptr;
	  uint32_t window_id = initializeWindow(&frontBuffer, x, y, width, height);
	  if (window_id == 0) perror("Failed to initialize window\n");
	  else printf("Success to initialize window\n");

	  // Initialize graphics objects for rendering
	  PalmyraOS::kernel::FrameBuffer  frameBuffer(width, height, frontBuffer, (uint32_t*)backBuffer);
	  PalmyraOS::kernel::Brush        brush(frameBuffer);
	  PalmyraOS::kernel::TextRenderer textRenderer(frameBuffer, PalmyraOS::fonts::FontManager::getFont("Arial-12"));
	  textRenderer.setPosition(5, 0);

	  // Setup terminal buffers and initial prompt
	  auto stdoutBufferPtr = heap.createInstance<KernelTerminal::StdoutType>();
	  auto stdinBufferPtr  = heap.createInstance<KernelTerminal::StdinType>();
	  KernelTerminal::StdoutType& stdoutBuffer = *stdoutBufferPtr;
	  KernelTerminal::StdinType & stdinBuffer  = *stdinBufferPtr;

	  uint64_t                             count = 0;
	  types::UVector<types::UString<char>> tokens(heap);

	  // TODO check for bad allocation
	  stdoutBuffer.append("PalmyraOS$ ", 11);
	  while (true)
	  {
		  count++;

		  // Render the terminal UI frame
		  brush.fill(PalmyraOS::Color::Black);
		  brush.fillRectangle(0, 0, width, 20, PalmyraOS::Color::DarkRed);
		  brush.drawFrame(0, 0, width, height, PalmyraOS::Color::White);
		  brush.drawHLine(0, width, 20, PalmyraOS::Color::White);
		  textRenderer << PalmyraOS::Color::White;
		  textRenderer.setCursor(1, 1);
		  textRenderer << "Kernel Terminal\n";
		  textRenderer.reset();
		  textRenderer.setCursor(1, 21);

		  // Render the prompt
		  textRenderer << PalmyraOS::Color::White;
		  textRenderer << stdoutBuffer.get();

		  // Another loop to catch all keyboard events
		  KeyboardEvent event;
		  while (true)
		  {
			  event = nextKeyboardEvent(window_id);        // Fetch the next event
			  if (event.key == '\0') break;                         // If no key is pressed, break the loop
			  else if (event.key == 8) stdinBuffer.backspace();     // Handle backspace for corrections
			  else stdinBuffer.append(event.key);                // Append any other key to our input buffer

			  // When Enter is pressed, process the command
			  if (event.key == '\n')
			  {
				  // Get the input from our buffer
				  auto input = stdinBuffer.get();

				  // Echo the input to the output buffer
				  stdoutBuffer.append(input, strlen(input));

				  // Time to execute the command
				  executeCommand(heap, stdinBuffer, stdoutBuffer);

				  // Clear the input buffer for the next command
				  stdinBuffer.clear();

				  // Prompt the user again
				  stdoutBuffer.append("PalmyraOS$ ", 11);
			  }
		  }

		  // Display the current input and a blinking cursor for feedback
		  textRenderer << PalmyraOS::Color::LightGreen << stdinBuffer.get() << PalmyraOS::Color::White;
		  if ((count >> 5) % 2) textRenderer << "_";


		  // Refresh the display and yield control to other processes
		  textRenderer.reset();
		  frameBuffer.swapBuffers();

		  // Be a good citizen and yield some CPU time to other processes
		  sched_yield();
	  }


	  return 0;
  }

  void parseCommand(UserHeapManager& heap, CircularBuffer<char>& input, types::UVector<types::UString<char>>& tokens)
  {
	  // Fetch the command string from input buffer
	  char* command = (char*)input.get();

	  // Begin tokenizing the command by spaces and newlines
	  char* token = strtok(command, " \n");

	  // As long as there are tokens...
	  while (token != nullptr)
	  {
		  // Create a dynamic string for each token
		  auto _token = types::UString<char>(heap);

		  // Assign the token to our dynamic string and push it
		  _token = token;
		  tokens.push_back(_token);

		  // Continue to the next token
		  token = strtok(nullptr, " \n");
	  }
  }

  void executeCommand(UserHeapManager& heap, StdinType& input, StdoutType& output)
  {
	  // Prepare a vector to hold our tokens
	  types::UVector<types::UString<char>> tokens(heap);

	  // Parse the command from input
	  parseCommand(heap, input, tokens);

	  // If there are no tokens, do nothing
	  if (tokens.empty()) return;

	  // ECHO
	  if (tokens[0] == "echo")
	  {
		  // Loop through all tokens except the first
		  for (int i = 1; i < tokens.size(); ++i)
		  {
			  // Append each token to output
			  output.append(tokens[i].c_str(), tokens[i].size());

			  // Add a space between tokens
			  output.append(' ');
		  }
		  output.append('\n');
		  return;
	  }

	  // CLEAR
	  if (tokens[0] == "clear")
	  {
		  output.clear();    // Simply clear the output buffer
		  return;
	  }

	  // CAT
	  if (tokens[0] == "cat")
	  {

		  if (tokens.size() <= 1)
		  {
			  // Remind the user to specify a file path
			  output.append("No path was provided\n", 22);
			  return;
		  }

		  // Attempt to open the file specified by the user
		  int fileDescriptor = open(tokens[1].c_str(), 0);
		  if (fileDescriptor < 0)
		  {
			  // Inform if the file couldn't be opened, perhaps it doesn't exist
			  output.append("cat: ", 6);
			  output.append(tokens[1].c_str(), tokens[1].size());
			  output.append(": No such file or directory\n", 29);
			  return;
		  }

		  // Allocate a buffer to read the file content
		  auto buffer = (char*)heap.alloc(512 * sizeof(char));

		  // Read up to 512 characters from the file
		  int bytesRead = read(fileDescriptor, buffer, 512);

		  // Close the file to tidy up and prevent resource leaks
		  close(fileDescriptor);

		  // Output the content read from the file and a newline to separate commands
		  output.append(buffer, bytesRead);
		  output.append('\n');
		  heap.free(buffer);            // Free up memory used by the buffer
		  return;
	  }

	  // LS
	  if (tokens[0] == "ls")
	  {

		  if (tokens.size() <= 1)
		  {
			  // Users need to provide a directory path
			  output.append("No path was provided\n", 22);
			  return;
		  }

		  // Try opening the directory specified
		  int fileDescriptor = open(tokens[1].c_str(), 0);
		  if (fileDescriptor < 0)
		  {
			  // If directory cannot be opened, inform the user
			  output.append("ls: ", 6);
			  output.append(tokens[1].c_str(), tokens[1].size());
			  output.append(": No such file or directory\n", 29);
			  return;
		  }

		  // Prepare a buffer to hold directory entries
		  auto buffer = (char*)heap.alloc(512 * sizeof(char));

		  {

			  // Read directory entries
			  linux_dirent* DirectoryEntry;
			  int  currentByteIndex;
			  char DentryType;            // File, Directory, ...
			  int  bytesRead = getdents(fileDescriptor, (linux_dirent*)buffer, 512);

			  // Process each entry in the directory
			  for (currentByteIndex = 0; currentByteIndex < bytesRead;)
			  {

				  DirectoryEntry = (struct linux_dirent*)(buffer + currentByteIndex);
				  DentryType     = *(buffer + currentByteIndex + DirectoryEntry->d_reclen - 1);

				  // Append directory names with a slash to indicate they are directories
				  output.append(DirectoryEntry->d_name, strlen(DirectoryEntry->d_name));

				  if (DentryType == DT_DIR) output.append("/  ", 4);
				  else output.append("  ", 3);

				  // Move to the next entry
				  currentByteIndex += DirectoryEntry->d_reclen;
			  }

			  // Add a newline in the end
			  if (bytesRead > 0) output.append('\n');
		  }

		  // Close the directory and free the buffer
		  close(fileDescriptor);
		  heap.free(buffer);
		  return;
	  }


	  // SLEEP
	  if (tokens[0] == "sleep")
	  {
		  if (tokens.size() <= 1)
		  {
			  // Require an interval
			  output.append("No time interval was provided!\n", 32);
			  return;
		  }

		  // Parse the interval (assume it is an integer for now) TODO: float
		  char* end;
		  long int seconds;
		  seconds = strtol(tokens[1].c_str(), &end, 10);
		  if (*end != '\0')
		  {
			  output.append("Please provide an integer!\n", 28);
			  return;
		  }

		  // Sleep
		  timespec req{};
		  req.tv_sec  = seconds;
		  req.tv_nsec = 0;
		  clock_nanosleep(CLOCK_REALTIME, 0, &req, nullptr);
		  return;
	  }

		  // Unknown command
	  else
	  {
		  output.append("Unknown command: '", 18);
		  output.append(tokens[0].c_str(), tokens[0].size());
		  output.append("'\n", 2);
		  return;
	  }
  }

}