dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb \
				fvp-base-gicv2-psci.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += juno.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb
dtb-$(CONFIG_ARCH_SUN50IW1P1) += sun50iw1p1-fpga.dtb  sun50iw1p1-perf1_v1_0.dtb \
				sun50iw1p1-perf2_v1_0.dtb sun50iw1p1-t1.dtb \
				sun50iw1p1-sata.dtb sun50iw1p1-soc.dtb
dtb-$(CONFIG_ARCH_SUN50IW2P1) += sun50iw2p1-fpga.dtb sun50iw2p1-perf1_v1_0.dtb \
				sun50iw2p1-perf2_v1_0.dtb sun50iw2p1-p1.dtb \
				sun50iw2p1-sata.dtb sun50iw2p1-soc.dtb
dtb-$(CONFIG_ARCH_SUN50IW3P1) += sun50iw3p1-fpga.dtb sun50iw3p1-soc.dtb \
				sun50iw3p1-perf1_v1_0.dtb sun50iw3p1-perf2_v1_0.dtb \
				sun50iw3p1-y1.dtb sun50iw3p1-t1.dtb
dtb-$(CONFIG_ARCH_SUN50IW6P1) += sun50iw6p1-fpga.dtb sun50iw6p1-soc.dtb \
				sun50iw6p1-perf1_v1_0.dtb sun50iw6p1-pro_v1_0.dtb \
				sun50iw6p1-perf2_v1_0.dtb sun50iw6p1-qc.dtb

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
