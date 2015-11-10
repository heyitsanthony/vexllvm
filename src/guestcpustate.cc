#include <vector>
#include <string.h>
#include <stdio.h>

#include "guestcpustate.h"

std::map<Arch::Arch, make_guestcpustate_t> GuestCPUState::makers;

GuestCPUState::GuestCPUState()
: state_data(NULL)
, state_byte_c(0)
{}

uint8_t* GuestCPUState::copyStateData(void) const
{
	uint8_t	*ret;

	assert (state_data != NULL && "NO STATE DATA TO COPY??");
	ret = new uint8_t[getStateSize()];
	memcpy(ret, state_data, getStateSize());
	return ret;
}

unsigned GuestCPUState::getFieldsSize(const struct guest_ctx_field* f)
{
	unsigned int		cur_byte_off, total_elems;

	/* add all fields to types vector from structure */
	cur_byte_off = 0;
	total_elems = 0;
	off2ElemMap.clear();
	reg2OffMap.clear();

	for (unsigned i = 0; f[i].f_len != 0; i++) {
		reg2OffMap[f[i].f_name] = cur_byte_off;
		for (unsigned int c = 0; c < f[i].f_count; c++) {
			off2ElemMap[cur_byte_off] = total_elems;
			cur_byte_off += f[i].f_len/ 8;
			total_elems++;
		}
	}

	return cur_byte_off;
}


unsigned GuestCPUState::name2Off(const char* name) const
{
	reg2byte_map::const_iterator	it(reg2OffMap.find(name));
	assert (it != reg2OffMap.end());
	return it->second;
}

void GuestCPUState::setReg(const char* name, unsigned bits, uint64_t v, int off)
{
	reg2byte_map::const_iterator it;
	unsigned		roff;

	assert (bits == 32 || bits == 64);
	it = reg2OffMap.find(name);
	if (it == reg2OffMap.end()) {
		std::cerr << "WHAT: " << name << '\n';
		assert ("REG NOT FOUND??" && it != reg2OffMap.end());
	}

	roff = it->second;
	if (bits == 32)
		((uint32_t*)(state_data+roff))[off] = v;
	else
		((uint64_t*)(state_data+roff))[off] = v;
}

uint64_t GuestCPUState::getReg(const char* name, unsigned bits, int off)
const
{
	reg2byte_map::const_iterator it;
	unsigned		roff;

	assert (bits == 32 || bits == 64);
	it = reg2OffMap.find(name);
	if (it == reg2OffMap.end()) {
		std::cerr << "WHAT: " << name << '\n';
		assert ("REG NOT FOUND??" && it != reg2OffMap.end());
	}

	roff = it->second;
	if (bits == 32)
		return ((uint32_t*)(state_data+roff))[off];

	return ((uint64_t*)(state_data+roff))[off];
}

void GuestCPUState::print(std::ostream& os) const
{ print(os, getStateData()); }

bool GuestCPUState::load(const char* fname)
{
	unsigned int	len = getStateSize();
	uint8_t		*data = (uint8_t*)getStateData();
	size_t		br;
	FILE		*f;

	f = fopen(fname, "rb");
	assert (f != NULL && "Could not load register file");
	if (f == NULL) return false;

	br = fread(data, len, 1, f);
	fclose(f);

	assert (br == 1 && "Error reading all register bytes");
	if (br != 1) return false;

	return true;
}

bool GuestCPUState::save(const char* fname)
{
	unsigned int	len = getStateSize();
	const uint8_t	*data = (const uint8_t*)getStateData();
	size_t		br;
	FILE		*f;

	f = fopen(fname, "w");
	if (!f) return false;

	assert (f != NULL);
	br = fwrite(data, len, 1, f);
	assert (br == 1);
	fclose(f);
	if (br != 1) return false;

	return true;
}

uint8_t* GuestCPUState::copyOutStateData(void)
{
	uint8_t	*old_dat = state_data;
	state_data = copyStateData();
	return old_dat;
}

unsigned int GuestCPUState::byteOffset2ElemIdx(unsigned int off) const
{
	auto it = off2ElemMap.find(off);
	if (it == off2ElemMap.end()) {
		unsigned int	c = 0;
		auto		fields = getFields();

		fprintf(stderr, "WTF IS AT %d\n", off);
		// dumpIRSBs();
		for (int i = 0; fields[i].f_len; i++) {
			fprintf(stderr, "%s@%d\n", fields[i].f_name, c);
			c += (fields[i].f_len/8) * fields[i].f_count;
		}
		assert (0 == 1 && "Could not resolve byte offset");
	}
	return (*it).second;
}

void GuestCPUState::registerCPU(Arch::Arch a, make_guestcpustate_t f)
{
	assert (makers.count(a) == 0);
	makers[a] = f;
}

GuestCPUState* GuestCPUState::create(Arch::Arch a)
{
	assert(makers.count(a) && "unsupported guest architecture");
	return makers[a]();
}

void GuestCPUState::noteRegion(const char* name, guest_ptr addr)
{
	std::cerr	<< "Unexpected region '" << name
			<< "' at " << (void*)addr.o << '\n';
}
