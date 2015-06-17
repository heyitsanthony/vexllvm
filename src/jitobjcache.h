#ifndef VEXLLVM_JITOBJCACHE_H
#define VEXLLVM_JITOBJCACHE_H

#include <llvm/ExecutionEngine/ObjectCache.h>

namespace llvm {
class Module;
class MemoryBufferRef;
}

class JITObjectCache : public llvm::ObjectCache
{
public:
	virtual ~JITObjectCache() {}
	static std::unique_ptr<JITObjectCache> create(void);

	void notifyObjectCompiled(
		const llvm::Module*, llvm::MemoryBufferRef) override;
	std::unique_ptr<llvm::MemoryBuffer> getObject(
		const llvm::Module* M) override;

	std::string getCachePath(const llvm::Module& m) const;

	static std::string getModuleHash(const llvm::Module& m);

protected:
	JITObjectCache(const std::string& cd) : cache_dir(cd) {}

private:
	llvm::SmallString<256>	cache_dir;
};

#endif
