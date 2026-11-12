#+++
#
#	FACILITY:	SKSHOW - a nano-utility to show TCP/IP socket counters
#
#	MODULE:		CMakeLists.txt
#
#	ABSTRACT:	Build of the SKSHOW utility.  The command line of the
#			utility is parsed by the CLI_ROUTINES of the StarLet
#			package, which is found where it was installed rather
#			than compiled alongside these sources; the on-line help
#			is a library of the HELP package, translated out of
#			doc/skshow.md at build time when that package is found.
#
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:	18-JUN-2026
#
#	MODIFICATION HISTORY:
#
#		 1-AUG-2026	RRL	X-01 : Reworked for the X.01-00 revision of skshow.c.
#
#		 1-AUG-2026	RRL	X-02 : The StarLet set is consumed as an installed CMake
#					package - find_package(StarLet) and StarLet::starlet -
#					instead of add_subdirectory() over a source tree.  Nothing
#					of the set is compiled here any more: the CLI parsing, the
#					logging and the FAO output all come from the package.
#
#		 1-AUG-2026	RRL	X-03 : Installation completed: the manual page and the README
#					join the image, the directories come from GNUInstallDirs so
#					that a distribution's lib/bin/man layout is honoured, and an
#					'uninstall' target takes back exactly what was installed.
#
#		28-AUG-2026	RRL	X-04 : The identification is read out of the head module rather than
#					set here a second time: the two disagreed, VERSION saying 1.0.0
#					while skshow.c said X.02-03. The form X-aa-bb[ECOcc] is enforced,
#					and __REV__ is checked to be the aa.bb.cc that follows from it.
#
#		11-SEP-2026	RRL	X-05 : Brought up to StarLet 1.6.2 and the HELP subsystem.
#					- StarLet 1.6 or later is asked for: the message catalogue
#					  ($PUTMSG_FAO with the derived count), __util$setlogfd() and
#					  the '?' wildcard of __util$pattern_match() are what the
#					  utility relies on from that delivery;
#					- the HELP package is looked for, before StarLet and optional:
#					  found, the image answers its help out of the library through
#					  HELP::help, and doc/skshow.md and doc/skshow_ru.md are
#					  translated by HELP_ADD_LIBRARY() and installed under
#					  share/help; absent, the summary built into the image answers
#					  and nothing else is lost;
#					- a handful of smoke tests under CTest;
#					- no identifier of this file carries a '$' any more, the house
#					  rule: the options are SKSHOW_M_STRICT and SKSHOW_M_SANITIZE.
#
#		12-NOV-2026	CLD	X-06 : Release packaging and the new form of the identification.
#					- __IDENT__ may be written X.aa-bb[ECOcc], a dot after the
#					  letter and two digits in every field (X.03-01ECO02), the form
#					  of all our projects from 12-NOV-2026; the older X-aa-bb[ECOc]
#					  is still taken, so X-03-00 builds as it is;
#					- CPACK makes the Debian package skshow_<aa.bb.cc>_<arch>.deb
#					  (section net, prefix of the configuration, /usr for a
#					  release), its version being the one derived from __IDENT__
#					  and checked against __REV__ above; .github/workflows/release.yml
#					  builds it for a tag equal to __IDENT__.
#
#	USAGE:
#		Build and install the StarLet package first, and the HELP package if
#		the on-line help is wanted out of a library:
#
#		    cmake -S <utility_routines> -B sl -DCMAKE_BUILD_TYPE=Release
#		    cmake --build sl
#		    cmake --install sl --prefix /usr/local
#
#		    cmake -S <help> -B hl -DCMAKE_BUILD_TYPE=Release
#		    cmake --build hl
#		    cmake --install hl --prefix /usr/local
#
#		then this utility:
#
#		    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#		    cmake --build build
#		    ctest --test-dir build
#		    cmake --install build --prefix /usr/local
#
#		or, with the Makefile generator, from the build directory:
#
#		    make
#		    make test
#		    make install                 [ DESTDIR=<staging> ]
#		    make uninstall
#
#		When a package lives somewhere CMake does not search by default, add
#		-DCMAKE_PREFIX_PATH=<prefix>.
#---

cmake_minimum_required (VERSION 3.16)

