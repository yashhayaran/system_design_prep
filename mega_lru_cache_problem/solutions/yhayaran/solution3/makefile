CC = g++
CXXFLAGS += -Wall -Werror -Wunused-but-set-variable -std=c++17
CXXFLAGS += -I -pthread -ferror-limit=5

OBJECT_FILES = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJDIR = obj
OBJECTS = $(addprefix $(OBJDIR)/,$(OBJECT_FILES))

main: $(OBJECTS)
	$(CC) -o main $(OBJECTS)

$(OBJDIR)/%.o : %.cpp | $(OBJDIR)
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

print: *.cpp
	lpr -p $?
	touch print

clean:
	-rm -rf main $(OBJDIR)