.PHONY: clean All

All:
	@echo ----------Building project:[ threadTest - Debug ]----------
	@"$(MAKE)" -f "threadTest.mk"
clean:
	@echo ----------Cleaning project:[ threadTest - Debug ]----------
	@"$(MAKE)" -f "threadTest.mk" clean
