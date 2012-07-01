.PHONY: clean All

All:
	@echo ----------Building project:[ cseTest - Debug ]----------
	@"$(MAKE)" -f "cseTest.mk"
clean:
	@echo ----------Cleaning project:[ cseTest - Debug ]----------
	@"$(MAKE)" -f "cseTest.mk" clean
