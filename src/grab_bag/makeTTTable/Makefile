SRCS = \
	calc_tt.f \
	ntrbl.f \
	Iasp91/libsun.f \
	Iasp91/libtau.f

calc_tt: $(SRCS)
	$(FC) -o $@ $(FFLAGS) $(SRCS)


# Clean-up rules
.PHONY: clean clean_bin

clean:
	-$(RM) a.out core *.o *.obj *% *~
	-$(RM) -r calc_tt.dSYM

clean_bin:
	-$(RM) calc_tt
