AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = \
  -I$(includedir) \
  -isystem$(OFFLINE_MAIN)/include \
  -isystem$(ROOTSYS)/include

lib_LTLIBRARIES = libcalibCaloEmc_pi0.la

AM_LDFLAGS = \
  -L$(libdir) \
  -L$(OFFLINE_MAIN)/lib \
  -L$(OFFLINE_MAIN)/lib64 \
  -lCLHEP \
  -lfun4all \
  -lglobalvertex_io \
  -lcalo_io \
  -lcalotrigger_io \
  -lcalotrigger \
  -lcdbobjects \
  -lffarawobjects \
  -lSubsysReco

libcalibCaloEmc_pi0_la_SOURCES = \
  CaloCalibEmc_Pi0.cc \
  pi0EtaByEta.cc

pkginclude_HEADERS = \
  CaloCalibEmc_Pi0.h \
  pi0EtaByEta.h

BUILT_SOURCES = \
  testexternals.cc

testexternals_SOURCES = \
  testexternals.cc

noinst_PROGRAMS = \
  testexternals

testexternals_LDADD = \
  libcalibCaloEmc_pi0.la

testexternals.cc:
	echo "this is a test.  $(ROOTSYS)/include"
	echo $(ROOTSYS)
	echo "//*** this is a generated file. Do not commit, do not edit" > $@
	echo "int main()" >> $@
	echo "{" >> $@
	echo "  return 0;" >> $@
	echo "}" >> $@

clean-local:
	rm -f $(BUILT_SOURCES)
