
#include <elf.h>

#include "libs/memory.h"    // memcpy
#include "core/tasks/elf.h"
#include "core/kernel.h"


int PalmyraOS::kernel::loadElf(PalmyraOS::kernel::KVector<uint8_t>& elfFileContent)
{

	// TODO remove text renderer
	auto& cout = *kernel::textRenderer_ptr;

	cout << "Elf Loader: \n" << SWAP_BUFF();

	// Ensure the ELF file is large enough to contain the header
	if (elfFileContent.size() < EI_NIDENT)
	{
		cout << "Error: ELF file is too small to contain a valid header.\n" << SWAP_BUFF();
		return -1;
	}


	// Read the ELF identification bytes
	unsigned char e_ident[EI_NIDENT];
	memcpy(e_ident, elfFileContent.data(), EI_NIDENT);

	// Verify the ELF magic number
	if (e_ident[EI_MAG0] != ELFMAG0 ||
		e_ident[EI_MAG1] != ELFMAG1 ||
		e_ident[EI_MAG2] != ELFMAG2 ||
		e_ident[EI_MAG3] != ELFMAG3)
	{
		cout << "Error: ELF file has an invalid magic number.\n" << SWAP_BUFF();
		return -1;
	}
	// Check the ELF class (32-bit or 64-bit)
	if (e_ident[EI_CLASS] != ELFCLASS32)
	{
		cout << "Error: ELF file is not 32-bit (ELFCLASS32).\n" << SWAP_BUFF();
		return -1;
	}

	// Check the data encoding (little-endian or big-endian)
	if (e_ident[EI_DATA] != ELFDATA2LSB)
	{
		cout << "Error: ELF file is not little-endian (ELFDATA2LSB).\n" << SWAP_BUFF();
		return -1;
	}

	// Check the ELF version
	if (e_ident[EI_VERSION] != EV_CURRENT)
	{
		cout << "Error: Unsupported ELF version.\n" << SWAP_BUFF();
		return -1;
	}

	// Now cast the elfFileContent data to an Elf32_Ehdr structure for easier access to the fields
	const auto* elfHeader = reinterpret_cast<const Elf32_Ehdr*>(elfFileContent.data());

	// Log the e_type and e_machine values
	cout << "ELF e_type: " << elfHeader->e_type << "\n" << SWAP_BUFF();
	cout << "ELF e_machine: " << elfHeader->e_machine << "\n" << SWAP_BUFF();
	cout << "ELF e_version: " << elfHeader->e_version << "\n" << SWAP_BUFF();
	cout << "ELF e_entry: " << HEX() << elfHeader->e_entry << DEC() << "\n" << SWAP_BUFF();
	cout << "ELF e_ehsize: " << elfHeader->e_ehsize << "\n" << SWAP_BUFF();
	cout << "ELF e_flags: " << elfHeader->e_flags << "\n" << SWAP_BUFF();

	// Check if the ELF file is an executable
	if (elfHeader->e_type != ET_EXEC)
	{
		cout << "Error: ELF file is not an executable (ET_EXEC).\n" << SWAP_BUFF();
		return -1;
	}

	// Check if the ELF file is for the Intel 80386 architecture
	if (elfHeader->e_machine != EM_386)
	{
		cout << "Error: ELF file is not for the Intel 80386 architecture (EM_386).\n" << SWAP_BUFF();
		return -1;
	}


	cout << "All validations were successful.\n" << SWAP_BUFF();


	// Loop through the program headers and display information
	const auto* programHeader = reinterpret_cast<const Elf32_Phdr*>(elfFileContent.data() + elfHeader->e_phoff);
	cout << "Program Headers: \n";
	for (int i = 0; i < elfHeader->e_phnum; ++i)
	{
		cout << DEC() << i << HEX()
			 << ", Type: " << programHeader[i].p_type << ", Offset: " << programHeader[i].p_offset
			 << ", VAddr: " << programHeader[i].p_vaddr << ", PAddr: " << programHeader[i].p_paddr
			 << ", FileSize: " << programHeader[i].p_filesz << ", MemSize: " << programHeader[i].p_memsz
			 << ", Flags: " << programHeader[i].p_flags << ", Align: " << programHeader[i].p_align << "\n"
			 << SWAP_BUFF();
	}


	// Locate the section header string table
	const auto* sectionHeaders = reinterpret_cast<const Elf32_Shdr*>(elfFileContent.data() + elfHeader->e_shoff);
	const char* shstrtab       = reinterpret_cast<const char*>(elfFileContent.data()
		+ sectionHeaders[elfHeader->e_shstrndx].sh_offset);

	// Loop through the section headers and display information
	cout << "Section Headers: \n";
	for (int i = 0; i < elfHeader->e_shnum; ++i)
	{
		const char* sectionName = shstrtab + sectionHeaders[i].sh_name;
		cout << DEC() << i << HEX()
			 << ", Name: " << sectionName << ", Type: " << sectionHeaders[i].sh_type
			 << ", Flags: " << sectionHeaders[i].sh_flags << ", Addr: " << sectionHeaders[i].sh_addr
			 << ", Offset: " << sectionHeaders[i].sh_offset << ", Size: " << sectionHeaders[i].sh_size
			 << ", Link: " << sectionHeaders[i].sh_link << ", Info: " << sectionHeaders[i].sh_info
			 << ", Addralign: " << sectionHeaders[i].sh_addralign << ", Entsize: " << sectionHeaders[i].sh_entsize
			 << "\n" << SWAP_BUFF();

	}


	while (true);
	return 0;
}








