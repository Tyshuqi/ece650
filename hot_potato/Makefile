TARGETS=player ringmaster

# Default target
all: $(TARGETS)

# Target for building the player executable
player: player.cpp
	g++ -g -o $@ $<

# Target for building the ringmaster executable
ringmaster: ringmaster.cpp
	g++ -g -o $@ $<

# Clean target for removing built executables and object files
clean:
	rm -f $(TARGETS) *.o *~