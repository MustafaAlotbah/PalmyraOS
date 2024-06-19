
#include "tests/allocatorTests.h"
#include "core/memory/HeapAllocator.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
//#include <string>
#include "libs/string.h" // always on heap, has no problems


bool PalmyraOS::Tests::Allocator::ExceptionTester::exceptionOccurred_ = false;

void PalmyraOS::Tests::Allocator::ExceptionTester::setup()
{
	kernel::runtime::setOutOfRangeHandler(RuntimeHandler);
}
void PalmyraOS::Tests::Allocator::ExceptionTester::reset()
{
	exceptionOccurred_ = false;
	kernel::runtime::setOutOfRangeHandler(nullptr);
}

void PalmyraOS::Tests::Allocator::ExceptionTester::RuntimeHandler(const char* message)
{
	exceptionOccurred_ = true;
}
bool PalmyraOS::Tests::Allocator::ExceptionTester::exceptionOccurred()
{
	bool result        = exceptionOccurred_;
	exceptionOccurred_ = false;
	return result;
}

bool PalmyraOS::Tests::Allocator::testVector()
{
	bool result = true;

	// declare heap and allocator
	kernel::HeapManager        heapManager;
	kernel::HeapAllocator<int> allocator(heapManager);
	ExceptionTester::setup();


	// define our vector and pass the allocator to it
	std::vector<int, kernel::HeapAllocator<int>> vec(allocator);
	vec.reserve(2);
	vec.push_back(1);
	vec.push_back(2);

	volatile int var = vec.at(5);    // should throw an exception
	if (!ExceptionTester::exceptionOccurred()) result = false;

	vec.push_back(3);
	vec.push_back(4);
	vec.push_back(5);
	vec.push_back(6);
	vec.push_back(7);
	vec.push_back(8);

	var = vec.at(5);    // should not throw an exception
	if (ExceptionTester::exceptionOccurred()) result = false;


	ExceptionTester::reset();
	return result;
}

class SomeData
{
 public:
	uint32_t x{ 0 };
	uint32_t y{ 0 };
	SomeData() = default;
	SomeData(uint32_t x, uint32_t y) : x(x), y(y)
	{}

	REMOVE_COPY(SomeData);
	DEFINE_DEFAULT_MOVE(SomeData);
};

bool PalmyraOS::Tests::Allocator::testVectorOfClasses()
{
	bool result = true;

	// declare heap and allocator
	kernel::HeapManager             heapManager;
	kernel::HeapAllocator<SomeData> allocator(heapManager);
	ExceptionTester::setup();


	// define our vector and pass the allocator to it
	std::vector<SomeData, kernel::KernelHeapAllocator<SomeData>> vec;
	vec.reserve(2);
	vec.emplace_back(0x1111'1111, 0x1212'1212);
	vec.emplace_back(0x2222'2222, 0x2f2f'2f2f);

	volatile SomeData& var = vec.at(5);    // should throw an exception
	if (!ExceptionTester::exceptionOccurred()) result = false;

	vec.emplace_back(0x3333'3333, 0x3f3f'3f3f);
	vec.emplace_back(0x4444'4444, 0x4f4f'4f4f);
	vec.emplace_back(0x5555'5555, 0x5f5f'5f5f);
	vec.emplace_back(0x6666'6666, 0x6f6f'6f6f);
	vec.emplace_back(0x7777'7777, 0x7f7f'7f7f);
	vec.emplace_back(0x8888'8888, 0x8f8f'8f8f);

	volatile SomeData& var2 = vec.at(5);    // should not throw an exception
	if (ExceptionTester::exceptionOccurred()) result = false;


	ExceptionTester::reset();
	return result;
}

