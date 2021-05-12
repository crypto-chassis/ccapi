CXXFLAGS += -Wall -Iinclude

NORMAL=\033[0m
YELLOW=\033[1;33m

all : include/hffix_fields.hpp doc fixprint examples

doc : doc/html/index.html

# Insert hffix.style.css into the head of only the index.html file, https://stackoverflow.com/questions/26141347/using-sed-to-insert-file-content-into-a-file-before-a-pattern
doc/html/index.html : doc/hffix.style.css include/hffix.hpp include/hffix_fields.hpp doc/Doxyfile README.md
	@echo -e "${YELLOW}*** Generating Doxygen in doc/html/ ...${NORMAL}"
	cd doc;rm -r html;doxygen Doxyfile
	cd doc;sed --in-place $$'/<\/head>/{e cat hffix.style.css\n}' html/index.html
	@echo -e "${YELLOW}*** Generated Doxygen in doc/html/${NORMAL}"

clean : clean-bin

clean-all : clean-bin
	@echo -e "${YELLOW}*** Cleaning all artifacts ...${NORMAL}"
	-rm -r doc/html
	-rm -f ctags
	@echo -e "${YELLOW}*** Cleaned all artifacts${NORMAL}"

clean-bin :
	@echo -e "${YELLOW}*** Cleaning binaries ...${NORMAL}"
	-rm -r util/bin
	-rm -r test/bin
	-rm -r test/produced
	@echo -e "${YELLOW}*** Cleaned binaries${NORMAL}"

fixspec/FIXRepository/Basic/Fields.xml:
	cd fixspec/FIXRepository; unzip -u FIXRepository_FIX.5.0SP2_EP240.zip Basic/Fields.xml

fixspec/FIXRepository/Basic/Messages.xml:
	cd fixspec/FIXRepository; unzip -u FIXRepository_FIX.5.0SP2_EP240.zip Basic/Messages.xml

# This build step requires the Haskell Tool Stack
include/hffix_fields.hpp: \
    fixspec/spec-parse-fields/src/Main.hs \
    fixspec/FIXRepository/Basic/Fields.xml \
    fixspec/FIXRepository/Basic/Messages.xml
	@echo -e "${YELLOW}*** Generating include/hffix_fields.hpp from FIX specs...${NORMAL}"
	-cd fixspec/spec-parse-fields && stack run > hffix_fields.hpp && mv hffix_fields.hpp ../../include/hffix_fields.hpp
	@echo -e "${YELLOW}*** Generated include/hffix_fields.hpp from FIX specs${NORMAL}"

fixprint : util/bin/fixprint

util/bin/fixprint : util/src/fixprint.cpp include/hffix.hpp include/hffix_fields.hpp
	@echo -e "${YELLOW}*** Building fixprint utility util/bin/fixprint ...${NORMAL}"
	mkdir -p util/bin
	$(CXX) $(CXXFLAGS) -o util/bin/fixprint util/src/fixprint.cpp
	@echo -e "${YELLOW}*** Built fixprint utility util/bin/fixprint${NORMAL}"

test/bin/writer01 : test/src/writer01.cpp include/hffix.hpp include/hffix_fields.hpp
	@echo -e "${YELLOW}*** Building test/bin/writer01 ...${NORMAL}"
	mkdir -p test/bin
	$(CXX) $(CXXFLAGS) -o test/bin/writer01 test/src/writer01.cpp
	@echo -e "${YELLOW}*** Built test/bin/writer01${NORMAL}"

test/bin/reader01 : test/src/reader01.cpp include/hffix.hpp include/hffix_fields.hpp
	@echo -e "${YELLOW}*** Building test/bin/reader01 ...${NORMAL}"
	mkdir -p test/bin
	$(CXX) $(CXXFLAGS) -o test/bin/reader01 test/src/reader01.cpp
	@echo -e "${YELLOW}*** Built test/bin/reader01${NORMAL}"

examples : test/bin/writer01 test/bin/reader01

unit_tests : test/bin/unit_tests
	@echo -e "${YELLOW}*** Running test/bin/unit_tests ...${NORMAL}"
	test/bin/unit_tests --color_output=true
	@echo -e "${YELLOW}*** Passed test/bin/unit_tests ...${NORMAL}"

test/bin/unit_tests : include/hffix.hpp include/hffix_fields.hpp test/src/unit_tests.cpp
	@echo -e "${YELLOW}*** Building test/bin/unit_tests ...${NORMAL}"
	$(CXX) $(CXXFLAGS) -o test/bin/unit_tests test/src/unit_tests.cpp
	@echo -e "${YELLOW}*** Built test/bin/unit_tests ...${NORMAL}"

ctags :
	ctags include/*

README.html : README.md
	pandoc --standalone --from markdown --to html < README.md > README.html

test : fixprint test01 test02 unit_tests

test01 : test/bin/writer01
	@echo -e "${YELLOW}*** Running $@ ...${NORMAL}"
	mkdir -p test/produced
	test/bin/writer01 > test/produced/writer01.fix
	diff test/expected/writer01.fix test/produced/writer01.fix || (echo -e "${YELLOW}*** $@ failed${NORMAL}" && exit 1)
	@echo -e "${YELLOW}*** Passed $@ ${NORMAL}"

test02 : test/bin/reader01 test/bin/writer01
	@echo -e "${YELLOW}*** Running $@ ...${NORMAL}"
	mkdir -p test/produced
	test/bin/writer01 | test/bin/reader01 > test/produced/reader01.txt
	diff test/expected/reader01.txt test/produced/reader01.txt || (echo -e "${YELLOW}*** $@ failed${NORMAL}" && exit 1)
	@echo -e "${YELLOW}*** Passed $@ ${NORMAL}"

.PHONY : help doc all clean clean-all clean-bin fixprint ctags examples test test01 test02 unit_tests
