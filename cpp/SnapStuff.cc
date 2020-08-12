/*
 * if want same
 * echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
 *
 */
extern "C" {
#include <sys/uio.h>
#include <sys/user.h>
}

#include <memory>
#include <vector>

#include <iostream>
#include <fstream>
#include <sstream>

#include <chrono>
#include <random>

#include <cstring>
#include <cassert>

#include "SnapStuff.h"
#include "SnapDisassem.h"

using std::string;

unsigned LogLevel;

void
SnapContext::print()
{

	for (auto &m : this->entries) {
#ifdef	__x86_64__
		IPRNT("\t%llx->%llx (size: %llu bytes) %s\n", m->me_from,
		  m->me_to, m->me_to - m->me_from, m->me_name);
#else
		IPRNT("\t%lx->%lx (size: %lu bytes) %s\n", m->me_from,
		  m->me_to, m->me_to - m->me_from, m->me_name);
#endif

#ifdef	_NO_MUCHO_MAS_MEJOR
		IPRNT("\tdata[0:4] : 0x%x 0x%x 0x%x 0x%x\n", m->me_data[0],
		  m->me_data[1], m->me_data[2], m->me_data[3]);
#endif
	}
}


/*
 * this will read /proc/${pid}/maps which will inform a read of certain data
 * from /proc/${pid}/mem ... this results in filling out the snap_me list.
 *
 */
void
SnapContext::readProcMaps()
{
	std::string pmaps_file, pmaps_line, pmem_file;
	std::ifstream pmaps_fh, pmem_fh;
	bool save_only_writable_memory = true; // expose this somehow

	IPRNT("Parsing /proc/%u/maps\n", this->pid);

	pmaps_file = std::string("/proc/") + std::to_string(this->pid) + 
	  std::string("/maps");
	pmem_file = std::string("/proc/") + std::to_string(this->pid) +
	  std::string("/mem");

	pmaps_fh.open(pmaps_file, std::ios::in);
	pmem_fh.open(pmem_file, std::ios::in | std::ios::binary);
	while (std::getline(pmaps_fh, pmaps_line)) {
		auto me = ProcMapsEntry::createUnique(pmaps_line, pmem_fh,
		  save_only_writable_memory);
		if (me != nullptr) {
			this->entries.push_back(std::move(me));
		}
	}
	pmem_fh.close();
	pmaps_fh.close();

	IPRNT("Read from /proc:\n");
	this->print();

	return;
}


void
SnapContext::writeProcMaps()
{
	unsigned i = 0;

	assert(this->entries.size() > 0 && "writeProcMaps: entries.size == 0");
	struct iovec *src = new struct iovec[this->entries.size()];
	struct iovec *dst = new struct iovec[this->entries.size()];

	for (auto &mp : this->entries) {
		src[i].iov_base = mp->me_data;
		src[i].iov_len = mp->me_to-mp->me_from;
		dst[i].iov_base = (void *)mp->me_from;
		dst[i].iov_len = mp->me_to-mp->me_from;
		i++;
	}
	assert(process_vm_writev(this->pid, src, i, dst, i, 0) != -1 &&  
	  "process_vm_writev");

	IPRNT("Wrote to /proc:\n");
	this->print();

	delete [] src;
	src = nullptr;

	delete [] dst;
	dst = nullptr;
}

