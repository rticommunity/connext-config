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

Hello_SOURCES = HelloWorld.cxx \
		HelloWorldSupport.cxx \
		HelloWorldPlugin.cxx \
		HelloPublisher.cxx \
		HelloSubscriber.cxx \
		Hello.cxx


HelloWorld.cxx: HelloWorld.idl
	${NDDSHOME}/bin/rtiddsgen -language C++ -replace HelloWorld.idl
