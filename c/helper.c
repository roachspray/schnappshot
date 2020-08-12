#define	_POSIX_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/queue.h>
#define	_GNU_SOURCE
#include <string.h>
#include <sys/uio.h>
#include <asm/unistd.h>
#include <time.h>

#include <sys/user.h>

#include "helper.h"

/* probably worth making a snap_context struct for some of the following */

extern char *outputdir;	// where snap data saved or read from

// proc maps entries and mem data list
LIST_HEAD(melist, maps_entry) snap_me = LIST_HEAD_INITIALIZER(snap_me);
struct melist *snap_mep;
unsigned num_mem_chunks;


void
print_snap_mmap()
{
	struct maps_entry *mp;

	for (mp = snap_me.lh_first; mp != NULL; mp = mp->me_entries.le_next) {
#ifdef	__x86_64__
		IPRNT("\t%llx->%llx (size: %llu bytes) %s\n", mp->me_from, mp->me_to,
#else
		IPRNT("\t%lx->%lx (size: %lu bytes) %s\n", mp->me_from, mp->me_to,
#endif
		  mp->me_to-mp->me_from, mp->me_name);
#ifdef	_NO_MUCHO_MAS_MEJOR
		IPRNT("\tdata[0:4] : 0x%x 0x%x 0x%x 0x%x\n", mp->me_data[0],
		  mp->me_data[1], mp->me_data[2], mp->me_data[3]);
#endif
	}
}


/*
 * this will read /proc/${pid}/maps which will inform a read of certain data
 * from /proc/${pid}/mem ... this results in filling out the snap_me list.
 *
 */
void
read_proc_mmap(pid_t pid)
{
	FILE *pmaps_fh = NULL;
	char pmaps_file[64], pmaps_line[256];
	char pmem_file[64];
	struct maps_entry *me;
	int fd = 0;

	LIST_INIT(&snap_me);
	num_mem_chunks = 0;

	memset(pmaps_file, 0, 64);
	sprintf(pmaps_file, "/proc/%d/maps", pid); 
	pmaps_fh = fopen(pmaps_file, "r");
	if (pmaps_fh == NULL) {
		perror("fopen: /proc/pid/maps");
		return;
	}

	memset(pmaps_line, 0, 256);
	memset(pmem_file, 0, 256);
	sprintf(pmem_file, "/proc/%d/mem", pid);
	fd = open(pmem_file, O_RDONLY);
	if (fd == -1) {
		perror("open: /proc/pid/mem");
		(void)fclose(pmaps_fh);
		return;
	}
	IPRNT("Parsing /proc/%u/maps\n", pid);
	while (fgets(pmaps_line, 256, pmaps_fh)) {
		me = calloc(1, sizeof(struct maps_entry));
		sscanf(pmaps_line, "%llx-%llx %4c %llx %x:%x %lld %255s", &me->me_from,
		  &me->me_to, me->me_flags, &me->me_offset,&me->me_major,
		  &me->me_minor, &me->me_inode, me->me_name);
		// skipping non-writable stuffs
		if (me->me_flags[1] != 'w') {
			DPRNT("Skipping (not w): %s\n", me->me_name);
			free(me);
			me = NULL;
			continue;
		}
//		IPRNT("%llx->%llx (size: %ld bytes) %s\n", me->me_from, me->me_to,
//		  me->me_to-me->me_from, me->me_name);

		IPRNT("Seeking to 0x%llx\n", me->me_from);
		if (lseek(fd, me->me_from, SEEK_SET) == -1) {
			perror("not adding region: lseek: /proc/pid/mem");
			free(me);
			me = NULL;
			continue;
		}
		me->me_data = (unsigned char *)calloc(1, me->me_to-me->me_from);	
		if (read(fd, me->me_data, me->me_to-me->me_from)  \
		  != me->me_to-me->me_from) {
			perror("fatal: read: /proc/pid/mem");
			free(me->me_data);
			me->me_data = NULL;
			free(me);
			me = NULL;
			continue;
		}
		LIST_INSERT_HEAD(&snap_me, me, me_entries);
		num_mem_chunks++;
	}
	(void)close(fd);
	(void)fclose(pmaps_fh);

	IPRNT("Read from /proc:\n");
	print_snap_mmap();

	return;
}


void
write_proc_mmap(pid_t pid)
{
	struct maps_entry *mp;
	unsigned i = 0;

	struct iovec *src = (struct iovec*)calloc(num_mem_chunks,
	  sizeof(struct iovec));
	struct iovec *dst = (struct iovec*)calloc(num_mem_chunks,
	  sizeof(struct iovec));

	for (mp = snap_me.lh_first; mp != NULL; mp = mp->me_entries.le_next,
	  i++) {
		src[i].iov_base = mp->me_data;
		src[i].iov_len = mp->me_to-mp->me_from;
		dst[i].iov_base = (void *)mp->me_from;
		dst[i].iov_len = mp->me_to-mp->me_from;
	}
	process_vm_writev(pid, src, num_mem_chunks, dst, num_mem_chunks, 0);

	IPRNT("Wrote to /proc:\n");
	print_snap_mmap();

	free(src);
	src = NULL;
	free(dst);
	dst = NULL;
}

void
write_all_disk(struct user_regs_struct *regs)
{
	struct maps_entry *mp;
	int mfd = -1;

	char *metadata_file = (char *)calloc(1, strnlen(outputdir, 64) + 32); //XXX
	sprintf(metadata_file, "%s/meta.data", outputdir);
	mfd = open(metadata_file, O_WRONLY|O_CREAT, 0644);	
	if (mfd == -1) {	
		perror("open: metadata_file");
		return;
	}
	free(metadata_file);
	metadata_file = NULL;	

	if (write(mfd, regs, sizeof(struct user_regs_struct))   \
	  != sizeof(struct user_regs_struct)) {
		perror("write: unable to write register values");
		(void)close(mfd);
		return;
	}
	if (write(mfd, &num_mem_chunks, sizeof(unsigned)) != sizeof(unsigned)) {
		perror("write: unable to write register values");
		(void)close(mfd);
		return;
	}
	for (mp = snap_me.lh_first; mp != NULL; mp = mp->me_entries.le_next) {
		int fd = -1;

		// create a random new file for each
		char *mem_file = (char *)calloc(1, strnlen(outputdir, 64) + 32); //XXX
		sprintf(mem_file, "%s/%.4d.raw", outputdir, rand());

		// len of file name
		size_t name_len = strlen(mem_file);
		write(mfd, &name_len, sizeof(size_t));
		write(mfd, mem_file, name_len);

		// memory region name
		size_t regname_len = strlen(mp->me_name);
		write(mfd, &regname_len, sizeof(size_t));
		write(mfd, &mp->me_name, regname_len);

		// we write base address and end address to metadata file
		write(mfd, &mp->me_from, sizeof(hma_t)); // XXX return value
		write(mfd, &mp->me_to, sizeof(hma_t)); // XXX return value

		// write the actual memory data to file
		fd = open(mem_file, O_WRONLY|O_CREAT, 0644); // XXX return value
		write(fd, mp->me_data, mp->me_to-mp->me_from); // XXX return value
		(void)close(fd);	

		free(mem_file);
		mem_file = NULL;
	}
	(void)close(mfd);	
	IPRNT("Wrote snap to disk:\n");
	print_snap_mmap();
}

/*
 * we read these and pop into snap_me, but this may need to change at some
 * point due to code getting sloppy/mistake ridden
 *
 */
void
read_all_disk(struct user_regs_struct *regs)
{
	int mfd = -1;
	char *metadata_file = NULL;

	metadata_file = (char *)calloc(1, strnlen(outputdir, 64) + 32); // XXX

	sprintf(metadata_file, "%s/meta.data", outputdir);
	mfd = open(metadata_file, O_RDONLY);	
	if (mfd == -1) {	
		perror("open: metadata_file");
		return;
	}
	free(metadata_file);
	metadata_file = NULL;	

	memset(regs, 0, sizeof(struct user_regs_struct));
	if (read(mfd, regs, sizeof(struct user_regs_struct))   \
    != sizeof(struct user_regs_struct)) {
		perror("read: unable to read register values");
		(void)close(mfd);
		return;
	}

	num_mem_chunks = 0;
	if (read(mfd, &num_mem_chunks, sizeof(unsigned)) != sizeof(unsigned)) {
		perror("read: unable to read number of chunks");
		(void)close(mfd);
		return;
	}
	DPRNT("Number of memory chunks saved: %u\n", num_mem_chunks);
	for (unsigned i = 0; i < num_mem_chunks; i++) {
		size_t name_len = 0;
		char *mem_file = NULL;
		int fd = 0;
	
		read(mfd, &name_len, sizeof(size_t));
		mem_file = (char *)calloc(1, name_len + 1);		
		read(mfd, mem_file, name_len);
		DPRNT("memory file named: %s\n", mem_file);
	
		struct maps_entry *me = calloc(1, sizeof(struct maps_entry));
		size_t regname_len = 0;
		read(mfd, &regname_len, sizeof(size_t));
		if (regname_len > 255) {
			//d00m
			IPRNT("bad reg name..\n");
			exit(1);
		}
		read(mfd, &me->me_name, regname_len);

		read(mfd, &me->me_from, sizeof(hma_t));
		read(mfd, &me->me_to, sizeof(hma_t));

		me->me_data = (unsigned char *)calloc(1, me->me_to-me->me_from);
		fd = open(mem_file, O_RDONLY);
		read(fd, me->me_data, me->me_to-me->me_from);
		LIST_INSERT_HEAD(&snap_me, me, me_entries);
		free(mem_file);
		mem_file = NULL;
		(void)close(fd);
	}
	(void)close(mfd);

	IPRNT("Read from disk:\n");
	print_snap_mmap();
}


#ifdef	_TEST_HELPER
char *outputdir;
int
main(int argc, char **argv)
{
	struct user_regs_struct regs;
	outputdir = argv[1];	
	memset(&regs, 0, sizeof(struct user_regs_struct));
	read_all_disk(&regs);
	
	return 0;
}
#endif

#ifdef  _WITH_XED
void
do_print_instr(pid_t pid, hma_t ip)
{
	char pmem[256];
	char disasm[BUFLEN];
	xed_uint8_t insn[IWIDTH];
	xed_uint_t isz = IWIDTH * sizeof(xed_uint8_t);
	xed_decoded_inst_t xedd;
	xed_machine_mode_enum_t mmode;
	xed_address_width_enum_t stack_addr_width;
	xed_bool_t succ;
	int fd = 0;

	memset(pmem, 0, 256);
	DPRNT(pmem, "/proc/%d/mem", pid);
	fd = open(pmem, O_RDONLY);
	if (fd == -1) {
		perror("open: proc mem do_print_instr()");
		return;
	}
	if (lseek(fd, ip, SEEK_SET) == -1) {
		perror("lseek");
		(void)close(fd);
		return;
	} 
	memset(insn, 0, isz);
	read(fd, &insn, isz);

#ifdef  __x86_64__
	mmode = XED_MACHINE_MODE_LONG_64;
	stack_addr_width = XED_ADDRESS_WIDTH_64b;
#else
	mmode = XED_MACHINE_MODE_LEGACY_32;
	stack_addr_width = XED_ADDRESS_WIDTH_32b;
#endif
	xed_decoded_inst_zero(&xedd);
	xed_decoded_inst_set_mode(&xedd, mmode, stack_addr_width);
	xed_error_enum_t xed_error = xed_decode(&xedd, insn, isz);
	if (xed_error == XED_ERROR_NONE) {
		succ = xed_format_context(XED_SYNTAX_ATT, &xedd, disasm, BUFLEN,
		  0, 0, 0);
		if (succ) {
#ifdef  __x86_64__
			IPRNT("Assembly @ 0x%llx\n\t%s\n", ip, disasm);
#else
			IPRNT("Assembly @ 0x%lx\n\t%s\n", ip, disasm);
#endif
			return;
		}
		IPRNT("%s: xed_format_context failed\n", __func__);
	} else {
#ifdef	__x86_64__
		IPRNT("%s: xed_decod failed @ 0x%llx\n", ip);
#else
		IPRNT("%s: xed_decod failed @ 0x%lx\n", ip);
#endif
	}
	return;
}
#endif

