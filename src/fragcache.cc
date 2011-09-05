/*
 * Tracks fragments using a two-phase strategy
 * 1. A hash table
 * 2. A bunch of sorted files
 *
 * Collision in the hash table marks the length as 0 => needs
 * a full search to figure out, but this should be unlikely.
 *
 * The hash table might be a problem, but I'll burn that bridge when I have
 * to cross it
 */
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
#include "Sugar.h"

#include "fragcache.h"

#define FLUSH_FCF_TRIGGER	1024
#define FLUSH_FCH_TRIGGER	1024

static int fchash_cmp(const void* v1, const void* v2)
{
	return memcmp(v1, v2, sizeof(fchash_rec));
}

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
, fchash(NULL)
, ok(false)
{
	DIR		*dir;
	dir = opendir(in_dirname);
	if (dir == NULL) return;
	closedir(dir);

	fchash = FCHash::create((dirname + "/hashtab").c_str());
	if (fchash == NULL)
		return;

	ok = true;
}

FragCache::~FragCache(void)
{
	foreach (it, cachetab.begin(), cachetab.end()) {
		FCFile	*fcf = *it;
		if (fcf) delete fcf;
	}
	if (fchash) delete fchash;
}

void FragCache::loadDir(void)
{
	struct dirent	*de;
	DIR		*dir;

	dir = opendir(dirname.c_str());
	assert (dir != NULL);

	/* XXX: Not reentrant */
	while ((de = readdir(dir)) != NULL) {
		unsigned int	i = 0;
		int		k;

		k = sscanf(de->d_name, "%d", &i);
		if (k != 1) continue;
		loadCacheFile(i);
	}
	closedir(dir);
}

bool FragCache::loadCacheFile(unsigned int len) const
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
	int	ret;

	ret = fchash->lookup(
		guest_bytes,
		cachetab.size() ? cachetab.size() : 1024);
	if (ret > 0)
		return ret;

	if (cachetab.size() == 0)
		loadDir();

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
	assert (fcf != NULL);

	if (fcf->contains(guest_bytes))
		return false;

	fchash->add(guest_bytes, len);
	fcf->add(guest_bytes);

	return true;
}

FCFile* FragCache::getFCFile(unsigned int len) const
{
	FCFile	*ret = NULL;

	if (cachetab.size() > len)
		ret = cachetab[len];

	if (ret != NULL)
		return ret;

	ret = FCFile::create(getCacheFileName(len).c_str(), len);
	assert (ret);
	delete ret;

	if (loadCacheFile(len) == false)
		return NULL;

	return cachetab[len];
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
		if (f_mmap != NULL) {
			munmap(f_mmap, f_mmap_sz);
			f_mmap = NULL;
		}
		flush();
		close(fd);
	}
	if (f_mmap != NULL) munmap(f_mmap, f_mmap_sz);
}

void FCFile::mmapFd(void)
{
	struct stat	s;

	assert (f_mmap == NULL);

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
	lseek(fd, 0, SEEK_SET);
	buffer.clear();

	sort();
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
	void	*ret;

	if (f_mmap == NULL)
		return false;

	fcfile_cmp_len = len;

	ret = bsearch(guest_bytes, f_mmap, f_mmap_sz/len, len, fcfile_cmp);
	return (ret != NULL);
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

	if (buffer.size() > FLUSH_FCF_TRIGGER)
		flush();
}

FCHash* FCHash::create(const char* fname)
{
	FCHash	*fch = new FCHash(fname);
	if (fch->fd == -1) {
		delete fch;
		return NULL;
	}
	return fch;
}

FCHash::FCHash(const char* fname)
: fd(-1)
, f_mmap_sz(0)
, f_mmap(NULL)
{
	fd = open(fname, O_RDWR | O_CREAT, 0660);
	if (fd == -1) return;
	mmapFD();
}

FCHash::~FCHash(void)
{
	if (fd == -1) return;
	flush();
	if (f_mmap != NULL) munmap(f_mmap, f_mmap_sz);
	close(fd);
}

