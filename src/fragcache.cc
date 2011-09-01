#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <iterator>
#include <algorithm>
#include "Sugar.h"

#include "fragcache.h"

FragCache* FragCache::create(const char* dirname)
{
	const char	*f_dir;
	FragCache	*f;
	char		buf[256];

	f_dir = dirname;
	if (dirname == NULL) {
		struct stat	s;
		snprintf(buf, 256, "%s/.vexllvm/cache", getenv("HOME"));
		f_dir = buf;
		if (stat(f_dir, &s) != 0) {
			/* try to make dir */
			assert (errno == ENOENT && "What is wrong with cache?");
			snprintf(buf, 256, "%s/.vexllvm", getenv("HOME"));
			mkdir(buf, 0770);
			snprintf(buf, 256, "%s/.vexllvm/cache", getenv("HOME"));
			mkdir(buf, 0770);
		}
	}

	f = new FragCache(f_dir);
	if (!f->ok) {
		delete f;
		return NULL;
	}
	return f;
}

FragCache::FragCache(const char* in_dirname)
: dirname(in_dirname)
, ok(false)
{
	struct dirent	*de;
	DIR		*dir;
	dir = opendir(in_dirname);
	if (dir == NULL) return;

	/* XXX: Not reentrant */
	while ((de = readdir(dir)) != NULL) {
		unsigned int	i = 0;
		int		k;

		k = sscanf(de->d_name, "%d", &i);
		if (k != 1) continue;
		loadCacheFile(i);
	}
	closedir(dir);
	ok = true;
}

FragCache::~FragCache(void)
{
	foreach (it, cachetab.begin(), cachetab.end()) {
		FCFile	*fcf = *it;
		if (fcf) delete fcf;
	}
}


bool FragCache::loadCacheFile(unsigned int len)
{
	FCFile	*fcf;

	fcf = FCFile::open(getCacheFileName(len).c_str(), len);
	if (fcf == NULL)
		return false;

	if (cachetab.size() <= len) {
		cachetab.resize(len+1, NULL);
	}

	assert (cachetab[len] == NULL);
	cachetab[len] = fcf;

	return true;
}

int FragCache::findLength(const void* guest_bytes)
{
	foreach (it, cachetab.begin(), cachetab.end()) {
		FCFile*	fcf = *it;
		if (fcf == NULL)
			continue;
		if (fcf->contains(guest_bytes))
			return fcf->getFragSize();
	}
	return -1;
}

std::string FragCache::getCacheFileName(unsigned int len) const
{
	char	buf[256];
	snprintf(buf, 256, "%s/%d", dirname.c_str(), len);
	return std::string(buf);
}

bool FragCache::addFragment(const void* guest_bytes, unsigned int len)
{
	FCFile	*fcf;

	fcf = getFCFile(len);
	if (!fcf) {
		fcf = FCFile::create(getCacheFileName(len).c_str(), len);
		assert (fcf);
		delete fcf;
		loadCacheFile(len);
		fcf = getFCFile(len);
		assert (fcf);
	} else if (fcf->contains(guest_bytes))
		return false;

	fcf->add(guest_bytes);
	return true;
}

FCFile* FragCache::getFCFile(unsigned int len) const
{
	return (cachetab.size() <= len)
		? NULL
		: cachetab[len];
}

FCFile* FCFile::create(const char* in_fname, unsigned int in_len)
{
	struct stat	s;

	if (stat(in_fname, &s) == 0) {
		int	new_fd = ::open(in_fname, O_CREAT|O_RDWR, 0660);
		if (new_fd > 0) close(new_fd);
	}

	return FCFile::open(in_fname, in_len);
}


FCFile* FCFile::open(const char* in_fname, unsigned int in_len)
{
	FCFile	*fcf;

	assert (in_len > 0);

	fcf = new FCFile(in_fname, in_len);
	if (fcf->fd == -1) {
		delete fcf;
		return NULL;
	}

	return fcf;
}

FCFile::FCFile(const char* in_fname, unsigned int in_len)
: fname(in_fname)
, len(in_len)
, fd(-1)
, f_mmap(NULL)
{
	fd = ::open(fname.c_str(), O_CREAT|O_RDWR, 0660);
	if (fd == -1) return;
	mmapFd();
}

FCFile::~FCFile()
{
	if (fd != -1) {
		flush();
		sort();
		close(fd);
	}
	if (f_mmap != NULL) munmap(f_mmap, f_mmap_sz);
}

void FCFile::mmapFd(void)
{
	struct stat	s;

	f_mmap = NULL;
	if (fstat(fd, &s) == -1) {
		close(fd);
		fd = -1;
		return;
	}

	f_mmap_sz = s.st_size;
	assert ((s.st_size % len) == 0 && "Should be multiple of frag len");
	if (f_mmap_sz) {
		f_mmap = (char*)mmap(
			NULL,
			f_mmap_sz,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			0);
		assert (f_mmap != MAP_FAILED);
	}
}

void FCFile::flush(void)
{
	if (buffer.size() == 0) return;

	lseek(fd, 0, SEEK_END);
	for (unsigned int i = 0; i < buffer.size(); i++) {
		size_t	 bw;
		bw = write(fd, buffer[i], len);
		assert (bw == len);
		delete [] buffer[i];
	}
	buffer.clear();
	lseek(fd, 0, SEEK_SET);
}

bool FCFile::containsBuffer(const void* guest_bytes) const
{
	for (unsigned int i = 0; i < buffer.size(); i++) {
		if (memcmp(guest_bytes, buffer[i], len) == 0)
			return true;
	}

	return false;
}


/* I tried really hard to do this the "right" C++ way and
 * ended up in iterator hell for a few hours. I am not going to figure 
 * out that mess. */
static unsigned int fcfile_cmp_len;
static int fcfile_cmp(const void* v1, const void* v2)
{
	return memcmp(v1, v2, fcfile_cmp_len);
}

void FCFile::sort(void)
{
	if (f_mmap != NULL) munmap(f_mmap, f_mmap_sz);
	f_mmap = NULL;

	mmapFd();
	if (f_mmap == NULL)
		return;

	fcfile_cmp_len = len;
	qsort(f_mmap, f_mmap_sz/len, len, fcfile_cmp);
}

bool FCFile::containsFile(const void* guest_bytes) const
{
	bool	found;

	if (f_mmap == NULL)
		return false;

	fcfile_cmp_len = len;
	
	found = bsearch(guest_bytes, f_mmap, f_mmap_sz/len, len, fcfile_cmp);
	return found;
}

bool FCFile::contains(const void* guest_bytes) const
{
	if (containsFile(guest_bytes)) return true;
	if (containsBuffer(guest_bytes)) return true;
	return false;
}

void FCFile::add(const void* guest_bytes)
{
	char	*buffered_guest_bytes;

	assert (!contains(guest_bytes));

	buffered_guest_bytes = new char[len];
	memcpy(buffered_guest_bytes, guest_bytes, len);
	buffer.push_back(buffered_guest_bytes);
}
