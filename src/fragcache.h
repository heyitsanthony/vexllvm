#ifndef VEXLLVM_FRAGCACHE_H
#define VEXLLVM_FRAGCACHE_H

#include <vector>
#include <string>

class FCFile
{
public:
	static FCFile* create(const char* fname, unsigned int len);
	static FCFile* open(const char* fname, unsigned int len);
	bool contains(const void* guest_bytes) const;
	void add(const void* guest_bytes);
	virtual ~FCFile();
	void flush(void);
	unsigned int getFragSize() const { return len; }
protected:
	FCFile(const char* fname, unsigned int len);
private:
	bool containsFile(const void* guest_bytes) const;
	bool containsBuffer(const void* guest_bytes) const;
	void sort(void);
	void mmapFd(void);

	std::string	fname;
	unsigned int	len;

	int		fd;
	unsigned int	f_mmap_sz;
	char		*f_mmap;

	std::vector<char*>	buffer;
};

class FragCache
{
public:
	static FragCache* create(const char* dirname);
	virtual ~FragCache(void);
	bool addFragment(const void* guest_bytes, unsigned int len);
	int findLength(const void* guest_bytes);
protected:
	FragCache(const char* dirname);
private:
	FCFile* getFCFile(unsigned int len) const;
	std::string getCacheFileName(unsigned int) const;
	bool loadCacheFile(unsigned int len);

	std::vector<FCFile*>	cachetab;
	std::string		dirname;
	bool			ok;
};

#endif