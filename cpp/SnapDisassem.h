#ifdef  _WITH_XED
#ifndef	__SNAPDISASSEM_H

struct SnapDisassem {
// just use max ilen for x86/x86_64... i dunno how xed likes/not that
#define	IWIDTH	15	
#define BUFLEN  1024

	// basically ripped from xed example code
	static void printDecoded(pid_t pid, hma_t ip) {
		std::string pmem;
		std::ifstream pmem_fh;
		char disasm[BUFLEN];
		xed_uint8_t insn[IWIDTH];
		xed_uint_t isz = IWIDTH * sizeof(xed_uint8_t);
		xed_decoded_inst_t xedd;
		xed_machine_mode_enum_t mmode;
		xed_address_width_enum_t stack_addr_width;
		xed_bool_t succ;

		pmem = std::string("/proc/") + std::to_string(pid) + 
		  std::string("/mem");
		std::memset(insn, 0, isz);

		pmem_fh = std::ifstream(pmem, std::ios::binary);
		pmem_fh.seekg(ip, std::ios_base::beg);
		assert(pmem_fh.fail() == false && "printDecoded: seekg(ip)");
		pmem_fh.read(reinterpret_cast<char *>(&insn), isz);
		assert(pmem_fh.fail() == false && "printDecoded.read(insn)");
		pmem_fh.close();

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
			IPRNT("%s: xed_decod failed @ 0x%llx\n", __func__, ip);
#else
			IPRNT("%s: xed_decod failed @ 0x%lx\n", ip);
#endif
		}
        return;
	}
};
#endif
#endif

