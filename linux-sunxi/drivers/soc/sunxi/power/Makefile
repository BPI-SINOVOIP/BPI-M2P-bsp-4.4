#use "-Os" flags.
use-flags := -Os
#Don't use "-O2" flags.
not-use-flags := -O2
KBUILD_CFLAGS := $(filter-out $(not-use-flags), $(KBUILD_CFLAGS))
KBUILD_CFLAGS += $(use-flags)

subdir- += brom
obj-y	+= suspend.o
obj-y	+= scenelock/scenelock.o
obj-y	+= scenelock/userscenelock.o
obj-y	+= scenelock/extended_standby.o

$(obj)/brom.o : $(obj)/brom/resumes.code FORCE
obj-y   += brom.o

suspend-y := main.o

$(obj)/main.o               : $(obj)/suspend_tmp.o

$(obj)/suspend_tmp.o        : $(obj)/brom/resumes.code FORCE

$(obj)/brom/resumes.code: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/brom
