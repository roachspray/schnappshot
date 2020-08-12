#ifndef	_SNAPCONTEXT_H
#define	_SNAPCONTEXT_H

#ifdef	__x86_64__
typedef	unsigned long long	hma_t;
#define	SETINT3(a)	((((a) & 0xFFFFFFFFFFFFFF00)) | 0x00000000000000CC)
#else
typedef	unsigned long	hma_t;
#define SETINT3(a)	((((a) & 0xFFFFFF00)) | 0x000000CC)
#endif

#ifdef  _WITH_XED
extern "C" {
#include <xed/xed-decoded-inst.h>
#include <xed/xed-decoded-inst-api.h>
#include <xed/xed-error-enum.h>
#include <xed/xed-init.h>
#include <xed/xed-types.h>
#include <xed/xed-interface.h>
#include <xed/xed-decode.h>
#include <xed/xed-machine-mode-enum.h>
}

void do_print_instr(pid_t pid, hma_t ip);
#else
#define	do_print_instr(a, b)	
#define	xed_tables_init()	
#endif // _WITH_XED


struct ProcMapsEntry {
	// me_ naming leftovers from C version
	hma_t me_from;
	hma_t me_to;
	char me_flags[5];
	hma_t me_offset;
	unsigned me_major;
	unsigned me_minor;
	hma_t me_inode;
	char me_name[256];
	char *me_data;

	ProcMapsEntry() : me_from(0), me_to(0), me_offset(0), me_major(0),
	  me_minor(0), me_inode(0), me_data(nullptr) {
		std::memset(&me_flags, 0, 5);
		std::memset(&me_name, 0, 256);
	}
	~ProcMapsEntry() {
		me_from = me_to = me_inode = me_offset = 0;
		me_minor = me_major = 0;
		std::memset(&me_flags, 0, 5);
		std::memset(&me_name, 0, 256);
		if (me_data != nullptr) {
			delete [] me_data;
			me_data = nullptr;
		}
	}

	bool hasWritableMem() {
		if (this->me_flags[1] != 'w') {
			return false;
		}
		return true;
	}

	static std::unique_ptr<ProcMapsEntry> createUnique(
	  std::string pmaps_line, std::ifstream &pmem_fh, bool only_writable) {

		std::unique_ptr<ProcMapsEntry> me =  \
		  std::make_unique<ProcMapsEntry>();

		// i'm not using stringstreams on this, sorry.
		std::sscanf(pmaps_line.c_str(), "%llx-%llx %4c %llx %x:%x %lld %255s",
		  &me->me_from, &me->me_to, me->me_flags, &me->me_offset,
		  &me->me_major, &me->me_minor, &me->me_inode, me->me_name);

		if (only_writable == true && me->me_flags[1] != 'w') {
			return nullptr;
		}

		pmem_fh.seekg(me->me_from, std::ios_base::beg);
		assert(pmem_fh.fail() == false && "pmem_fh.seekg()");

		me->me_data = new char[me->me_to-me->me_from];
		pmem_fh.read(me->me_data, me->me_to-me->me_from);
		assert(pmem_fh.fail() == false && "pmem_fh.read()");
		
		return me;
	}
};


class SnapContext {
	std::minstd_rand0 lcg_rng;

public:
	struct user_regs_struct regs;
	std::vector<std::unique_ptr<ProcMapsEntry>> entries;

	// not sure i want this here.
	pid_t pid;
	std::string outputdir;

	SnapContext(std::string _outdir) : outputdir(_outdir), pid(0) {
		xed_tables_init();

		std::memset(&regs, 0, sizeof(struct user_regs_struct));
		auto seed = std::chrono::system_clock::now()
		  .time_since_epoch().count();
		lcg_rng = std::minstd_rand0(seed);
		seed = 0;
	}
	void addEntry(std::unique_ptr<ProcMapsEntry> m) {
		entries.push_back(std::move(m));
	}
	void setRegs(const struct user_regs_struct *r) {
		std::memset(&regs, 0, sizeof(struct user_regs_struct));
		std::memcpy(&regs, r, sizeof(struct user_regs_struct));
	}
	void setPID(pid_t pid) { this->pid = pid; }
	pid_t getPID() { return this->pid; }
	void readProcMaps();
	void writeProcMaps();
	void readSnapFromDisk();
	void writeSnapToDisk();
	void print();
};

extern unsigned LogLevel;

#define DPRNT(...) do {	  \
	if (LogLevel >= 2) std::printf(__VA_ARGS__);  \
} while (0)

#define IPRNT(...) do {  \
	if (LogLevel >= 1) std::printf(__VA_ARGS__);   \
} while (0)

#endif 
