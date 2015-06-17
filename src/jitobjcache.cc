#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>

#include <openssl/sha.h>

#include <iostream>
#include <sstream>

#include "jitobjcache.h"

using namespace llvm;

std::unique_ptr<JITObjectCache> JITObjectCache::create(void)
{
	SmallString<256>	dir;
	const char		*udir;

	udir = getenv("VEXLLVM_JITCACHE_PATH");
	if (udir == nullptr)
		return nullptr;

	if (udir[0] == '\0') {
		sys::fs::current_path(dir);
		sys::path::append(dir, "vexllvm_jit_cache");
	} else {
		dir = udir;
	}

	if (	!sys::fs::exists(dir.str()) &&
		sys::fs::create_directory(dir.str()))
	{
		std::cerr << "Unable to create cache directory\n";
		return nullptr;
	}

	return std::unique_ptr<JITObjectCache>(new JITObjectCache(dir.str()));
}

static const char hex[] = {"0123456789abcdef"};
std::string JITObjectCache::getModuleHash(const Module& m)
{
	std::stringstream	ss, ss2;
	unsigned char		md[SHA_DIGEST_LENGTH];
	auto			rfs(std::make_unique<llvm::raw_os_ostream>(ss));

	m.print(*rfs, nullptr);
	rfs = nullptr;

	auto s = ss.str();
	SHA((const unsigned char*)s.c_str(), s.size(), md);

	ss2 << "jit:";
	for (unsigned i = 0; i < sizeof(md); i++)
		ss2 << hex[md[i] & 0xf] << hex[(md[i] & 0xf0) >> 4];

	return ss2.str();
}

void JITObjectCache::notifyObjectCompiled(const Module *m, MemoryBufferRef obj)
{
	auto p = getCachePath(*m);
	if (p.empty() || sys::fs::exists(p)) {
		// don't clobber if it already exists...
		return;
	}

	std::error_code err;
	raw_fd_ostream ir_obj_os(p.c_str(), err, sys::fs::F_RW);
	ir_obj_os << obj.getBuffer();
}

std::string JITObjectCache::getCachePath(const Module& m) const
{
	const std::string mod_id = m.getModuleIdentifier();

	if (mod_id.compare(0, 4, "jit:") != 0) {
		return "";
	}

	SmallString<256> IRCacheFile = cache_dir;
	sys::path::append(IRCacheFile, mod_id.substr(4));
	return IRCacheFile.str();
}

std::unique_ptr<MemoryBuffer> JITObjectCache::getObject(const Module* m)
{
	auto p = getCachePath(*m);
	if (!sys::fs::exists(p)) {
		// This file isn't in our cache
		return nullptr;
	}

	// JIT will want to write into this buffer, and we don't want that
	// because the file has probably just been mmapped.  Instead make
	// a copy.  The filed-based buffer will release on out of scope.
	auto ir_obj_buf = MemoryBuffer::getFile(p.c_str(), -1, false);
	assert(ir_obj_buf);
	auto mbr = ir_obj_buf->get()->getMemBufferRef();
	return MemoryBuffer::getMemBufferCopy(mbr.getBuffer());
}
