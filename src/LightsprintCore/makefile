# include project settings

include makefile.proj

# specify include and library directories for compiler

OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
CPP_FLAGS += $(addprefix -I, $(INC_DIRS))
CPP_FLAGS += $(addprefix -L, $(LIB_DIRS))

# link

$(TARGET): $(CFG_DIR)/current $(OBJECTS)
	@mkdir -p $(dir $@)
	gcc -o $@ -shared $(OBJECTS)

# include dependencies

-include $(OBJECTS:%.o=%.d)

# compile

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	gcc -o $@ -c $< $(CPP_FLAGS) -MD
	@sed 's|^.*:||;s|^ *||;s| *\\$$||;s|$$|:|' < $(OBJ_DIR)/$*.d >> $(OBJ_DIR)/$*.d

# clean object and dependency files

clean:
	@rm -f `find $(OBJ_DIR) -type f`
	@rm -rf $(OBJ_DIR)

# check whether the platform-specific configuration file exists

$(CFG_DIR)/current:
	@echo "Please use \"make <config>\" first to select configuration."
	@exit 1

# create platform-specific configuration file

ps3-linux ps3-linux-debug \
pc-linux32 pc-linux32-debug \
pc-linux64 pc-linux64-debug \
:
	@cp $(CFG_DIR)/$@ $(CFG_DIR)/current
	@echo Switched to configuration \"$@\". You may now use simple \"make\".