#
#	The VERSION is set from the identification of the head module below; a
#	literal here would be a second place to keep in step, and it did not keep
#	in step - it read 1.0.0 while the module read X.02-03.
#
project (SKSHOW DESCRIPTION "TCP/IP socket statistic viewer" LANGUAGES C)

#
#	The identification is written down once, in the head module, and is read
#	back from it here.  SKSHOW is a single module, so that module is the head:
#	taking the value the other way round would put it in the build files, where
#	the source that carries __IDENT__ could not see it.
#
file (STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/skshow.c SKSHOW_K_IDLINE
	REGEX "^#define[ \t]+__IDENT__")
file (STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/skshow.c SKSHOW_K_REVLINE
	REGEX "^#define[ \t]+__REV__")

if ( NOT SKSHOW_K_IDLINE MATCHES "\"([^\"]+)\"" )
	message (FATAL_ERROR "No __IDENT__ in skshow.c")
endif()

set (SKSHOW_K_IDENT "${CMAKE_MATCH_1}")

if ( NOT SKSHOW_K_REVLINE MATCHES "\"([^\"]+)\"" )
	message (FATAL_ERROR "No __REV__ in skshow.c")
endif()

set (SKSHOW_K_REV "${CMAKE_MATCH_1}")

#
#	The form is X.aa-bb, optionally followed by ECOcc - a dot after the letter
#	and exactly two digits in every field, the form of all our projects from
#	12-NOV-2026.  A module not touched since keeps the older X-aa-bb[ECOc], a
#	dash after the letter and the digits not counted, and that is taken too.
#	Both branches put the fields into the same capture groups.
#
if ( SKSHOW_K_IDENT MATCHES "^[A-Za-z]\\.([0-9][0-9])-([0-9][0-9])(ECO([0-9][0-9]))?$" )
elseif ( SKSHOW_K_IDENT MATCHES "^[A-Za-z]-([0-9]+)-([0-9]+)(ECO([0-9]+))?$" )
else()
	message (FATAL_ERROR
		"__IDENT__ of skshow.c is '${SKSHOW_K_IDENT}', "
		"which is of neither form X.aa-bb[ECOcc] nor X-aa-bb[ECOc]")
endif()

#
#	The capture groups are taken at once: any further IF() would reset the
#	CMAKE_MATCH_<n> variables before they could be read.  CMAKE_MATCH_4 is
#	empty when the identification carries no ECO.
#
set (SKSHOW_K_VMAJOR "${CMAKE_MATCH_1}")
set (SKSHOW_K_VMINOR "${CMAKE_MATCH_2}")
set (SKSHOW_K_VPATCH "${CMAKE_MATCH_4}")

if ( NOT SKSHOW_K_VPATCH )
	set (SKSHOW_K_VPATCH "0")
endif()

#
#	A leading zero is stripped rather than fed to MATH: '07' is not a decimal
#	to every version of MATH(EXPR), and a version component of '07' would be
#	refused by FIND_PACKAGE in any case.
#
foreach ( c VMAJOR VMINOR VPATCH )
	string (REGEX REPLACE "^0+([0-9])" "\\1" SKSHOW_K_${c} "${SKSHOW_K_${c}}")
endforeach()

set (SKSHOW_K_DERIVED "${SKSHOW_K_VMAJOR}.${SKSHOW_K_VMINOR}.${SKSHOW_K_VPATCH}")

#
#	__REV__ is not merely parsed but CHECKED: it is the same value in another
#	style, and a mismatch means one of the two was moved and the other was not.
#
if ( NOT SKSHOW_K_REV STREQUAL SKSHOW_K_DERIVED )
	message (FATAL_ERROR
		"__REV__ of skshow.c is '${SKSHOW_K_REV}', but __IDENT__ "
		"'${SKSHOW_K_IDENT}' yields '${SKSHOW_K_DERIVED}'")
endif()

set (PROJECT_VERSION		"${SKSHOW_K_DERIVED}")
set (PROJECT_VERSION_MAJOR	"${SKSHOW_K_VMAJOR}")
set (PROJECT_VERSION_MINOR	"${SKSHOW_K_VMINOR}")
set (PROJECT_VERSION_PATCH	"${SKSHOW_K_VPATCH}")
set (SKSHOW_VERSION		"${SKSHOW_K_DERIVED}")

message (STATUS "Product identification ${SKSHOW_K_IDENT}, revision ${SKSHOW_K_REV}")

