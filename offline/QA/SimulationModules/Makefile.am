AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = \
  -I$(includedir) \
  -isystem$(OFFLINE_MAIN)/include \
  -isystem`root-config --incdir`\
  -DHomogeneousField

lib_LTLIBRARIES = \
   libsimqa_modules.la \
   libsimqa_kfparticle.la

AM_LDFLAGS = \
  -L$(libdir) \
  -L$(OFFLINE_MAIN)/lib

libsimqa_kfparticle_la_LIBADD = \
  libsimqa_modules.la \
  -lkfparticle_sphenix

libsimqa_modules_la_LIBADD = \
  -lcalo_io \
  -lfun4all \
  -lg4detectors_io \
  -lg4eval \
  -ljetbase \
  -lphg4hit \
  -ltrackbase_historic_io \
  -ltpc \
  -lCLHEP \
  -ldecayfinder_io \
  -lphhepmc \
  -lqautils

pkginclude_HEADERS = \
  QAG4SimulationCalorimeter.h \
  QAG4SimulationCalorimeterSum.h \
  QAG4SimulationDistortions.h \
  QAG4SimulationIntt.h \
  QAG4SimulationJet.h \
  QAG4SimulationKFParticle.h \
  QAG4SimulationMicromegas.h \
  QAG4SimulationMvtx.h \
  QAG4SimulationTpc.h \
  QAG4SimulationTracking.h \
  QAG4SimulationVertex.h \
  QAG4SimulationUpsilon.h \
  QAG4SimulationTruthDecay.h \
  QAG4Decayer.h

libsimqa_kfparticle_la_SOURCES = \
  QAG4SimulationKFParticle.cc

libsimqa_modules_la_SOURCES = \
  QAG4SimulationCalorimeter.cc \
  QAG4SimulationCalorimeterSum.cc \
  QAG4SimulationDistortions.cc \
  QAG4SimulationIntt.cc \
  QAG4SimulationJet.cc \
  QAG4SimulationMicromegas.cc \
  QAG4SimulationMvtx.cc \
  QAG4SimulationTpc.cc \
  QAG4SimulationTracking.cc \
  QAG4SimulationVertex.cc \
  QAG4SimulationUpsilon.cc \
  QAG4SimulationTruthDecay.cc \
  QAG4Decayer.cc

################################################
# linking tests

noinst_PROGRAMS = \
  testexternals_qa \
  testexternals_simqa_kfparticle

BUILT_SOURCES = \
	testexternals.cc

testexternals_qa_SOURCES = \
	testexternals.cc

testexternals_qa_LDADD = \
	libsimqa_modules.la

testexternals_simqa_kfparticle_SOURCES = \
	testexternals.cc

testexternals_simqa_kfparticle_LDADD = \
	libsimqa_modules.la

testexternals.cc:
	echo "//*** this is a generated file. Do not commit, do not edit" > $@
	echo "int main()" >> $@
	echo "{" >> $@
	echo "  return 0;" >> $@
	echo "}" >> $@

clean-local:
	rm -f $(BUILT_SOURCES)
