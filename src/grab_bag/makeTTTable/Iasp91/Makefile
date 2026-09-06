SRC1 = \
	emiasp91.f \
	libtau.f \
	libsun.f \
	remodl.f

SRC2 = \
	libsun.f \
	libtau.f \
	setbrn.f

SRC3 = \
	libsun.f \
	libtau.f \
	ttimes.f

remodl: $(SRC1)
	$(FC) -o $@ $(FFLAGS) $(SRC1)

setbrn: $(SRC2)
	$(FC) -o $@ $(FFLAGS) $(SRC2)

ttimes: $(SRC3)
	$(FC) -o $@ $(FFLAGS) $(SRC3)


# Clean-up rules
.PHONY: clean clean_bin

clean:
	-$(RM) a.out core *.o *.obj *% *~
	-$(RM) -r remodl.dSYM setbrn.dSYM ttimes.dSYM

clean_bin:
	-$(RM) remodl setbrn ttimes