set (CMAKE_C_STANDARD 11)
set (CMAKE_C_STANDARD_REQUIRED ON)

#
#	bin, share/man, share/doc and the rest, in whatever layout the target
#	distribution uses - a Debian derivative and a Red Hat one disagree, and
#	GNUInstallDirs knows which is which.
#
include (GNUInstallDirs)

#
#	A build with no type named is a release build.  Without this the compiler
#	is given neither optimization nor NDEBUG, and the tracing of the
#	UTILITY$ROUTINES - which NDEBUG is what silences - reaches the terminal on
#	every run.
#
if ( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
	set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	message (STATUS "No build type named, assuming ${CMAKE_BUILD_TYPE}")
endif()

option (SKSHOW_M_STRICT "Treat the compilation diagnostics as errors" OFF)

#
#	HELP - the reading side of the help subsystem is a library of its own,
#	HELP::help, so this product answers its help out of the library of the
#	product instead of leaving the reader to invoke the browser.  It is
#	optional: found, it is used; absent, a help request falls back on the
#	summary built into the image and the product loses nothing else.
#
#	It is looked for BEFORE StarLet on purpose.  HELP is built against an
#	installed StarLet and its config asks for that package with
#	FIND_DEPENDENCY, so the very copy HELP was built against is the one this
#	product then links - two revisions of it in one image being exactly what
#	a duplicate symbol at link time is made of.
#
option (SKSHOW_M_HELPAPI "Answer the help out of the help library when HELP is found" ON)

if ( SKSHOW_M_HELPAPI )
	find_package (HELP 1.0 QUIET)
endif()

if ( HELP_FOUND )
	message (STATUS "SKSHOW: the help is answered out of the library, HELP ${HELP_VERSION} from ${HELP_DIR}")
else()
	message (STATUS "SKSHOW: the help is answered by the summary built into the image")
endif()

#
#	The StarLet package - UTILITY$ROUTINES, CLI_ROUTINES, STARLET and AVPROTO.
#	The delivery 1.6 or later is required: the message catalogue is reached
#	through $PUTMSG_FAO with the count derived by the macro (V.01-21), the log
#	is pointed at the standard error by __util$setlogfd() (V.01-17), the '?'
#	of a /PMASK pattern is a wildcard of __util$pattern_match() (V.01-29, the
#	1.6.0 delivery), and CLI_ROUTINES X.00-16 makes the Unix spelling of a
#	qualifier the GNU long form, which the help and the manual say.
#
find_package (StarLet 1.6 QUIET)

if ( NOT StarLet_FOUND )
	message (FATAL_ERROR
		"The StarLet package (UTILITY\$ROUTINES) 1.6 or later was not found.\n"
		"Build and install it, then configure this utility again:\n"
		"    cmake -S <utility_routines> -B sl -DCMAKE_BUILD_TYPE=Release\n"
		"    cmake --build sl && cmake --install sl --prefix <prefix>\n"
		"    cmake -S . -B build -DCMAKE_PREFIX_PATH=<prefix>")
endif()

message (STATUS "Using StarLet ${StarLet_VERSION} from ${StarLet_DIR}")

#
#	The utility itself.  __ARCH__NAME__ names the target architecture in the
#	identification line; the set expects it to be supplied by the build.
#
add_executable (skshow skshow.c)

target_compile_definitions (skshow PRIVATE
	__ARCH__NAME__="${CMAKE_SYSTEM_PROCESSOR}")

target_compile_options (skshow PRIVATE
	-Wall -Wextra -Wno-unused-parameter)

if ( SKSHOW_M_STRICT )
	target_compile_options (skshow PRIVATE -Werror)
endif()

#
#	_GNU_SOURCE is defined by the first line of skshow.c itself; it is repeated
#	here for the headers of the package, which are included before it in some
#	translation orders.
#
target_compile_definitions (skshow PRIVATE _GNU_SOURCE)

target_link_libraries (skshow PRIVATE StarLet::starlet)

#
#	The help libraries of the product.  The description of the utility lives
#	in doc/skshow.md and doc/skshow_ru.md, in the source format of the
#	VHELPTRAN librarian, and is translated into .hlb at build time by
#	HELP_ADD_LIBRARY() of the HELP package - every product of the line states
#	it the same way.  The librarian comes as the imported executable
#	HELP::helptran, so nothing here has to know where the package was
#	installed.
#
#	The libraries go where the HELP browser looks for them: <prefix>/share/help,
#	the Russian one under ru/, so that a plain 'vhelp skshow' finds the topic
#	with nothing being set - and where the image itself looks, see
#	s_hlb_findlib() of skshow.c.
#
#	NOFACILITY: the one heading of level 1 in the source, "# SKSHOW", is the
#	facility key already.  Left to the helper, a root of the same name would
#	be put on top of it, and every topic would sit one level deeper than the
#	image expects - SKSHOW SKSHOW /PMASK rather than SKSHOW /PMASK.
#
#	The name of a macro carrying a '$' is expanded away by MAKE before the
#	compiler sees it, so the two definitions below are spelled with plain
#	identifiers - the value of the second one is a path and may carry anything.
#
option (SKSHOW_M_HELPLIB "Translate and install the help libraries" ON)

set (SKSHOW_K_HLBDIR "${CMAKE_INSTALL_DATAROOTDIR}/help"
	CACHE STRING "Directory the help libraries are installed into, relative to the prefix")

if ( HELP_FOUND )
	target_link_libraries (skshow PRIVATE HELP::help)

	target_compile_definitions (skshow PRIVATE
		SKSHOW_WITH_HLB=1
		SKSHOW_K_HLBDIR="${CMAKE_INSTALL_PREFIX}/${SKSHOW_K_HLBDIR}")
endif()

if ( SKSHOW_M_HELPLIB AND HELP_FOUND )
	message (STATUS "SKSHOW: the help libraries are translated by the HELP package")

	help_add_library (NAME skshow_help ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/skshow.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/skshow.hlb
		IDENT		${SKSHOW_K_IDENT}
		DESTINATION	${SKSHOW_K_HLBDIR})

	help_add_library (NAME skshow_help_ru ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/skshow_ru.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/skshow_ru.hlb
		IDENT		${SKSHOW_K_IDENT}RU
		DESTINATION	${SKSHOW_K_HLBDIR}/ru)
elseif ( SKSHOW_M_HELPLIB )
	message (STATUS "SKSHOW: no HELP package found, the help libraries are not built")
endif()

#
#	An optional sanitized build: $ cmake ... -DSKSHOW_M_SANITIZE=ON
#
option (SKSHOW_M_SANITIZE "Build with the Address/UB sanitizers" OFF)

if ( SKSHOW_M_SANITIZE )
	target_compile_options (skshow PRIVATE
		-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
	target_link_options (skshow PRIVATE -fsanitize=address,undefined)
endif()

#
#	Smoke tests.  None of them needs the superuser: the inventory of /proc/net
#	is readable by anybody, and /NOCOUNTERS leaves NETLINK alone.  The help
#	requests are answered out of the library when it was built beside the image
#	- s_hlb_findlib() looks there - and out of the built-in summary otherwise,
#	so the same checks hold either way; the one that names a qualifier as a
#	topic is added only when there is a library to name it in.
#
include (CTest)

if ( BUILD_TESTING )
	add_test (NAME skshow_help		COMMAND skshow help)
	add_test (NAME skshow_help_show		COMMAND skshow help show)
	add_test (NAME skshow_help_dashh	COMMAND skshow -h monitor)
	add_test (NAME skshow_help_unknown	COMMAND skshow help nosuchtopic)
	add_test (NAME skshow_show		COMMAND skshow show /nocounters)
	add_test (NAME skshow_show_pmask	COMMAND skshow show /nocounters "/pmask=(SSHD,init*)")
	add_test (NAME skshow_bad_state		COMMAND skshow show /state=nosuch)
	add_test (NAME skshow_bad_port		COMMAND skshow show /lpn=70000)
	add_test (NAME skshow_bad_token		COMMAND skshow show -lpn=80)

	set_tests_properties (skshow_help_unknown skshow_bad_state skshow_bad_port skshow_bad_token
		PROPERTIES WILL_FAIL TRUE)
	set_tests_properties (skshow_help	PROPERTIES PASS_REGULAR_EXPRESSION "SKSHOW")
	set_tests_properties (skshow_show	PROPERTIES PASS_REGULAR_EXPRESSION "record\\(s\\) displayed")
	set_tests_properties (skshow_show_pmask	PROPERTIES PASS_REGULAR_EXPRESSION "PMask: \"SSHD\", \"init\\*\"")

	if ( SKSHOW_M_HELPLIB AND HELP_FOUND )
		add_test (NAME skshow_help_qualifier	COMMAND skshow help pmask)
		add_test (NAME skshow_help_slashed	COMMAND skshow help show /pmask)
		add_test (NAME skshow_help_messages	COMMAND skshow help messages ivaddr)

		set_tests_properties (skshow_help_qualifier skshow_help_slashed
			PROPERTIES PASS_REGULAR_EXPRESSION "SKSHOW /PMASK")
		set_tests_properties (skshow_help_messages
			PROPERTIES PASS_REGULAR_EXPRESSION "IVADDR")
	endif()

	#
	#	The topics are named in English: under a Russian locale the image would
	#	open the Russian library and answer in Russian, which the checks above
	#	do not read.
	#
	get_property (SKSHOW_K_TESTS DIRECTORY PROPERTY TESTS)

	set_tests_properties (${SKSHOW_K_TESTS} PROPERTIES ENVIRONMENT "LC_ALL=C")
endif()

#
#	Installation.  DESTDIR is honoured by CMake itself, so a packager stages
#	with 'make install DESTDIR=<root>' without any help from here.
#
install (TARGETS skshow
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/skshow.8
	DESTINATION ${CMAKE_INSTALL_MANDIR}/man8)

#	CMAKE_INSTALL_DOCDIR carries the project name as CMake spells it - SKSHOW -
#	and a documentation directory is conventionally lower case.
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/skshow)

#
#	CMake generates no 'uninstall' target of its own.  The install manifest
#	names exactly what was put down, and nothing else is touched - the
#	directories are left alone, since they may hold another product's files.
#
if ( NOT TARGET uninstall )
	configure_file (
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
		@ONLY)

	add_custom_target (uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
		COMMENT "Removing the files named by install_manifest.txt")
endif()

#
#	Packaging: a Debian package of what 'make install' puts down, made by
#
#	    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
#	    cmake --build build && (cd build && cpack -G DEB)
#
#	The version is the aa.bb.cc derived from __IDENT__ above, and checked against
#	__REV__ there, so X-03-00 gives skshow_3.0.0_<arch>.deb.  The package is laid
#	out under the prefix of the configuration: the image looks for its help
#	libraries under that prefix (SKSHOW_K_HLBDIR), and a package staged anywhere
#	else would leave it to its fall-backs.  StarLet and HELP are static archives,
#	so the image needs nothing beyond the C library, which dpkg-shlibdeps names.
#
set (CPACK_PACKAGE_NAME			"skshow")
set (CPACK_PACKAGE_VENDOR		"StarLet Squad")
set (CPACK_PACKAGE_CONTACT		"StarLet Squad and Ruslan R. Laishev <polylab240@gmail.com>")
set (CPACK_PACKAGE_VERSION_MAJOR	"${SKSHOW_K_VMAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR	"${SKSHOW_K_VMINOR}")
set (CPACK_PACKAGE_VERSION_PATCH	"${SKSHOW_K_VPATCH}")
set (CPACK_PACKAGE_VERSION		"${SKSHOW_K_DERIVED}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY	"TCP/IP socket statistic viewer")
string (CONCAT CPACK_PACKAGE_DESCRIPTION
	"SKSHOW shows the TCP/IP sockets of the system with their byte and packet counters, "
	"selected by protocol, state, port, address or process, as one snapshot or refreshed "
	"until interrupted; the on-line help is answered out of a HELP library.  "
	"Identification ${SKSHOW_K_IDENT}.")
set (CPACK_RESOURCE_FILE_LICENSE	"${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set (CPACK_PACKAGING_INSTALL_PREFIX	"${CMAKE_INSTALL_PREFIX}")
set (CPACK_STRIP_FILES			ON)
set (CPACK_VERBATIM_VARIABLES		ON)
set (CPACK_GENERATOR			"DEB")

set (CPACK_DEBIAN_FILE_NAME		DEB-DEFAULT)
set (CPACK_DEBIAN_PACKAGE_SECTION	net)
set (CPACK_DEBIAN_PACKAGE_PRIORITY	optional)
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS	ON)
set (CPACK_DEBIAN_PACKAGE_HOMEPAGE	"https://github.com/SysMan-One/skshow")

include (CPack)
