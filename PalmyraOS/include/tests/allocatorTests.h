
#pragma once

#include "core/std/error_handling.h"


namespace PalmyraOS::Tests::Allocator
{

  class ExceptionTester
  {
   public:
	  static void setup();
	  static void reset();
	  static bool exceptionOccurred();
   private:
	  // The custom page fault handler for testing purposes
	  static void RuntimeHandler(const char* message);
   private:
	  static bool exceptionOccurred_;
  };

  bool testVector();
  bool testVectorOfClasses();
  bool testMap();
  bool testUnorderedMap();
  bool testSet();
  bool testString();
  bool testQueue();

}