bool PalmyraOS::Tests::Allocator::testMap()
{
	// TODO
	// std::_Rb_tree_insert_and_rebalance(bool, std::_Rb_tree_node_base*, std::_Rb_tree_node_base*, std::_Rb_tree_node_base&)
	// std::_Rb_tree_decrement(std::_Rb_tree_node_base*)
	// std::_Rb_tree_increment(std::_Rb_tree_node_base*)

	bool                                                        result = true;
	kernel::HeapManager                                         heapManager;
	kernel::HeapAllocator<std::pair<const int, const SomeData>> allocator(heapManager);
	ExceptionTester::setup();

	std::map<int, SomeData, std::less<>, kernel::HeapAllocator<std::pair<const int, const SomeData>>> myMap(allocator);
	myMap[1] = { 0x1111'1111, 0x1212'1212 };
	myMap.insert(std::make_pair(2, SomeData(0x2222'2222, 0x2f2f'2f2f)));

	SomeData& value = myMap.at(5);    // should throw an exception
	if (!ExceptionTester::exceptionOccurred()) result = false;

	myMap[3] = { 0x3333'3333, 0x3f3f'3f3f };
	myMap[4] = { 0x4444'4444, 0x4f4f'4f4f };
	myMap[5] = { 0x5555'5555, 0x5f5f'5f5f };
	myMap[6] = { 0x6666'6666, 0x6f6f'6f6f };
	myMap[7] = { 0x7777'7777, 0x7f7f'7f7f };
	myMap[8] = { 0x8888'8888, 0x8f8f'8f8f };

	SomeData& var2 = myMap.at(5);    // should not throw an exception
	if (ExceptionTester::exceptionOccurred()) result = false;

	myMap.erase(1);

	ExceptionTester::reset();
	return result;
}
bool PalmyraOS::Tests::Allocator::testUnorderedMap()
{
	// TODO
	//
	// std::__detail::_Prime_rehash_policy::_M_need_rehash(unsigned int, unsigned int, unsigned int) const

	bool result = true;
//	kernel::HeapManager heapManager;
//	kernel::HeapAllocator<std::pair<const int, const int>> allocator(heapManager);
//	ExceptionTester::setup();
//
//	std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, kernel::HeapAllocator<std::pair<const int, const int>>> myMap(allocator);
//
//	myMap[1] = 11;
//	myMap[2] = 12;
//
//	int value = myMap.at(5);    // should throw an exception
//	if (!ExceptionTester::exceptionOccurred()) result = false;
//
//	myMap[3] = 13;
//	myMap[4] = 14;
//	myMap[5] = 15;
//	myMap[6] = 16;
//	myMap[7] = 17;
//	myMap[8] = 18;
//
//	value = myMap.at(5);    // should not throw an exception
//	if (ExceptionTester::exceptionOccurred()) result = false;
//
//	ExceptionTester::reset();
	return result;
}

bool PalmyraOS::Tests::Allocator::testSet()
{
	// TODO
	// std::_Rb_tree_decrement(std::_Rb_tree_node_base*)
	// std::_Rb_tree_insert_and_rebalance(bool, std::_Rb_tree_node_base*, std::_Rb_tree_node_base*, std::_Rb_tree_node_base&)

	bool result = true;
	kernel::HeapManager heapManager;
	kernel::HeapAllocator<int> allocator(heapManager);
	ExceptionTester::setup();

	std::set<int, std::less<int>, kernel::HeapAllocator<int>> mySet(allocator);
	mySet.insert(1);
	mySet.insert(2);

	if (mySet.find(5) != mySet.end()) result = false;

	mySet.insert(3);
	mySet.insert(4);
	mySet.insert(5);
	mySet.insert(6);
	mySet.insert(7);
	mySet.insert(8);

	if (mySet.find(5) == mySet.end()) result = false;

	ExceptionTester::reset();
	return result;
}

bool PalmyraOS::Tests::Allocator::testString()
{
	// TODO (Heap overwriting beyond reserved, not reallocating)

	using String = types::string<char, kernel::HeapAllocator<char>>;

	bool result = true;
	kernel::HeapManager heapManager;
	kernel::HeapAllocator<char> allocator(heapManager);
	kernel::HeapAllocator<String> stringAllocator(heapManager);
	ExceptionTester::setup();

	String str(allocator);
	str.reserve(50);     // force memory on heap
	str = "hello";

	volatile char ch = str.at(10);    // should throw an exception
//	if (!ExceptionTester::exceptionOccurred()) result = false;

	str = "hello world!";

	ch = str.at(5);    // should not throw an exception
	if (ExceptionTester::exceptionOccurred()) result = false;

	str = "123456789 123456789 123456789 123456789 123456789 123456789"; // not working when more than reserved

	auto subs = str.split(stringAllocator, ' ');


	ExceptionTester::reset();
	return result;
}




