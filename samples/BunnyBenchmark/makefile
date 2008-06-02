# include project settings

include makefile.proj

OBJECTS = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(basename $(SOURCES))))

# specify include and library directories for compiler

CPP_FLAGS += $(addprefix -I, $(INC_DIRS))
CPP_FLAGS += $(addprefix -L, $(LIB_DIRS))

# link

$(TARGET): config $(OBJECTS)
	@mkdir -p $(dir $@)
	gcc -o $@ $(OBJECTS) $(LIBS)

# include dependencies

-include $(OBJECTS:%.o=%.d)

# compile

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	gcc -o $@ -c $< $(CPP_FLAGS) -MD
	@sed 's|^.*:||;s| *\\$$||;s|$$|:|' < $(OBJ_DIR)/$*.d >> $(OBJ_DIR)/$*.d

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	gcc -o $@ -c $< $(CPP_FLAGS) -MD
	@sed 's|^.*:||;s| *\\$$||;s|$$|:|' < $(OBJ_DIR)/$*.d >> $(OBJ_DIR)/$*.d

# clean object and dependency files

clean:
	@rm -f `find $(OBJ_DIR) -type f`
	@rm -rf $(OBJ_DIR)

# include common handling of platforms and configurations

include $(CFG_DIR)/makefile.cfg
