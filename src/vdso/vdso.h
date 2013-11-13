#ifndef VEXLLVM_VDSO_H
#define VEXLLVM_VDSO_H

struct vdso_ent
{
	void		*ve_f;
	unsigned long	ve_sz;
	const char	*ve_name;
};

extern vdso_ent vdso_tab[];

#endif
