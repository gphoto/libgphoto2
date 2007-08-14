# FIXME: Only run this if not cross-compiling
installcheck-local: $(INSTALL_TESTS) $(INSTALLCHECK_DEPS)
	@total=0; failed=0; success=0; ignored=0; \
	echo "Running \"installcheck\" test cases..."; \
	for script in $(INSTALL_TESTS); do \
		echo "Test case: $$script"; \
		total="$$(expr $$total + 1)"; \
		if test -f "./$$script"; then dir="./"; \
		elif test -f "$$script"; then dir=""; \
		else dir="$(srcdir)/"; fi; \
		if $(INSTALL_TESTS_ENVIRONMENT) "$$dir/$$script"; then \
			echo "SUCCEEDED: $$script"; \
			success="$$(expr $$success + 1)"; \
		else \
			s="$$?"; \
			if test "$$s" = "77"; then \
				echo "FAILURE IGNORED (return code $$s): $$script"; \
				ignored="$$(expr $$ignored + 1)"; \
			else \
				echo "FAILED (return code $$s): $$script"; \
				failed="$$(expr $$failed + 1)"; \
			fi; \
		fi; \
	done; \
	echo "Test summary: $$success succeeded, $$failed failed, $$ignored failures ignored, $$total tests in total."; \
	if test "$$failed" -gt 0; then \
		echo "Error: One or more \"installcheck\" testcases have failed."; \
		exit 1; \
	fi