void
SnapContext::writeSnapToDisk()
{
	std::string metadata_file = this->outputdir +  std::string("/meta.data");
	std::ofstream md_h = std::ofstream(metadata_file, std::ios::binary);

	/*
	 * format:
	 *  - struct user_regs_struct  regs
	 *  - unsigned number_of_memory_chunks
	 *    For each chunk:
	 *    - size_t memfilename_len
	 *    - char memfile[]
	 *	  - hma_t from
	 *    - hma_t to 
	 *      ... then writes the memory data to a file
	 *
	 */
	md_h.write(reinterpret_cast<char *>(&this->regs),
	  sizeof(struct user_regs_struct));
	assert(md_h.bad() == false && "write(user_regs_struct)");

	unsigned b = this->entries.size();
	md_h.write(reinterpret_cast<char *>(&b), sizeof(unsigned));
	assert(md_h.bad() == false && "write(vector size)");

	for (auto &m : this->entries) {
		std::string memfile = this->outputdir + std::string("/") +
		  std::to_string(this->lcg_rng()) + std::string(".raw");

		size_t mf_len = memfile.size();
		md_h.write(reinterpret_cast<char *>(&mf_len), sizeof(size_t));
		assert(md_h.bad() == false && "write(mf_len)");
		md_h.write(memfile.c_str(), mf_len);
		assert(md_h.bad() == false && "write(memfile)");

		size_t name_len = strlen(m->me_name);
		md_h.write(reinterpret_cast<char *>(&name_len), sizeof(size_t));
		assert(md_h.bad() == false && "write(name_len)");
		md_h.write(m->me_name, name_len);
		assert(md_h.bad() == false && "write(me_name)");

		md_h.write(reinterpret_cast<char *>(&m->me_from), sizeof(hma_t));
		assert(md_h.bad() == false && "write(me_from)");
		md_h.write(reinterpret_cast<char *>(&m->me_to), sizeof(hma_t));
		assert(md_h.bad() == false && "write(me_to)");

		std::ofstream m_h(memfile, std::ios::binary);
		m_h.write(m->me_data, m->me_to-m->me_from);
		m_h.close();	
	}

	md_h.close();

	IPRNT("Wrote snap to disk:\n");
	this->print();
}

/*
 * we read these and pop into snap_me, but this may need to change at some
 * point due to code getting sloppy/mistake ridden
 *
 */
void
SnapContext::readSnapFromDisk()
{
	std::string metadata_file = this->outputdir +  std::string("/meta.data");

	std::ifstream md_h(metadata_file, std::ios::binary);

	std::memset(&this->regs, 0, sizeof(struct user_regs_struct));	
	md_h.read(reinterpret_cast<char *>(&this->regs),
	  sizeof(struct user_regs_struct));
	assert(md_h.fail() == false && "read(user_regs_struct)");

	unsigned num_mem_chunks = 0;
	md_h.read(reinterpret_cast<char *>(&num_mem_chunks), sizeof(unsigned));
	std::cout << "Number of memory chunks saved: " <<
	  num_mem_chunks << std::endl;

	for (unsigned i = 0; i < num_mem_chunks; i++) {
		auto mp = std::make_unique<ProcMapsEntry>();

		size_t mf_len = 0;
		md_h.read(reinterpret_cast<char *>(&mf_len), sizeof(size_t));
		assert(md_h.fail() == false && "read(mf_len)");

		char *mem_file = new char[mf_len+1];
		std::memset(mem_file, 0, mf_len+1);
		md_h.read(mem_file, mf_len);	
		assert(md_h.fail() == false && "read(mem_file)");
		std::cout << "Memory file named: " << mem_file << std::endl;

		size_t name_len = 0;
		md_h.read(reinterpret_cast<char *>(&name_len), sizeof(size_t));
		assert(md_h.fail() == false && "read(name)");

		char *mapsname = new char[name_len+1];
		std::memset(mapsname, 0, name_len+1);
		md_h.read(mapsname, name_len);	
		assert(md_h.fail() == false && "read(mapsname)");
		std::cout << "Section of memory name: " << mapsname<< std::endl;

		md_h.read(reinterpret_cast<char *>(&mp->me_from), sizeof(hma_t));	
		assert(md_h.fail() == false && "read(me_from from disk)");
		md_h.read(reinterpret_cast<char *>(&mp->me_to), sizeof(hma_t));	
		assert(md_h.fail() == false && "read(me_to from disk)");
		
		// xxx ... allocating for value inside
		mp->me_data = new char[mp->me_to-mp->me_from];
		std::ifstream m_h(mem_file, std::ios::binary);
		m_h.read(mp->me_data, mp->me_to-mp->me_from);	
		assert(md_h.fail() == false && "read(me_data from disk)");

		m_h.close();

		this->entries.push_back(std::move(mp));

		delete [] mem_file;
		mem_file = nullptr;
	}
	md_h.close();

	IPRNT("Read from disk:\n");
	this->print();
}
