#ifndef VEXLLVM_FRAGCACHE_H
#define VEXLLVM_FRAGCACHE_H

#include <vector>
#include <list>
#include <string>
#include <openssl/sha.h>
#include <stdint.h>

typedef struct fchash_rec {
	unsigned char	md[SHA_DIGEST_LENGTH];
	uint16_t	len;
} fchash_rec;

/* pending record-- integer marks return code given */
typedef std::pair<fchash_rec, int>	fchash_p_rec;
#define FCHASH_NOTFOUND		-1
#define FCHASH_COLLISION	-2
class FCHash
{
public:
	static FCHash* create(const char* fname);
	virtual ~FCHash();
	int lookup(const void* guest_bytes, unsigned int max_len=1024) const;
	void add(const void* guest_bytes, unsigned int len);
protected:
	FCHash(const char* fname);

private:
	void flush(void);
	struct fchash_rec* findHashRec(const unsigned char* md) const;
	int findHash(const unsigned char* md) const;
	int findHashInFile(const unsigned char* md) const;
	int findHashInPending(const unsigned char* md) const;
	void mmapFD(void);
	int			fd;
	unsigned int		f_mmap_sz;
	void			*f_mmap;
	std::list<fchash_p_rec>	pending;
};

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
	bool loadCacheFile(unsigned int len) const;
	void loadDir(void);

	mutable std::vector<FCFile*>	cachetab;
	std::string		dirname;
	FCHash			*fchash;
	bool			ok;
};

#endif