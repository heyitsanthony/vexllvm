.globl vdso_tab
vdso_tab:
/* #object for arm haha */
.type vdso_tab,#object
.long	0,0,0
.Lvdso_tab_end:
.size vdso_tab,.Lvdso_tab_end-vdso_tab
