all: 
	@if [ "`uname -s`" = "Linux" ] || [ "`uname -s`" = "Darwin" ]; then \
		make -f Makefile.linux ; \
	else \
		make -f Makefile.sunos ; \
	fi