void FCHash::flush(void)
{
	if (pending.size() == 0) return;

	std::list<fchash_p_rec>::iterator it;
	it = pending.begin();
	while (it != pending.end()) {
		std::list<fchash_p_rec>::iterator it_next;

		it_next = it;
		it_next++;

		if ((*it).second <= 0) {
			it = it_next;
			continue;
		}

		/* mark a collision in the hashtable at the entry */
		fchash_rec	*r;
		r = findHashRec((*it).first.md);
		assert (r != NULL && r->len != 0);
		r->len = 0;

		/* drop collision */
		pending.erase(it);
		it = it_next;
	}

	if (pending.size() == 0)
		return;

	lseek(fd, 0, SEEK_END);
	foreach (i, pending.begin(), pending.end()) {
		struct fchash_rec	r((*i).first);
		size_t			bw;

		bw = write(fd, &r, sizeof(fchash_rec));
		assert (bw == sizeof(fchash_rec));
	}
	lseek(fd, 0, SEEK_SET);
	pending.clear();

	/* changed the size of the file; need to reload */
	if (f_mmap != NULL) {
		munmap(f_mmap, f_mmap_sz);
		f_mmap = NULL;
	}

	mmapFD();
	assert (f_mmap != NULL);

	qsort(	f_mmap,
		f_mmap_sz/sizeof(fchash_rec),
		sizeof(fchash_rec),
		fchash_cmp);
}

void FCHash::mmapFD(void)
{
	struct stat s;
	assert (f_mmap == NULL);
	if (fstat(fd, &s) == -1) {
		close(fd);
		fd = -1;
		return;
	}

	f_mmap_sz = s.st_size;
	assert ((s.st_size % sizeof(fchash_rec)) == 0 &&
		"Should be multiple of hash record len");
	if (f_mmap_sz == 0)
		return;
	f_mmap = (char*)mmap(
		NULL,
		f_mmap_sz,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		fd,
		0);
	assert (f_mmap != MAP_FAILED);
}

int FCHash::lookup(const void* guest_bytes, unsigned int max_len) const
{
	unsigned char	digest[SHA_DIGEST_LENGTH];

	for (unsigned int i = 1; i <= max_len; i++) {
		int	ret;
		SHA1(	(const unsigned char*)guest_bytes,
			i+1,
			digest);

		ret = findHash(digest);
		if (ret > 0 || ret == FCHASH_COLLISION)
			return ret;
	}

	return -1;
}

void FCHash::add(const void* guest_bytes, unsigned int len)
{
	fchash_p_rec	rec;
	int		rc;

	rc = lookup(guest_bytes, len);
	if (rc == (int)len)
		return;

	/* don't need to do another write if there is a collision */
	if (rc == FCHASH_COLLISION)
		return;

	SHA1(	(const unsigned char*)guest_bytes,
		len,
		(unsigned char*)&rec.first.md);
	rec.second = rc;
	pending.push_back(rec);

	if (pending.size() > FLUSH_FCH_TRIGGER)
		flush();
}

int FCHash::findHash(const unsigned char* md) const
{
	int	ret;

	ret = findHashInFile(md);
	if (ret > 0 || ret == FCHASH_COLLISION)
		return ret;

	ret = findHashInPending(md);
	return ret;
}

fchash_rec* FCHash::findHashRec(const unsigned char* md) const
{
	if (f_mmap == NULL)
		return NULL;

	return (fchash_rec*)bsearch(
		md,
		f_mmap,
		f_mmap_sz/sizeof(fchash_rec),
		sizeof(fchash_rec), fchash_cmp);

}

int FCHash::findHashInFile(const unsigned char* md) const
{
	fchash_rec	*r;

	r = findHashRec(md);
	if (r == NULL)
		return FCHASH_NOTFOUND;

	if (r->len == 0)
		return FCHASH_COLLISION;

	return r->len;
}

int FCHash::findHashInPending(const unsigned char* md) const
{
	if (pending.size() == 0) return FCHASH_NOTFOUND;

	foreach (it, pending.begin(), pending.end()) {
		fchash_rec	r(it->first);
		if (memcmp(r.md, md, SHA_DIGEST_LENGTH) == 0)
			return r.len;
	}

	return FCHASH_NOTFOUND;
}
