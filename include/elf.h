/*   flash/source/elf.h
 *	ELF binary file formatting...
 *	From FreeBSD 3.1, from "sys/sys/include/elf.h"
 * $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/elf.h,v 1.2
 *2000/06/21 17:21:45 aleks Exp $
 */

/*-
 * Copyright (c) 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      -Id: elf32.h,v 1.5 1998/09/14 20:30:13 jdp Exp -
 */

#ifndef _INCL_ELF_H
#define _INCL_ELF_H

#include <sys/types.h>
#include <stdint.h>

/*
 * ELF definitions common to all 32-bit architectures.
 */

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Size;

/*
 * ELF header.
 */
#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* File identification. */

    Elf32_Half e_type;    /* File type. */
    Elf32_Half e_machine; /* Machine architecture. */
    Elf32_Word e_version; /* ELF format version. */
    Elf32_Addr e_entry;   /* Entry point. */
    Elf32_Off  e_phoff;   /* Program header file offset. */

    Elf32_Off  e_shoff;     /* Section header file offset. */
    Elf32_Word e_flags;     /* Architecture-specific flags. */
    Elf32_Half e_ehsize;    /* Size of ELF header in bytes. */
    Elf32_Half e_phentsize; /* Size of program header entry. */
    Elf32_Half e_phnum;     /* Number of program header entries. */
    Elf32_Half e_shentsize; /* Size of section header entry. */

    Elf32_Half e_shnum;    /* Number of section header entries. */
    Elf32_Half e_shstrndx; /* Section name strings section. */
} Elf32_Ehdr;
/* sizeof == 0x34 */

/* ----------------------------------------------------------------- */

/*
 *      Program header
 */

typedef struct {
    Elf32_Word p_type;   /* entry type */
    Elf32_Off  p_offset; /* file offset */
    Elf32_Addr p_vaddr;  /* virtual address */
    Elf32_Addr p_paddr;  /* physical address */

    Elf32_Word p_filesz; /* file size */
    Elf32_Word p_memsz;  /* memory size */
    Elf32_Word p_flags;  /* entry flags */
    Elf32_Word p_align;  /* memory/file alignment */
} Elf32_Phdr;
/* sizeof = 0x20 */

/* Values for p_type. */
#define PT_NULL 0    /* Unused entry. */
#define PT_LOAD 1    /* Loadable segment. */
#define PT_DYNAMIC 2 /* Dynamic linking information segment. */
#define PT_INTERP 3  /* Pathname of interpreter. */
#define PT_NOTE 4    /* Auxiliary information. */
#define PT_SHLIB 5   /* Reserved (not used). */
#define PT_PHDR 6    /* Location of program header itself. */

#define PT_COUNT 7 /* Number of defined p_type values. */

#define PT_LOPROC 0x70000000 /* First processor-specific type. */
#define PT_HIPROC 0x7fffffff /* Last processor-specific type. */

/* Values for p_flags. */
#define PF_X 0x1 /* Executable. */
#define PF_W 0x2 /* Writable. */
#define PF_R 0x4 /* Readable. */

#endif
