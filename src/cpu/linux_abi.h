#ifndef LINUX_ABI_H
#define LINUX_ABI_H

class GuestABI;
class Guest;

class LinuxABI
{
public:
	LinuxABI() = delete;
	~LinuxABI() = delete;
	static GuestABI* create(Guest &g);
private:
	static const char*	scregs_i386[];
	static const char*	scregs_amd64[];
	static const char*	scregs_arm[];
};

#endif