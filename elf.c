#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "elf.h"

void elf_objdump(void* data) {
	elf32_ehdr *ehdr = (elf32_ehdr*) data;

	/* Make sure the file ain't fucked */
	assert(ehdr->e_ident[0] == ELF_MAGIC);
	//assert(ehdr->e_machine 	== EM_386);
	//assert(ehdr->e_type		== ET_EXEC);

	/* Parse the program headers */
	elf32_phdr* phdr 		= (uint32_t) data + ehdr->e_phoff;
	elf32_phdr* last_phdr 	= (uint32_t) phdr + (ehdr->e_phentsize * ehdr->e_phnum);
	while(phdr < last_phdr) {
		printf("LOAD:\toff 0x%x\tvaddr\t0x%x\tpaddr\t0x%x\n\t\tfilesz\t%d\tmemsz\t%d\talign\t%d\t\n",
		 	phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz, phdr->p_align);
		phdr++;
	} 

	/* Parse the section headers */
	elf32_shdr* shdr 		= (uint32_t) data + ehdr->e_shoff;
	elf32_shdr* sh_str		= (uint32_t) shdr + (ehdr->e_shentsize * ehdr->e_shstrndx);
	elf32_shdr* last_shdr 	= (uint32_t) shdr + (ehdr->e_shentsize * ehdr->e_shnum);

	char* string_table 		= (uint32_t) data + sh_str->sh_offset;

	shdr++;					// Skip null entry
	int q = 0;

	printf("Sections:\nIdx   Name\tSize\t\tAddress \tOffset\tAlign\n");
	while (shdr < last_shdr) {	
		printf("%d:   %s\t%x\t%x\t%d\t%x\n", 
			q++, string_table + shdr->sh_name, shdr->sh_size,
			shdr->sh_addr, shdr->sh_offset, shdr->sh_addralign);
		shdr++;
	}

}

int main(int argc, char* argv[]) {
	if (argc < 2)
		return -1;
	int fp = open(argv[1], O_RDWR, 0444);
	assert(fp);

	int sz = lseek(fp, 0, SEEK_END);		// seek to end of file
	lseek(fp, 0, SEEK_SET);		// back to beginning

	char* buffer = malloc(sz);	// File buffer
	pread(fp, buffer, sz, 0);
	printf("%s %d\n", argv[1], sz);

	elf_objdump(buffer);

}