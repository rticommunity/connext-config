#########################################################################
# APPLICATION SECTION
#########################################################################

bin_PROGRAMS = Hello

noinst_HEADERS = Hello.h \
		 HelloPublisher.h \
		 HelloSubscriber.h \
		 HelloWorld.h \
		 HelloWorldSupport.h \
		 HelloWorldPlugin.h

Hello_SOURCES = Hello.cxx \
		HelloPublisher.cxx \
		HelloSubscriber.cxx \
		HelloWorld.cxx \
		HelloWorldSupport.cxx \
		HelloWorldPlugin.cxx


HelloWorld.cxx: HelloWorld.idl
	${NDDSHOME}/bin/rtiddsgen -language C++ -replace HelloWorld.idl
