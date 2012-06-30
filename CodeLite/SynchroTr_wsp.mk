.PHONY: clean All

All:
	@echo ----------Building project:[ threadTest - Debug ]----------
	@"$(MAKE)" -w -f "threadTest.mk"
clean:
	@echo ----------Cleaning project:[ threadTest - Debug ]----------
	@"$(MAKE)" -w -f "threadTest.mk" clean
